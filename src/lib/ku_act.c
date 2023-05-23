#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM2 IOCTL_START_NUM+2

#define ACT_IOCTL_NUM 'z'
#define ACT_IOCTL_WRITE _IOWR(ACT_IOCTL_NUM, IOCTL_NUM2, unsigned long)


int act_dev;
int buf;

int ku_act_open(){
	act_dev = open("/dev/act_module", O_RDWR);

	return act_dev;
}

int ku_act_send(int distance){
	
	int result = ioctl(act_dev, ACT_IOCTL_WRITE, distance);
	printf("act send\n");
	return result;
} 

int ku_act_close(){

	printf("act close\n");

	close(act_dev);

	return 1;
}
