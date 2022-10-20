package main

import (
	"net/http"
	g "remote_drive/common"

	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

func getCarsInfo(c *gin.Context) {
	cars := []Client{}
	g_hub.CarsMutex.RLock()
	for _, car := range g_hub.Cars {
		cars = append(cars, car.Client)
	}
	g_hub.CarsMutex.RUnlock()
	c.JSON(http.StatusOK, gin.H{
		"error_code": g.NoError,
		"cars":       cars,
	})
}

func carBreakdown(c *gin.Context) {
	carId, ok := c.GetQuery("id")
	if !ok {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrDataInvalid,
		})
		return
	}

	g_hub.CarsMutex.Lock()
	g_hub.DriversMutex.Lock()
	defer g_hub.DriversMutex.Unlock()
	defer g_hub.CarsMutex.Unlock()
	car, ok := g_hub.Cars[carId]
	if !ok {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrCarOffLine,
		})
		return
	}

	lightDriverClient := g_hub.GetLightDriver()
	if lightDriverClient != nil {
		lightDriverClient.ApplyControlledCars[carId] = car
		car.driverId = lightDriverClient.Id
	} else {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrNoDriver,
		})
		return
	}

	lightDriverClient.SendMessage(gin.H{
		"message_id": uuid.New().String(),
		"car_id":     carId,
		"action":     g.ActApplyBeControlled,
	})
}

func pullVideo(c *gin.Context) {
	data := make(map[string]interface{})
	c.ShouldBind(&data)

	carId, okCarId := data["car_id"].(string)
	driverId, okDriverId := data["driver_id"].(string)
	interCameraNames, okCameras := data["cameras"].([]interface{})
	if !okCarId || !okDriverId || !okCameras || len(interCameraNames) == 0 {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrDataInvalid,
		})
		return
	}

	var cameraNames []string
	for _, interCameraName := range interCameraNames {
		cameraNames = append(cameraNames, interCameraName.(string))
	}

	g_hub.CarsMutex.Lock()
	defer g_hub.CarsMutex.Unlock()

	car, ok := g_hub.Cars[carId]
	if !ok {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrCarOffLine,
		})
		return
	}

	//查看车是否已被分配房间号
	//将该房间号发往驾驶端
	if len(car.Watcher) == 0 || len(car.roomId) == 0 {
		car.roomId = uuid.New().String()
	}

	g_hub.DriversMutex.Lock()
	driver, ok := g_hub.Drivers[driverId]
	if !ok {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrDriverIdInvalid,
		})
		g_hub.DriversMutex.Unlock()
		return
	}
	car.Watcher[driverId] = driver
	driver.WatchedCars[carId] = car
	g_hub.DriversMutex.Unlock()

	//查看请求推流的摄像头是否在摄像头集合中，检测当前推流状态(成功或者失败)
	//如果在则告知驾驶端该摄像头推流状态，将未存在于车端摄像头数组中的摄像头发往车端
	//var cameraToServers []string
	//var successCameras []string
	//var failCameras []string
	// if len(car.cameras) > 0 {
	// 	for _, camName := range cameraNames {
	// 		camRes, ok := car.cameras[camName]
	// 		if ok {
	// 			if camRes {
	// 				successCameras = append(successCameras, camName)
	// 			} else {
	// 				//failCameras = append(failCameras, camName)
	// 				cameraToServers = append(cameraToServers, camName)
	// 			}
	// 		} else {
	// 			cameraToServers = append(cameraToServers, camName)
	// 		}
	// 	}
	// } else {
	// 	cameraToServers = cameraNames
	// }
	//cameraToServers = cameraNames

	//将未有推流状态的摄像头告诉车端推流
	if len(cameraNames) > 0 {
		car.SendMessage(gin.H{
			"action":    g.ActPushVideo,
			"room_id":   car.roomId,
			"driver_id": driverId,
			"cameras":   cameraNames,
		})
	}

	c.JSON(http.StatusOK, gin.H{
		"error_code": g.NoError,
		//"success_camera": successCameras,
		//"fail_camera":    failCameras,
		"room_id": car.roomId,
	})
}

func getTime(c *gin.Context) {
	requestTime, ok := c.GetQuery("request_time")
	if !ok {
		c.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrDriverIdInvalid,
		})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"error_code":   g.NoError,
		"request_time": requestTime,
		"server_time":  time.Now().UnixMilli(),
	})
}
