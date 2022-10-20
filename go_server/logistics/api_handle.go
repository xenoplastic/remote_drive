package main

import (
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	g "remote_drive/common"

	"github.com/gin-gonic/gin"
)

func GetRunPath() string {
	file, _ := exec.LookPath(os.Args[0])
	path, _ := filepath.Abs(file)
	index := strings.LastIndex(path, string(os.PathSeparator))
	ret := path[:index]
	return ret
}

func getCarVersion(ctx *gin.Context) {
	exeDir := GetRunPath()
	content, err := ioutil.ReadFile(exeDir + "/static/package/car/version")
	if err != nil {
		log.Printf("getCarVersion ReadFile err:%#v", err)
		ctx.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrInternal,
		})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{
		"error_code": g.NoError,
		"version":    string(content),
	})
}

func getDriverVersion(ctx *gin.Context) {
	exeDir := GetRunPath()
	content, err := ioutil.ReadFile(exeDir + "/static/package/driver/version")
	if err != nil {
		log.Printf("getDriverVersion ReadFile err:%#v", err)
		ctx.JSON(http.StatusOK, gin.H{
			"error_code": g.ErrInternal,
		})
		return
	}
	ctx.JSON(http.StatusOK, gin.H{
		"error_code": g.NoError,
		"version":    string(content),
	})
}
