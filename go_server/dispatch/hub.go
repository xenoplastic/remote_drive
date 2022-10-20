package main

import (
	"log"
	"net/http"
	g "remote_drive/common"
	"strconv"
	"sync"

	"github.com/gin-gonic/gin"
)

/* 摄像头名称:
 * left_camera：左摄像头
 * right_camera:右摄像头
 * front_camera:前摄像头
 * back_camera:后摄像头
 */
type CarClient struct {
	Client
	roomId   string                   //推流房间号
	driverId string                   //管理该车的驾驶员id
	Watcher  map[string]*DriverClient //观看该车的驾驶端
	cameras  map[string]bool          //key:摄像头名称，value:是否成功推流
}

type DriverClient struct {
	WatchedCars         map[string]*CarClient //被该驾驶端拉取视频的车辆
	ControlledCars      map[string]*CarClient //正在驾驶的远程车辆
	ApplyControlledCars map[string]*CarClient //请求被控制的车辆
	*Client
}

// Hub 任务处理中心
//需要锁住多个对象时，锁的顺序按照CarsMutex、DriversMutex,以防死锁
type Hub struct {
	Cars              map[string]*CarClient
	CarsMutex         sync.RWMutex
	Drivers           map[string]*DriverClient
	DriversMutex      sync.RWMutex
	broadcastToDriver chan []byte
	register          chan *Client
	unregister        chan *Client
}

func newHub() *Hub {
	return &Hub{
		broadcastToDriver: make(chan []byte),
		register:          make(chan *Client),
		unregister:        make(chan *Client),
		Cars:              make(map[string]*CarClient),
		Drivers:           make(map[string]*DriverClient),
	}
}

func (h *Hub) registCar(client *Client) {
	h.CarsMutex.Lock()
	h.Cars[client.Id] = &CarClient{
		Client:  *client,
		Watcher: make(map[string]*DriverClient),
		cameras: make(map[string]bool),
	}
	h.CarsMutex.Unlock()

	h.DriversMutex.RLock()
	defer h.DriversMutex.RUnlock()
	for _, driver := range h.Drivers {
		driver.SendMessage(
			gin.H{
				"action": g.ActCarOnline,
				"car_id": client.Id,
				"info":   client.Info,
				"state":  client.State,
			},
		)
	}
}

func (h *Hub) registDriver(client *Client) {
	h.DriversMutex.Lock()
	h.Drivers[client.Id] = &DriverClient{
		ControlledCars:      make(map[string]*CarClient),
		ApplyControlledCars: make(map[string]*CarClient),
		Client:              client,
		WatchedCars:         make(map[string]*CarClient),
	}
	h.DriversMutex.Unlock()

	go func() {
		h.CarsMutex.RLock()
		defer h.CarsMutex.RUnlock()
		for _, car := range h.Cars {
			car.SendMessage(gin.H{
				"action":    g.ActNewDriverUp,
				"driver_id": client.Id,
			})
		}
	}()
}

func (h *Hub) unregistCar(carId string) {
	h.CarsMutex.Lock()
	_, ok := h.Cars[carId]
	if ok {
		delete(h.Cars, carId)
	}
	h.CarsMutex.Unlock()

	h.DriversMutex.Lock()
	defer h.DriversMutex.Unlock()
	for _, driver := range h.Drivers {
		delete(driver.ControlledCars, carId)
		delete(driver.ApplyControlledCars, carId)
		delete(driver.WatchedCars, carId)
		driver.SendMessage(
			gin.H{
				"car_id": carId,
				"action": g.ActCarOffline,
			},
		)
	}
}

func (h *Hub) unregistDriver(id string) {
	h.CarsMutex.Lock()
	h.DriversMutex.Lock()
	defer h.DriversMutex.Unlock()
	defer h.CarsMutex.Unlock()

	driver, ok := h.Drivers[id]
	if ok {
		delete(h.Drivers, id)
	} else {
		return
	}

	//将该驾驶员驾驶的车置为非远程控制状态
	for _, car := range driver.ControlledCars {
		car.State = g.Idle
		car.driverId = ""
	}

	for _, car := range driver.WatchedCars {
		delete(car.Watcher, id)
		if len(car.Watcher) == 0 {
			car.cameras = make(map[string]bool)
			car.SendMessage(gin.H{
				"action": g.ActExitPushVideo,
			})
		}
	}

	//将该驾驶员名下的故障车辆分配给别的驾驶员
	for carId, car := range driver.ApplyControlledCars {
		_, ok := h.Cars[carId]
		if !ok {
			continue
		}
		lightDriverClient := g_hub.GetLightDriver()
		if lightDriverClient != nil {
			car.driverId = lightDriverClient.Id
			lightDriverClient.ApplyControlledCars[carId] = car
			lightDriverClient.SendMessage(gin.H{
				"car_id": carId,
				"action": g.ActApplyBeControlled,
			})
		}
	}
}

func (h *Hub) run() {
	for {
		select {
		case client := <-h.register:
			switch client.Role {
			case g.Car:
				h.registCar(client)
			case g.Driver:
				h.registDriver(client)
			}
		case client := <-h.unregister:
			id := client.Id
			switch client.Role {
			case g.Car:
				h.unregistCar(id)
			case g.Driver:
				h.unregistDriver(id)
			}
		case message := <-h.broadcastToDriver:
			h.DriversMutex.Lock()
			for id, driver := range h.Drivers {
				select {
				case driver.send <- message:
				default:
					delete(h.Drivers, id)
				}
			}
			h.DriversMutex.Unlock()
		}
	}
}

//获取请求被控制车辆最少的驾驶端
func (h *Hub) GetLightDriver() *DriverClient {
	var lightDriverClient *DriverClient
	firstSet := false
	for _, driverClient := range g_hub.Drivers {
		driverCarsNum := len(driverClient.ApplyControlledCars)
		if !firstSet {
			lightDriverClient = driverClient
		} else if len(lightDriverClient.ApplyControlledCars) > driverCarsNum {
			lightDriverClient = driverClient
		}
	}
	return lightDriverClient
}

func serveWs(hub *Hub, w http.ResponseWriter, r *http.Request) {
	id := r.FormValue("id")
	info := r.FormValue("info")
	if len(id) == 0 || len(info) == 0 {
		return
	}

	state, err := strconv.Atoi(r.FormValue("state"))
	if err != nil {
		log.Println(err)
		return
	}

	role, err := strconv.Atoi(r.FormValue("role"))
	if err != nil {
		log.Println(err)
		return
	}

	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println(err)
		return
	}

	client := &Client{
		hub:   hub,
		conn:  conn,
		send:  make(chan []byte, 1024),
		Id:    id,
		Info:  info,
		State: state,
		Role:  role,
		task:  make(chan *Task),
	}
	client.hub.register <- client

	log.Println("accountID=" + id + ", info=" + info + " connect in")

	go client.writePump()
	go client.taskPump()
	go client.readPump()
}
