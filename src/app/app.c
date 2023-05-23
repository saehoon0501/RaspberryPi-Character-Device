#include "ku_sense.c"
#include "ku_act.c"
#include <unistd.h>

int main(void){

	int distance;

	long i;

	ku_act_open();
	while(i<100000000){
		distance = ku_sensor_get();
		ku_act_send(distance);
		usleep(1000000);
		i++;
	}
	ku_act_close();
	return 0;
}