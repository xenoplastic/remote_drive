# 目录说明
base:基础库  
dispatch:调度服务  
doc:各个服务协议说明  
logistics:后勤服务     

***
# 编译  
go版本: >= 1.18  
``` 
1.进入dispatch目录,运行命令:go build,完成dispatch编译  
2.进入logistics目录,运行命令:go build,完成logistics编译 
```

***
# 部署  
## dispatch  
```
1.将dispatch程序复制到服务器  
2.文件"/etc/rc.local"中，在"exit 0"命令的上一行，输入命令"[dispatch全路径] &",以设置dispatch程序自启动
```       

## logistics  
1. 同dispatch服务  

## 车端更新源  
```
1.logistics程序同目录创建车端更新目录:[logistics路径]/static/package/car  
2.将车端程序包remote_drive_car.gz复制到上述创建的路径下
3.创建文本文件version,输入最新车端程序版本号,版本号同remote_drive_car.gz包中的文件VERSION内容   
```

注:旧的车端主程序启动后,通过http请求,获取后勤服务车端更新目录下version中的内容，如果车端程序发现自身执行程序所在目录VERSION文件中的版本号,和后勤服务车端更新目录下version文件中的内容不一致，旧的车端主程序就开始下载后勤服务车端更新目录下的remote_drive_car.gz程序包，并更新自身  

## 驾驶端更新源  
```
1.logistics程序同目录创建驾驶端更新目录：[logistics路径]/static/package/driver  
2.将驾驶端安装包命名为RemoteDrive_Setup.exe，复制到上述创建的路径下  
3.创建文本文件version,输入最新驾驶端程序版本号，和RemoteDrive_Setup.exe安装后目录中driver.exe文件右键"属性"下"详细信息"中“产品版本”保持一致  
```

注:旧的驾驶端程序启动后，会获取驾驶端更新目录下version中的内容，如果驾驶端发现自身执行程序的"产品版本"，和驾驶端更新目录下version文件中的内容不一致，驾驶端就开始下载驾驶端更新目录下的RemoteDrive_Setup.exe安装包，并更新自身  