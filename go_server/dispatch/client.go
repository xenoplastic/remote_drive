package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"remote_drive/common"
	g "remote_drive/common"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

const (
	writeWait      = 20 * time.Second
	pongWait       = 60 * time.Second
	pingPeriod     = (pongWait * 9) / 10
	maxMessageSize = 10240
)

var (
	newline = []byte{'\n'}
	space   = []byte{' '}
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  10240,
	WriteBufferSize: 10240,
}

var (
	ErrorsChannelIsNil = errors.New("send channel is nul")
	ErrorsSendTimeout  = errors.New("send is timeout")
)

// Task 任务数据
type Task struct {
	client *Client
	data   []byte
}

// Client 管理客户端
type Client struct {
	hub   *Hub
	conn  *websocket.Conn
	send  chan []byte
	Id    string `json:"id"`
	Info  string `json:"info,omitempty"`
	State int    `json:"state,omitempty"`
	Role  int
	task  chan *Task
}

func (client *Client) SendMessage(msg gin.H) error {
	if client.send == nil {
		return ErrorsChannelIsNil
	}
	ticker := time.NewTicker(writeWait)
	defer func() {
		ticker.Stop()
	}()

	bSendData, _ := json.Marshal(msg)
	select {
	case client.send <- bSendData:
		//log.Println("SendMessage:" + string(bSendData))
		return nil
	case <-ticker.C:
		return ErrorsSendTimeout
	}
}

func (c *Client) readPump() {
	defer func() {
		c.hub.unregister <- c
		c.conn.Close()
		c.task <- &Task{client: nil}
	}()
	c.conn.SetReadLimit(maxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(pongWait))
	c.conn.SetPongHandler(func(string) error { c.conn.SetReadDeadline(time.Now().Add(pongWait)); return nil })
	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("client exit, websocket.IsUnexpectedCloseError client Info=%s error: %v\n", c.Info, err)
			} else {
				log.Printf("client exit, Info=%s error: %v\n", c.Info, err)
			}
			break
		}
		message = bytes.TrimSpace(bytes.Replace(message, newline, space, -1))
		//c.hub.broadcast <- message
		c.task <- &Task{client: c, data: message}
	}
}

func (c *Client) taskPump() {
	defer func() {
		log.Printf("client taskPump exit, Info=%s\n", c.Info)
	}()
	for {
		select {
		case task := <-c.task:
			if task.client == nil {
				return
			}
			c.handleTask(task)
		}
	}
}

