#!/bin/sh

./build_car_installer.sh

image_name="remote_drive:latest"

docker rmi $image_name

docker build -f ./Dockerfile -t $image_name ./remote_drive_car

image_file="car_docker_image.tar"

rm $image_file

docker save -o $image_file $image_name

rm -rf remote_drive_car remote_drive_car.gz

docker rmi $image_name