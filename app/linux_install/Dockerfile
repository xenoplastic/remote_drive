FROM ubuntu

ENV DEBCONF_NOWARNINGS yes

RUN apt-get update -y&&\
    apt-get install udev -y&&\
    apt-get install vim -y

COPY * /home/remote_drive_car/

ENTRYPOINT /home/remote_drive_car/car.sh

EXPOSE 6667