func (c *Client) handleTask(task *Task) {
	jsonData := make(map[string]interface{})
	err := json.Unmarshal(task.data, &jsonData)
	if err != nil {
		fmt.Println(err.Error())
	}

	action, ok := jsonData["action"].(float64)
	if !ok {
		task.client.SendMessage(gin.H{
			"action":  g.ErrDataInvalid,
			"message": "the requested data is invalid",
		})
		return
	}

	h := c.hub
	//开始处理任务
	switch int(action) {
	case g.ActPushVideoNotice:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.CarsMutex.Lock()
			defer h.CarsMutex.Unlock()
			car, ok := h.Cars[carId]
			if ok {
				driverId, ok := jsonData["driver_id"].(string)
				if ok {
					driver, ok := car.Watcher[driverId]
					if ok {
						driver.SendMessage(jsonData)
					}
				}

				//更新该车摄像头推流状态
				successCameras, ok := jsonData["success_cameras"].([]interface{})
				if ok {
					for _, cameraName := range successCameras {
						car.cameras[cameraName.(string)] = true
					}
				}
				failCameras, ok := jsonData["fail_cameras"].([]interface{})
				if ok {
					for _, cameraName := range failCameras {
						car.cameras[cameraName.(string)] = false
					}
				}
			}
		}
	case g.ActApplyBeControlled:
	case g.ActCarOffline:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.unregistCar(carId)
		}
	case g.ActTransferData:
	case g.ActCameraExitNotice:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.CarsMutex.Lock()
			defer h.CarsMutex.Unlock()
			car, ok := h.Cars[carId]
			if ok {
				cameraName, ok := jsonData["camera"].(string)
				if ok {
					delete(car.cameras, cameraName)
				}
			}
		}
	case g.ActDriveDataToRos:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.CarsMutex.RLock()
			defer h.CarsMutex.RUnlock()
			if len(carId) > 0 {
				car, ok := h.Cars[carId]
				if ok {
					car.SendMessage(jsonData)
					rosData, ok := jsonData["data"].(map[string]interface{})
					if ok {
						typeData, _ := rosData["type"].(string)
						if typeData == "autonomous" {
							car.driverId = ""
						}
					}
				}
			} else {
				for _, car := range h.Cars {
					car.SendMessage(jsonData)
				}
			}
		}
	case g.ActRosDataToDrivers:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			rosData, ok := jsonData["data"].(map[string]interface{})
			if ok {
				state, ok := rosData["state"].(float64)
				if ok {
					h.CarsMutex.Lock()
					defer h.CarsMutex.Unlock()
					car, ok := h.Cars[carId]
					if ok {
						if car.State != int(state) {
							h.DriversMutex.RLock()
							for _, driver := range h.Drivers {
								driver.SendMessage(jsonData)
							}
							h.DriversMutex.RUnlock()
							car.State = int(state)
						} else {
							for _, driver := range car.Watcher {
								driver.SendMessage(jsonData)
							}
						}

						//如果当前车的驾驶员id为空，状态为远程驾驶，则置为自动驾驶状态
						if len(car.driverId) == 0 && car.State == common.RemoteDrive {
							car.SendMessage(gin.H{
								"action": common.ActDriveDataToRos,
								"car_id": carId,
								"data": gin.H{
									"type": "autonomous",
								},
								"global_time": time.Now().UnixMilli(),
							})
						}
					}
				}
			}
		}

	case g.ActRosDataToDriver:
		h.DriversMutex.RLock()
		defer h.DriversMutex.RUnlock()
		driverId, ok := jsonData["driver_id"].(string)
		if ok {
			driver, ok := h.Drivers[driverId]
			if ok {
				driver.SendMessage(jsonData)
			}
		}
	case g.ActCarLocalDataUp:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.CarsMutex.RLock()
			defer h.CarsMutex.RUnlock()
			car, ok := h.Cars[carId]
			if ok {
				for _, driver := range car.Watcher {
					driver.SendMessage(jsonData)
				}
			}
		}
	case g.ActApplyControlCar:
		c.OnApplControlCar(jsonData)
	case g.ActApplyControlResult:
		c.OnApplControlCarResult(jsonData)
	case g.ActExitPushVideo:
		carId, ok := jsonData["car_id"].(string)
		if ok {
			h.CarsMutex.Lock()
			car, ok := h.Cars[carId]
			if ok {
				delete(car.Watcher, task.client.Id)
				if len(car.Watcher) == 0 {
					car.cameras = make(map[string]bool)
					car.SendMessage(jsonData)
				}
				if car.driverId == task.client.Id {
					car.driverId = ""
				}
			}
			h.CarsMutex.Unlock()

			h.DriversMutex.Lock()
			driver, ok := h.Drivers[task.client.Id]
			if ok {
				delete(driver.WatchedCars, carId)
				delete(driver.ControlledCars, carId)
			}
			h.DriversMutex.Unlock()
		}
	case g.ActCarUpdateFail, g.ActCarUpdateProcess:
		h.DriversMutex.RLock()
		for _, driver := range h.Drivers {
			driver.SendMessage(jsonData)
		}
		h.DriversMutex.RUnlock()
	}
}

