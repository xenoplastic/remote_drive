package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strings"
	"time"

	"github.com/aler9/goroslib"
	"github.com/aler9/goroslib/pkg/msgs/std_msgs"
	"github.com/gorilla/websocket"
)

const (
	g_rosNodeName = "remote_drive_car_bridge"
)

var (
	g_wsSend         chan []byte    //websocket发送数据通道
	g_wsRecv         chan []byte    //websocket读取数据通道
	g_interrupt      chan os.Signal //Ctl + C打断信号
	g_done           chan interface{}
	g_wsClientClose  chan interface{}
	g_roSClientClose chan interface{}
	g_wsClient       *websocket.Conn
	g_rosClient      *goroslib.Node
	g_subRosToAgent  *goroslib.Subscriber
	g_pubAgentToRos  *goroslib.Publisher
)

// 命令行
var wsServerPort = flag.String("wsPort", "6667", "websocket server port")
var wsServerIp = flag.String("wsIP", "127.0.0.1", "websocket server ip")
var rosPort = flag.String("rosPort", "11311", "ros port")
var rosIp = flag.String("rosIP", "127.0.0.1", "ros ip")

func wsSendPump() {
	for {
		select {
		case data := <-g_wsSend:
			g_wsClient.WriteMessage(websocket.TextMessage, data)
		case <-g_done:
			return
		}
	}
}

func wsRecvPump() {
	for {
		_, msg, err := g_wsClient.ReadMessage()
		if err != nil {
			log.Println("wsRecvPump:", err)
			g_done <- 1
			g_wsClientClose <- 1
			return
		}
		g_wsRecv <- msg
	}
}

func onRosToAgent(msg *std_msgs.String) {
	msg.Data = strings.Replace(msg.Data, " ", "", -1)
	msg.Data = strings.Replace(msg.Data, "\n", "", -1)
	g_wsSend <- []byte(msg.Data)
	//log.Println("onRosMessage:", msg.Data)
}

func createWSClient() error {
	if g_wsClient != nil {
		g_wsClient.Close()
	}

	var err error
	g_wsClient, _, err = websocket.DefaultDialer.Dial("ws://"+*wsServerIp+":"+*wsServerPort, nil)
	if err != nil {
		log.Printf("createWSClient: %#v\n", err)
		return err
	}
	log.Println("createWSClient success")
	go wsSendPump()
	go wsRecvPump()
	return nil
}

func createRosClient() error {
	if g_rosClient != nil {
		g_rosClient.Close()
	}
	var err error
	g_rosClient, err = goroslib.NewNode(goroslib.NodeConf{
		Name:          g_rosNodeName,
		MasterAddress: *rosIp + ":" + *rosPort,
	})
	if err != nil {
		log.Printf("createRosClient NewNode: %#v\n", err)
		return err
	}

	if g_subRosToAgent != nil {
		g_subRosToAgent.Close()
	}
	g_subRosToAgent, err = goroslib.NewSubscriber(goroslib.SubscriberConf{
		Node:     g_rosClient,
		Topic:    "ros_to_agent",
		Callback: onRosToAgent,
	})
	if err != nil {
		log.Printf("createRosClient NewSubscriber ros_to_agent: %#v\n", err)
		g_rosClient.Close()
		return err
	}

	if g_pubAgentToRos != nil {
		g_pubAgentToRos.Close()
	}
	g_pubAgentToRos, err = goroslib.NewPublisher(goroslib.PublisherConf{
		Node:  g_rosClient,
		Topic: "agent_to_ros",
		Msg:   &std_msgs.String{},
	})
	if err != nil {
		log.Printf("createRosClient NewPublisher: %#v\n", err)
		g_subRosToAgent.Close()
		g_rosClient.Close()
		return err
	}
	log.Println("createRosClient success")
	go checkRos()
	return nil
}

func connectWS() {
	for {
		if createWSClient() == nil {
			break
		}
		time.Sleep(5 * time.Second)
	}
}

func connectRos() {
	for {
		if createRosClient() == nil {
			break
		}
		time.Sleep(5 * time.Second)
	}
}

func checkRos() {
	for {
		_, err := g_rosClient.NodePing(g_rosNodeName)
		if err != nil {
			log.Printf("checkRos NodePing: %#v\n", err)
			g_roSClientClose <- 1
			return
		}
		time.Sleep(5 * time.Second)
	}
}

func main() {
	fmt.Println("运行-help，查看命令行用法,命令行参数指定方式,例如:./car_bridge -rosIP=127.0.0.1")
	flag.Parse()
	fmt.Println("rosip为" + *rosIp + ",ros port为" + *rosPort)
	fmt.Println("car websocket ip为" + *wsServerIp + ",car port为" + *wsServerPort)

	g_wsSend = make(chan []byte, 512)
	g_wsRecv = make(chan []byte, 512)
	g_interrupt = make(chan os.Signal)
	g_done = make(chan interface{})
	g_wsClientClose = make(chan interface{})
	g_roSClientClose = make(chan interface{})
	defer close(g_wsSend)
	defer close(g_wsRecv)
	defer close(g_interrupt)
	defer close(g_done)
	defer close(g_wsClientClose)
	defer close(g_roSClientClose)

	signal.Notify(g_interrupt, os.Interrupt)

	connectWS()

	connectRos()

	for {
		select {
		case data := <-g_wsRecv:
			msg := &std_msgs.String{
				Data: string(data),
			}
			g_pubAgentToRos.Write(msg)
			log.Printf("pubAgentToRos.Write: %#v\n", msg)
		case <-g_wsClientClose:
			connectWS()
		case <-g_roSClientClose:
			connectRos()
		case <-g_interrupt:
			g_subRosToAgent.Close()
			g_pubAgentToRos.Close()
			g_rosClient.Close()
			g_wsClient.Close()
			return
		}
	}
}
