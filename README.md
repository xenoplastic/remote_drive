# 模块说明  
主要分为4个模块:驾驶端、车端、调度服务和RTSP服务  
* 驾驶端(app/driver):驾驶端主程序  
* 车端(app/car):车端主程序,连接调度服务,完成车端视频推流,最终响应驾驶端对车端的控制          
* 车端桥接程序(app/car_bridge):用于车端主程序和ros系统之间数据的中转(不用python和c++的原因:go不需要安装ros环境,就可以编译通过)
* 调度服务(go_server/dispatch):主要的服务端程序,中转车端与驾驶端数据        
* RTSP服务(third_party/EasyDarwin-linux-8.1.0-1901141151):RTSP流媒体服务器   
* 后勤服务(go_server/logistics):负责客户端、图片等资源的下载，后期可以实现某些信息的查询(比如所有服务的地址，以取消客户端的配置)  
注:模块名后括号中为路径    
# 目录说明  
app:所有客户端程序   
doc:公共帮助文档  
go_server:go服务  
third_party:第三方库  
# 编译、打包和部署
[驾驶端](./app/README.md) | [车端](./app/README.md) | [车端桥接程序](./app/README.md) | [调度服务](./go_server/README.md) |  [后勤服务](./go_server/README.md)
# 远程驾驶系统部署  
* 后端服务部署
```
1.服务器上运行调度服务dispatch    
2.服务器上运行后勤服务logistics 
3.服务器上运行RTSP服务(通过EasyDarwin目录中start.sh脚本，启动RTSP服务)
```
* 车端部署(Linux)
```
1.车端宿主机上运行car_bridge   
2.车端docker中启动车端主程序car
3.设置车端主程序car配置文件ConfigCar.ini(参看该文档中注释)  
```
注:车端主程序详细部署查看[车端主程序部署](./app/README.md)
* 驾驶端部署(Windows)
```
1.通过驾驶端安装包安装远程驾驶2.0
```
注：需要提前安装[罗技方向盘驱动](https://download01.logi.com/web/ftp/pub/techsupport/gaming/lghub_installer.exe)