func (c *Client) OnApplControlCar(jsonData map[string]interface{}) {
	carId, ok := jsonData["car_id"].(string)
	if !ok {
		return
	}
	driverId, ok := jsonData["driver_id"].(string)
	if !ok {
		return
	}

	h := c.hub
	h.CarsMutex.Lock()
	h.DriversMutex.Lock()
	defer h.DriversMutex.Unlock()
	defer h.CarsMutex.Unlock()

	driver, ok := h.Drivers[driverId]
	if !ok {
		return
	}

	car, ok := h.Cars[carId]
	if !ok {
		driver.SendMessage(gin.H{
			"action":     g.ActApplyControlResult,
			"car_id":     carId,
			"driver_id":  driverId,
			"error_code": g.ErrCarOffLine,
		})
		return
	}

	if car.driverId == driverId {
		driver.ControlledCars[carId] = car
		driver.SendMessage(gin.H{
			"action":     g.ActApplyControlResult,
			"car_id":     carId,
			"driver_id":  driverId,
			"error_code": g.NoError,
		})
	} else {
		driverOfCar, ok := h.Drivers[car.driverId]
		if ok {
			driverOfCar.SendMessage(jsonData)
		} else {
			car.driverId = driverId
			driver.ControlledCars[carId] = car
			driver.SendMessage(gin.H{
				"action":     g.ActApplyControlResult,
				"car_id":     carId,
				"driver_id":  driverId,
				"error_code": g.NoError,
			})
		}
	}
}

func (c *Client) OnApplControlCarResult(jsonData map[string]interface{}) {
	h := c.hub
	errorCode, _ := jsonData["error_code"].(float64)
	switch int(errorCode) {
	case g.ErrDriverNoControlCar:
		c.OnApplControlCar(gin.H{
			"action":    g.ActApplyControlCar,
			"car_id":    jsonData["car_id"],
			"driver_id": jsonData["driver_id"],
		})
	case g.NoError:
		carId, _ := jsonData["car_id"].(string)
		driverId, _ := jsonData["driver_id"].(string)
		h.CarsMutex.Lock()
		h.DriversMutex.Lock()
		defer h.DriversMutex.Unlock()
		defer h.CarsMutex.Unlock()
		car, okCar := h.Cars[carId]
		driver, okDriver := h.Drivers[driverId]
		if okCar {
			if okDriver {
				car.driverId = driverId
				driver.ControlledCars[carId] = car
				driver.SendMessage(jsonData)
			} else {
				car.driverId = ""
			}
		} else {
			if okDriver {
				driver.SendMessage(gin.H{
					"action":     g.ActApplyControlResult,
					"car_id":     carId,
					"driver_id":  driverId,
					"error_code": g.ErrCarOffLine,
				})
			}
		}
	case g.ErrRefuseApplyingControl:
		driverId, _ := jsonData["driver_id"].(string)
		h.DriversMutex.RLock()
		defer h.DriversMutex.RUnlock()
		driver, ok := h.Drivers[driverId]
		if ok {
			driver.SendMessage(jsonData)
		}
	}
}

func (c *Client) writePump() {
	ticker := time.NewTicker(pingPeriod)
	defer func() {
		ticker.Stop()
		c.conn.Close()
	}()
	for {
		select {
		case message, ok := <-c.send:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				log.Printf("client %s channel read error", c.Info)
				return
			}

			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				log.Printf("client %s NextWriter error", c.Info)
				return
			}
			defer func() {
				w.Close()
			}()
			_, err = w.Write(message)
			if err != nil {
				log.Printf("client %s write error, message=%s, err=%#v", c.Info, message, err)
				return
			}

			n := len(c.send)
			for i := 0; i < n; i++ {
				w.Write(newline)
				data := <-c.send
				_, err = w.Write(data)
				if err != nil {
					log.Printf("client %s for write error, data=%s , err=%#v", c.Info, data, err)
					return
				}
			}

			if err := w.Close(); err != nil {
				log.Printf("client %s Close error", c.Info)
				return
			}
		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(writeWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				log.Printf("client %s WriteMessage error", c.Info)
				return
			}
		}
	}
}
