#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/delay.h>

MODULE_LICENSE("GPL");

#define SWITCH 12

#define PIN1 6
#define PIN2 13
#define PIN3 19
#define PIN4 26

#define STEPS 8
#define ONEROUND 128

#define DEV_NAME "act_module"


#define IOCTL_START_NUM 0x80
#define IOCTL_NUM2 IOCTL_START_NUM+2

#define ACT_IOCTL_NUM 'z'
#define ACT_IOCTL_WRITE _IOWR(ACT_IOCTL_NUM, IOCTL_NUM2, unsigned long)

static int irq_num;
static int state=0;

static irqreturn_t switch_irq_isr(int irq, void* dev_id){
		printk("switch pushed\n");
		state = 0;
		return IRQ_HANDLED;
}


int blue[8] = {1,1,0,0,0,0,0,1};
int pink[8] = {0,1,1,1,0,0,0,0};
int yellow[8] = {0,0,0,1,1,1,0,0};
int orange[8] = {0,0,0,0,0,1,1,1};

void setstep(int p1, int p2, int p3, int p4){

	gpio_set_value(PIN1, p1);
	gpio_set_value(PIN2, p2);
	gpio_set_value(PIN3, p3);
	gpio_set_value(PIN4, p4);

}

void forward (int round, int delay){
	int i = 0, j = 0;

	for(i = 0; i < ONEROUND * round; i++){
		for(j = 0; j<STEPS; j++){
			if(state == 1){
				setstep(blue[j], pink[j], yellow[j], orange[j]);
				udelay(delay);
			}
		}
	}
	setstep(0,0,0,0);
}

static long act_module_ioctl(struct file *file, unsigned int cmd, unsigned long arg){

	switch(cmd){
		case ACT_IOCTL_WRITE:
				if( (arg>0)&&(arg<11)){
					state = 1;
					forward(1,1200*arg);
				}
			break;
		default:
			break;
	}
	return 0;
}

static int act_module_open(struct inode *inode, struct file *file){
	printk("actuator module opened\n");
	return 0;
}

static int act_module_release(struct inode *inode, struct file *file){
	return 0;
}

struct file_operations act_module_fops = {
	.unlocked_ioctl=act_module_ioctl,
	.open=act_module_open,
	.release=act_module_release
};

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init actuator_motor_init(void){
	int ret;

	gpio_request_one(SWITCH, GPIOF_IN, "SWITCH");

	irq_num = gpio_to_irq(SWITCH);
	ret = request_irq(irq_num, switch_irq_isr, IRQF_TRIGGER_RISING,"sensor_irq",NULL);
	
	if(ret){
		printk("Unable to reset IRQ: %d\n",irq_num);
		free_irq(irq_num,NULL);
	}else{
		printk("switch_irq: Enable to set request IRQ: %d\n", irq_num);
	}

	gpio_request_one(PIN1, GPIOF_OUT_INIT_LOW, "p1");
	gpio_request_one(PIN2, GPIOF_OUT_INIT_LOW, "p2");
	gpio_request_one(PIN3, GPIOF_OUT_INIT_LOW, "p3");
	gpio_request_one(PIN4, GPIOF_OUT_INIT_LOW, "p4");

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &act_module_fops);
	cdev_add(cd_cdev, dev_num, 1);
	
	return 0;
}

static void __exit actuator_motor_exit(void){
	
	disable_irq(irq_num);
	free_irq(irq_num, NULL);
	gpio_free(SWITCH);

	gpio_free(PIN1);
	gpio_free(PIN2);
	gpio_free(PIN3);
	gpio_free(PIN4);

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num,1);
}

module_init(actuator_motor_init);
module_exit(actuator_motor_exit);
