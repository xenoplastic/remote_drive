#!/bin/sh 
#发布的程序名称 
exe="car" 
#执行程序所在目录
exeDir=../out/x64/Release 
exePath=$exeDir/$exe
deplist=$(ldd $exePath | awk  '{if (match($3,"/")){ printf("%s "),$3 } }') 
#输出文件夹名
desName=remote_drive_car 
#输出路径
des="./$desName" 
rm -rf $des
mkdir $des 
cp $deplist $des  
cp $exePath $des 
cp ./car.sh $des 
cp $exeDir/* $des
rm -rf $desName.gz
tar -zcvf $desName.gz $des
