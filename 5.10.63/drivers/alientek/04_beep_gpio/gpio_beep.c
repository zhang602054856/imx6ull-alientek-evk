#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define BEEP_NAME        "beep"/*device name*/
#define BEEP_CLASS_NAME    "beep_class"

#define LEDOFF     0                /* 关灯 */
#define LEDON     1                /* 开灯 */

#define DEV_NUM        1    // number of led device

/*beep device struct*/
struct chr_beep {
    dev_t dev;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *node;
    int gpio_beep;
};

static struct chr_beep *pbeep_data = NULL;

/*
 * @description        : 打开设备
 * @param - inode     : 传递给驱动的inode
 * @param - filp     : 设备文件，file结构体有个叫做private_data的成员变量
 *                       一般在open的时候将private_data指向设备结构体。
 * @return             : 0 成功;其他 失败
 */
static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = pbeep_data;
    return 0;
}

/*
 * @description        : 从设备读取数据
 * @param - filp     : 要打开的设备文件(文件描述符)
 * @param - buf     : 返回给用户空间的数据缓冲区
 * @param - cnt     : 要读取的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return             : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t beep_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description        : 向设备写数据
 * @param - filp     : 设备文件，表示打开的文件描述符
 * @param - buf     : 要写给设备写入的数据
 * @param - cnt     : 要写入的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return             : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char stat;
    struct chr_beep *dev = filp->private_data;

    if(dev == NULL) {
        printk("%s|null filp->private_data\n", __func__);
         return -1;
    }
    ret = copy_from_user(databuf, buf, cnt);
    if(ret < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }
    stat = databuf[0];

    if(stat == LEDON) {
        gpio_set_value(dev->gpio_beep, 0);
    }
    else if(stat == LEDOFF) {
        gpio_set_value(dev->gpio_beep, 1);
    }
    else {
        printk("%s|unknow paramters %d !\n", __func__, stat);
    }
    return 0;
}

/*
 * @description        : 关闭/释放设备
 * @param - filp     : 要关闭的设备文件(文件描述符)
 * @return             : 0 成功;其他 失败
 */
static int beep_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 设备操作函数 */
static struct file_operations beep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .read = beep_read,
    .write = beep_write,
    .release =     beep_release,
};

/*
 * @description    : 驱动出口函数
 * @param         : 无
 * @return         : 无
 */
static int __init beep_init(void)
{
    int ret = 0;
    struct property *proper;

    pbeep_data = kcalloc(1, sizeof(struct chr_beep), GFP_KERNEL);
    /*get of propeties*/
    pbeep_data->node = of_find_node_by_path("/gpio_beep");
    if(!pbeep_data->node) {
        printk("cannot find /gpio_beep in dts\n");
        return -EINVAL;
    }
    printk("get dts node: /gpio_beep: %p\n", pbeep_data->node);

    proper = of_find_property(pbeep_data->node, "compatible", NULL);
    if(!proper) {
        printk("get compatible in node failed\n");
        return -EINVAL;
    }
    printk("get compatible: %s\n", (char *)proper->value);

    pbeep_data->gpio_beep = of_get_named_gpio(pbeep_data->node, "beep-gpio", 0);
    if(pbeep_data->gpio_beep < 0) {
        printk("get beep-gpio numb failed\n");
        return -EINVAL;
    }
    printk("get beep-gpio numb: %d\n", pbeep_data->gpio_beep);

    ret = gpio_request(pbeep_data->gpio_beep, "beep-gpio");
    if(ret < 0) {
        printk("request gpio (%d) failed\n", pbeep_data->gpio_beep);
        return -EINVAL;
    }

    ret = gpio_direction_output(pbeep_data->gpio_beep, 1);
    if(ret < 0) {
        printk("set gpio as output failed\n");
        return -EINVAL;
    }

    ret = alloc_chrdev_region(&pbeep_data->dev, 20, DEV_NUM, BEEP_NAME);
    printk("allocate char device number for beep, major=%d, minor=%d\n",
            MAJOR(pbeep_data->dev), MINOR(pbeep_data->dev));

    //    create char devices
    cdev_init(&pbeep_data->cdev, &beep_fops);
    pbeep_data->cdev.owner = THIS_MODULE;
    cdev_add(&pbeep_data->cdev, pbeep_data->dev, DEV_NUM);

    // create class and devices
    pbeep_data->class = class_create(THIS_MODULE, BEEP_CLASS_NAME);
    pbeep_data->device = device_create(pbeep_data->class, NULL, pbeep_data->dev, NULL, BEEP_NAME);

    return 0;
}

/*
 * @description    : 驱动出口函数
 * @param         : 无
 * @return         : 无
 */
static void __exit beep_exit(void)
{
    printk("%s\n", __func__);

    /* 注销字符设备驱动 */
    //unregister_chrdev(LED_MAJOR, BEEP_NAME);
    gpio_free(pbeep_data->gpio_beep);
    cdev_del(&pbeep_data->cdev);
    unregister_chrdev_region(pbeep_data->dev, DEV_NUM);
    device_destroy(pbeep_data->class, pbeep_data->dev);
    class_destroy(pbeep_data->class);
    kfree(pbeep_data);
}

module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jzhang35");
