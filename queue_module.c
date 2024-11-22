#include <asm-generic/errno.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define QUEUE_SIZE 10
#define DEVICE_NAME "myQueue"
#define CLASS_NAME "myQueueClass"

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static int majorNumber;
static char queue[QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;
static int blocking = 1;

static DECLARE_WAIT_QUEUE_HEAD(read_queue);
static DECLARE_WAIT_QUEUE_HEAD(write_queue);

static struct class* myQueueClass = NULL;
static struct device* myQueueDevice = NULL;

static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init myQueue_init(void) {
    printk(KERN_INFO "Initializing the myQueue\n");

    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "myQueue failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "myQueue: registered correctly with major number %d\n", majorNumber);

    myQueueClass = class_create(CLASS_NAME);
    if (IS_ERR(myQueueClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(myQueueClass);
    }
    printk(KERN_INFO "myQueue: device class registered correctly\n");

    myQueueDevice = device_create(myQueueClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(myQueueDevice)) {
        class_destroy(myQueueClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(myQueueDevice);
    }
    printk(KERN_INFO "myQueue: device class created correctly\n");

    return 0;
}

static void __exit myQueue_exit(void) {
    device_destroy(myQueueClass, MKDEV(majorNumber, 0));
    class_destroy(myQueueClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "Goodbye from the myQueue LKM\n");
}

static int dev_open(struct inode* inodep, struct file* filep) {
    printk(KERN_INFO "myQueue: Device has been opened\n");
    return 0;
}

static int dev_release(struct inode* inodep, struct file* filep) {
    printk(KERN_INFO "myQueue: Device successfully closed\n");
    return 0;
}

static ssize_t dev_read(struct file* filep, char* buffer, size_t len, loff_t* offset) {
    int error_count = 1;

    if (queue_count == 0) {
        if (blocking) {
            wait_event_interruptible_timeout(read_queue, queue_count > 0, msecs_to_jiffies(10000));
            if (queue_count == 0) {
                return 0;
            }
        } else {
            return 0;
        }
    }

    if (queue_count > 0) {
        char ch = queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;
        wake_up_interruptible(&write_queue);

        error_count = copy_to_user(buffer, &ch, 1);
        if (error_count == 0) {
            printk(KERN_INFO "myQueue: Sent %d characters to the user\n", 1);
            return 1;
        } else {
            printk(KERN_INFO "myQueue: Failed to send %d characters to the user\n", error_count);
            return -EFAULT;
        }
    }

    return 0;
}

static ssize_t dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset) {
    int error_count = 0;

    if (queue_count == QUEUE_SIZE) {
        if (blocking) {
            wait_event_interruptible_timeout(write_queue, queue_count < QUEUE_SIZE, msecs_to_jiffies(10000));
            if (queue_count == QUEUE_SIZE) {
                return -ETIMEDOUT;
            }
        } else {
            return 0;
        }
    }

    if (queue_count < QUEUE_SIZE) {
        char ch;
        error_count = copy_from_user(&ch, buffer, 1);
        if (error_count == 0) {
            queue[queue_tail] = ch;
            queue_tail = (queue_tail + 1) % QUEUE_SIZE;
            queue_count++;
            wake_up_interruptible(&read_queue);

            printk(KERN_INFO "myQueue: Received %d characters from the user\n", 1);
            return 1;
        } else {
            printk(KERN_INFO "myQueue: Failed to receive %d characters from the user\n", error_count);
            return -EFAULT;
        }
    }

    return 0;
}

module_param(blocking, int, 0644);
MODULE_PARM_DESC(blocking, "Blocking mode (1) or non-blocking mode (0)");

module_init(myQueue_init);
module_exit(myQueue_exit);
