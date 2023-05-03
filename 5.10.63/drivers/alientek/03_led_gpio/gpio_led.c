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
/***************************************************************
Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
文件名        : led.c
作者          : 左忠凯
版本           : V1.0
描述           : LED驱动文件。
其他           : 无
论坛            : www.openedv.com
日志           : 初版V1.0 2019/1/30 左忠凯创建
***************************************************************/
#define LED_MAJOR        200        /* 主设备号 */
#define LED_NAME        "led1"/*device name*/
#define LED_CLASS_NAME    "led_class"

#define LEDOFF     0                /* 关灯 */
#define LEDON     1                /* 开灯 */

#define LED_DEV_NUM        1    // number of led device

/*led device struct*/
struct chr_led {
    dev_t dev;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *node;
    int gpio_led;
};

static struct chr_led *led_data = NULL;

/*
 * @description        : 打开设备
 * @param - inode     : 传递给驱动的inode
 * @param - filp     : 设备文件，file结构体有个叫做private_data的成员变量
 *                       一般在open的时候将private_data指向设备结构体。
 * @return             : 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = led_data;
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
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct chr_led *dev = filp->private_data;

    if(dev == NULL) {
        printk("%s|null filp->private_data\n", __func__);
         return -1;
    }
    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }
    ledstat = databuf[0];

    if(ledstat == LEDON) {
        gpio_set_value(dev->gpio_led, 0);
    }
    else if(ledstat == LEDOFF) {
        gpio_set_value(dev->gpio_led, 1);
    }
    else {
        printk("%s|unknow paramters %d !\n", __func__, ledstat);
    }
    return 0;
}

/*
 * @description        : 关闭/释放设备
 * @param - filp     : 要关闭的设备文件(文件描述符)
 * @return             : 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release =     led_release,
};

/*
 * @description    : 驱动出口函数
 * @param         : 无
 * @return         : 无
 */
static int __init led_init(void)
{
    int ret = 0;
    struct property *proper;

    led_data = kcalloc(1, sizeof(struct chr_led), GFP_KERNEL);
    /*get of propeties*/
    led_data->node = of_find_node_by_path("/gpio_led");
    if(!led_data->node) {
        printk("cannot find /gpio_led in dts\n");
        return -EINVAL;
    }
    printk("get dts node: /gpio_led: %p\n", led_data->node);

    proper = of_find_property(led_data->node, "compatible", NULL);
    if(!proper) {
        printk("get compatible in node failed\n");
        return -EINVAL;
    }
    printk("get compatible: %s\n", (char *)proper->value);

    led_data->gpio_led = of_get_named_gpio(led_data->node, "led-gpio", 0);
    if(led_data->gpio_led < 0) {
        printk("get led-gpio numb failed\n");
        return -EINVAL;
    }
    printk("get gpio-led numb: %d\n", led_data->gpio_led);

    ret = gpio_direction_output(led_data->gpio_led, 1);
    if(ret < 0) {
        printk("set gpio as output failed\n");
        return -EINVAL;
    }

    ret = alloc_chrdev_region(&led_data->dev, 0, LED_DEV_NUM, LED_NAME);
    printk("allocate char device number for led, major=%d, minor=%d\n",
            MAJOR(led_data->dev), MINOR(led_data->dev));

    //    create char devices
    cdev_init(&led_data->cdev, &led_fops);
    led_data->cdev.owner = THIS_MODULE;
    cdev_add(&led_data->cdev, led_data->dev, LED_DEV_NUM);

    // create class and devices
    led_data->class = class_create(THIS_MODULE, LED_CLASS_NAME);
    led_data->device = device_create(led_data->class, NULL, led_data->dev, NULL, LED_NAME);

    return 0;
}

/*
 * @description    : 驱动出口函数
 * @param         : 无
 * @return         : 无
 */
static void __exit led_exit(void)
{
    printk("%s\n", __func__);

    /* 注销字符设备驱动 */
    //unregister_chrdev(LED_MAJOR, LED_NAME);
    cdev_del(&led_data->cdev);
    unregister_chrdev_region(led_data->dev, LED_DEV_NUM);
    device_destroy(led_data->class, led_data->dev);
    class_destroy(led_data->class);
    kfree(led_data);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jzhang35");
