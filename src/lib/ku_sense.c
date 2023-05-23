#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1

#define SENSOR_IOCTL_NUM 'z'
#define SENSOR_IOCTL_READ _IOWR(SENSOR_IOCTL_NUM, IOCTL_NUM1, unsigned long*)


int dev;
int buf;

int ku_sensor_dev(){
	return dev;
}

int ku_sensor_get(){

	dev = open("/dev/sensor_module",O_RDWR);

	buf = 0;
	
	ioctl(dev,SENSOR_IOCTL_READ,&buf);

	printf("sensor get: %d\n",buf);

	close(dev);
	
	return buf;
} 
