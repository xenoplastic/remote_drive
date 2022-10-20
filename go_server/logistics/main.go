package main

import (
	"flag"

	"github.com/gin-gonic/gin"
)

var g_serverPort = flag.String("addr", "4451", "http service address")
var g_apiVersion = flag.String("version", "v1.0", "api version")
var g_debug = flag.String("debug", "false", "debug mode")

func main() {
	flag.Parse()

	if *g_debug == "false" {
		gin.SetMode(gin.ReleaseMode)
	} else {
		gin.SetMode(gin.DebugMode)
	}

	e := gin.Default()
	apiGroup := e.Group("/api/" + *g_apiVersion + "/logistics")
	apiGroup.Static("/static/", "./static")
	apiGroup.GET("/get_car_version", getCarVersion)
	apiGroup.GET("/get_driver_version", getDriverVersion)

	e.Run(":" + *g_serverPort)
}
