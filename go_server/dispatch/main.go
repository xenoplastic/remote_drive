package main

import (
	"flag"
	"net/http"

	"github.com/gin-gonic/gin"
)

var g_serverPort = flag.String("addr", "4450", "http service address")
var g_apiVersion = flag.String("version", "v1.0", "api version")
var g_debug = flag.String("debug", "false", "debug mode")
var g_hub *Hub

func main() {
	flag.Parse()
	g_hub = newHub()

	go g_hub.run()

	if *g_debug == "false" {
		gin.SetMode(gin.ReleaseMode)
	} else {
		gin.SetMode(gin.DebugMode)
	}

	e := gin.Default()
	e.GET("/ws", func(c *gin.Context) {
		serveWs(g_hub, c.Writer, c.Request)
	})
	e.GET("/", func(c *gin.Context) {
		//用于测试websocket
		http.ServeFile(c.Writer, c.Request, "./static/index.html")
	})
	e.Static("/static/", "./static")

	apiGroup := e.Group("/api/" + *g_apiVersion + "/dispatch")
	apiGroup.GET("/get_cars_info", getCarsInfo)
	apiGroup.POST("/car_breakdown", carBreakdown)
	apiGroup.POST("/pull_video", pullVideo)
	apiGroup.GET("/get_time", getTime)

	e.Run(":" + *g_serverPort)
}
