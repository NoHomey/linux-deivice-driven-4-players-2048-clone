#include "tlc5947.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/errno.h>

module_param(tlc5947_data, ushort, CONST_PARAM);
MODULE_PARM_DESC(tlc5967_data, "Number of gpio pin on wich DATA signal is connected (BCM Enum).");
module_param(tlc5947_clock, ushort, CONST_PARAM);
MODULE_PARM_DESC(tlc5967_clock, "Number of gpio pin on wich CLOCK signal is connected (BCM Enum).");
module_param(tlc5947_latch, ushort, CONST_PARAM);
MODULE_PARM_DESC(tlc5967_latch, "Number of gpio pin on wich LATCH signal is connected (BCM Enum).");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivo Stratev");
MODULE_DESCRIPTION("Basic Linux Kernel module using GPIOs to drive tlc5947");
MODULE_SUPPORTED_DEVICE("tlc5947");

static int tlc5947_file_open(struct inode* inode, struct file* file) {
    if(tlc5947_file_opened == 1) {
        printk(KERN_ERR "Fail to open file /dev/%s\n", TLC5947_NAME);
        return -EBUSY;
    }
    tlc5947_file_opened = 1;

    return 0;
}

static ssize_t tlc5947_file_write(struct file* file, const char __user* buffer, const size_t length, loff_t* offset) {
    int i, k;
    return_value = copy_from_user(tlc5947_buffer, buffer, length);
    if(return_value > 0) {
        printk(KERN_ERR "Error while copying data\nCalling copy_from_user reurned%d\n", return_value);
        return -ENOMEM;
    }
    gpio_set_value(tlc5947_latch, GPIO_LOW);
    for(i = length - 1; i >= 0; --i) {
        for(k = 7; k >= 0; --k) {
            gpio_set_value(tlc5947_clock, GPIO_LOW);
            gpio_set_value(tlc5947_data, (tlc5947_buffer[i] & (1 << k)) ? GPIO_HIGH : GPIO_LOW);
            gpio_set_value(tlc5947_clock, GPIO_HIGH);
	   }
    }
    gpio_set_value(tlc5947_clock, GPIO_LOW);
    gpio_set_value(tlc5947_latch, GPIO_HIGH);
    gpio_set_value(tlc5947_latch, GPIO_LOW);

    return length;
}

static int tlc5947_file_close(struct inode* inode, struct file* file) {
    tlc5947_file_opened = 0;

    return 0;
}

long tlc5947_ioctl(struct file * file, unsigned int cmd, unsigned long arg) {
	if(_IOC_TYPE(cmd) != TLC5947_MAGIC_NUMBER) {
		printk("ioctl magic number isn't device driver's one.\ncmd equals %d\n", cmd);
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > TLC5947_MAX_NUMBER) {
		printk("ioctl command exceeds last implemented command.\ncmd equals %d\n", cmd);
		return -ENOTTY;
	}
	switch(cmd) {
		case TLC5947_FREE: {
			if(tlc5947_buffer) {
		        kfree(tlc5947_buffer);
				tlc5947_buffer = NULL;
		    }
			break;
		}
		case TLC5947_ALLOC: {
			if(!tlc5947_buffer) {
		        tlc5947_buffer = (char*) kmalloc(arg * sizeof(char), GFP_KERNEL);
		        if(!tlc5947_buffer) {
		            printk(KERN_ERR "Failed to allocate memmry!\nCalling kmalloc returned NULL\n");
					tlc5947_exit();
		            return -ENOMEM;
		        }
		    }
		}
	}

	return 0;
}

static int __init tlc5947_init(void) {
    if(tlc5947_data == 255) {
        printk(KERN_ERR "Parameter tlc5947_data value not setted when loading the module\n");
		tlc5947_exit();
        return -EINVAL;
    }
    if(tlc5947_clock == 255) {
        printk(KERN_ERR "Parameter tlc5947_clock value not setted when loading the module\n");
		tlc5947_exit();
        return -EINVAL;
    }
    if(tlc5947_latch == 255) {
        printk(KERN_ERR "Parameter tlc5947_latch value not setted when loading the module\n");
		tlc5947_exit();
        return -EINVAL;
    }
    tlc5947[0].gpio = tlc5947_data;
	tlc5947[1].gpio = tlc5947_clock;
	tlc5947[2].gpio = tlc5947_latch;
	return_value = gpio_request_array(tlc5947, TLC5947_GPIOS);
    if(return_value) {
        printk(KERN_ERR "Unable to request GPIOs!\nCalling gpio_request_array returned %d\n", return_value);
		tlc5947_exit();
        return return_value;
    }
    return_value = alloc_chrdev_region(&tlc5947_numbers, tlc5947_first_minor, tlc5947_minor_count, TLC5947_NAME);
    if(return_value) {
       printk(KERN_ERR "Could not allocate device numbers\nCalling alloc_chrdev_region returned %d\n", return_value);
	   tlc5947_exit();
       return return_value;
    }
    tlc5947_major_number = MAJOR(tlc5947_numbers);
    printk(KERN_INFO "Device major number is %d. Use $ sudo make device major=%d\n", tlc5947_major_number, tlc5947_major_number);
    tlc5947_cdev = cdev_alloc();
    tlc5947_cdev->owner = THIS_MODULE;
    tlc5947_file_operations.owner = THIS_MODULE;
    tlc5947_file_operations.open = tlc5947_file_open;
    tlc5947_file_operations.release = tlc5947_file_close;
    tlc5947_file_operations.write = tlc5947_file_write;
	tlc5947_file_operations.unlocked_ioctl = tlc5947_ioctl;
    tlc5947_cdev->ops = &tlc5947_file_operations;
    return_value = cdev_add(tlc5947_cdev, tlc5947_numbers, tlc5947_minor_count);
    if(return_value) {
        printk(KERN_ERR "Failed to add device numbers to struct cdev\nCalling cdev_add returned %d\n", return_value);
		tlc5947_exit();
        return return_value;
    }
    tlc5947_file_opened = 0;
    tlc5947_buffer = NULL;

    return 0;
}

static void tlc5947_exit(void) {
	if(tlc5947_cdev) {
		cdev_del(tlc5947_cdev);
	}
	if(tlc5947_major_number > -1) {
		unregister_chrdev_region(tlc5947_numbers, tlc5947_minor_count);
	}
	if((tlc5947[0].gpio == tlc5947_data) && (tlc5947[1].gpio == tlc5947_clock) && (tlc5947[2].gpio == tlc5947_latch)) {
		gpio_free_array(tlc5947, TLC5947_GPIOS);
	}
}

module_init(tlc5947_init);
module_exit(tlc5947_exit);
