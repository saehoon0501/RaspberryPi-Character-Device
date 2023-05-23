#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <asm/delay.h>

MODULE_LICENSE("GPL");

#define ULTRA_TRIG 17
#define ULTRA_ECHO 18
#define DEV_NAME "sensor_module"

#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1

#define SENSOR_IOCTL_NUM 'z'
#define SENSOR_IOCTL_READ _IOWR(SENSOR_IOCTL_NUM, IOCTL_NUM1, unsigned long*)

static int irq_num;
static int echo_valid_flag = 3;
static int size=0;

static ktime_t echo_start;
static ktime_t echo_stop;

spinlock_t my_lock;

struct sensor_info{
	struct timer_list timer;
	long delay_jiffies;
	int distance;
};

struct sensor_list{
	struct list_head list;
	struct sensor_info sensor;
};

static struct sensor_info my_sensor;
static struct sensor_list my_list_head;

static int sensor_read(int *buf){
	struct sensor_list *tmp=0;
	int ret;

	printk("sensor data reading\n");

	spin_lock(&my_lock);
	tmp = list_entry(my_list_head.list.next, struct sensor_list, list);
	ret = copy_to_user(buf, &(tmp->sensor).distance, sizeof(struct sensor_info));
	spin_unlock(&my_lock);
	list_del(my_list_head.list.next);
	size--;	
	kfree(tmp);
	
	return ret;
}

static int sensor_write(struct sensor_info *buf){
	struct sensor_list *tmp=0;
        struct list_head *pos=0;
        struct list_head *q = 0;	

	printk("sensor data writing\n");

	spin_lock(&my_lock);
	tmp = (struct sensor_list*)kmalloc(sizeof(struct sensor_list), GFP_KERNEL);
	tmp->sensor = *buf;
	list_add(&(tmp->list), &(my_list_head.list));
	spin_unlock(&my_lock);
	size++;
	
	if(size>100){
		list_for_each_safe(pos, q, &my_list_head.list){
                	tmp = list_entry(pos, struct sensor_list, list);
                	list_del(pos);
                	kfree(tmp);
        	}
		size = 0;
	}
	return 1;
}

static void my_sensor_func(struct timer_list *t){

	if(echo_valid_flag == 3){
		echo_start = ktime_set(0,1);
		echo_stop = ktime_set(0,1);
		echo_valid_flag = 0;

		gpio_set_value(ULTRA_TRIG,1);
		udelay(10);
		gpio_set_value(ULTRA_TRIG,0);

		echo_valid_flag = 1;
	}

	printk("sensor_module: Jiffies %ld, Distance %d \n", jiffies, my_sensor.distance);
	sensor_write(&my_sensor);	
	mod_timer(&my_sensor.timer, jiffies + my_sensor.delay_jiffies);
	
}	

static irqreturn_t simple_ultra_isr(int irq, void* dev_id){

	ktime_t tmp_time;
	s64 time;
	int cm;

	tmp_time = ktime_get();
	if(echo_valid_flag==1){
		
		printk("simple_ultra: Echo UP\n");
		if(gpio_get_value(ULTRA_ECHO) == 1){
			echo_start = tmp_time;
			echo_valid_flag = 2;
		}
	} else if(echo_valid_flag == 2){
		
		printk("simple_ultra: Echo Down\n");
		if(gpio_get_value(ULTRA_ECHO) == 0) {

			echo_stop = tmp_time;
			time = ktime_to_us(ktime_sub(echo_stop, echo_start));
			cm = (int) time/58;
			printk("simple_ultra: Detect %d cm\n", cm);
			
			my_sensor.distance = cm;
			echo_valid_flag = 3;
		}
	}

	return IRQ_HANDLED;
}

static long sensor_module_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	int ret;
	int *buf = arg;
	unsigned long flags;

	printk("calling ioctl\n");

	switch(cmd){
		case SENSOR_IOCTL_READ:
			if(size>0){
				ret = sensor_read(buf);
			}
			break;
		default:
			break;
	}
	return 0;
}

static int sensor_module_open(struct inode *inode, struct file *file){
	printk("sensor module opened\n");
	return 0;
}

static int sensor_module_release(struct inode *inode, struct file *file){
	return 0;
}

struct file_operations sensor_module_fops = {
	.unlocked_ioctl=sensor_module_ioctl,
	.open=sensor_module_open,
	.release=sensor_module_release
};

static dev_t dev_num;
static struct cdev *cd_cdev;

static int __init sensor_module_init(void){
	int ret;

	printk("simple_ultra: Init Module\n");

	gpio_request_one(ULTRA_TRIG, GPIOF_OUT_INIT_LOW, "ULTRA_TRIG");
	gpio_request_one(ULTRA_ECHO, GPIOF_IN, "ULTRA_ECHO");

	irq_num = gpio_to_irq(ULTRA_ECHO);
	ret = request_irq(irq_num, simple_ultra_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "ULTRA_ECHO", NULL);
	if(ret){
		printk("sensor_module: Unable to rquest IRQ: %d\n", ret);
		free_irq(irq_num, NULL);
	}

	spin_lock_init(&my_lock);
	
	my_sensor.delay_jiffies = msecs_to_jiffies(2000);
	my_sensor.distance = 0;
	timer_setup(&my_sensor.timer, my_sensor_func, 0);
	my_sensor.timer.expires = jiffies + my_sensor.delay_jiffies;
	add_timer(&my_sensor.timer);

	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &sensor_module_fops);
	cdev_add(cd_cdev, dev_num, 1);

	INIT_LIST_HEAD(&my_list_head.list);

	return 0;
}

static void __exit sensor_module_exit(void){
	printk("simple_ultra: Exit Module\n");

	struct sensor_list *tmp=0;
	struct list_head *pos=0;
	struct list_head *q = 0;

	free_irq(irq_num, NULL);
	del_timer(&my_sensor.timer);

	list_for_each_safe(pos, q, &my_list_head.list){
		tmp = list_entry(pos, struct sensor_list, list);
		list_del(pos);
		kfree(tmp);
	}

	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num,1);
}

module_init(sensor_module_init);
module_exit(sensor_module_exit);

