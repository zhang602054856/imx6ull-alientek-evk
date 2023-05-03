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
#define LED_NAME        "led0"/*device name*/
#define LED_CLASS_NAME    "led_class"

#define LEDOFF     0                /* 关灯 */
#define LEDON     1                /* 开灯 */

#define LED_DEV_NUM        1    // number of led device

/* register physical address*/
// #define CCM_CCGR1_BASE                (0X020C406C)
// #define SW_MUX_GPIO1_IO03_BASE        (0X020E0068)
// #define SW_PAD_GPIO1_IO03_BASE        (0X020E02F4)
// #define GPIO1_DR_BASE                (0X0209C000)
// #define GPIO1_GDIR_BASE                (0X0209C004)

/* register victual address after iomap */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/*led device struct*/
struct chr_led {
    dev_t dev;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *node;
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
    int retvalue, val;
    unsigned char databuf[1];
    unsigned char ledstat;

    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }
    ledstat = databuf[0];

    if(ledstat == LEDON) {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    }
    else if(ledstat == LEDOFF) {
        val = readl(GPIO1_DR);
        val |= (1 << 3);
        writel(val, GPIO1_DR);
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
    u32 val = 0;
    struct property *proper;
    const char *state;
    u32 reg_buf[10] = {0};

    led_data = kcalloc(1, sizeof(struct chr_led), GFP_KERNEL);
    /*get of propeties*/
    led_data->node = of_find_node_by_path("/led");
    if(!led_data->node) {
        printk("cannot find /led in dts\n");
        return -EINVAL;
    }
    printk("get dts node: /led\n");

    proper = of_find_property(led_data->node, "compatible", NULL);
    if(!proper) {
        printk("get compatible in node failed\n");
        return -EINVAL;
    }
    printk("get compatible: %s\n", (char *)proper->value);

    ret = of_property_read_string(led_data->node, "status", &state);
    if(ret < 0) {
        printk("get status failed\n");
        return -EINVAL;
    }
    printk("get status: %s\n", state);

    ret = of_property_read_u32_array(led_data->node, "reg", reg_buf, 10);
    if(ret < 0) {
        printk("get register data failed\n");
        return -EINVAL;
    }
    else {
        u8 i= 0;
        for(i=0; i<10/2; i++) {
            printk("regs: 0x%08x, %d\n", reg_buf[i*2], reg_buf[i*2+1]);
        }
    }

    /* 初始化LED */
    /* 1、寄存器地址映射 */
      // IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    // SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
      // SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    // GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    // GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
      IMX6U_CCM_CCGR1 = of_iomap(led_data->node, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(led_data->node, 1);
      SW_PAD_GPIO1_IO03 = of_iomap(led_data->node, 2);
    GPIO1_DR = of_iomap(led_data->node, 3);
    GPIO1_GDIR = of_iomap(led_data->node, 4);

    /* 2、使能GPIO1时钟 */
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);    /* 清楚以前的设置 */
    val |= (3 << 26);    /* 设置新值 */
    writel(val, IMX6U_CCM_CCGR1);

    /* 3、设置GPIO1_IO03的复用功能，将其复用为
     *    GPIO1_IO03，最后设置IO属性。
     */
    writel(5, SW_MUX_GPIO1_IO03);

    /*寄存器SW_PAD_GPIO1_IO03设置IO属性
     *bit 16:0 HYS关闭
     *bit [15:14]: 00 默认下拉
     *bit [13]: 0 kepper功能
     *bit [12]: 1 pull/keeper使能
     *bit [11]: 0 关闭开路输出
     *bit [7:6]: 10 速度100Mhz
     *bit [5:3]: 110 R0/6驱动能力
     *bit [0]: 0 低转换率
     */
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    /* 4、设置GPIO1_IO03为输出功能 */
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3);    /* 清除以前的设置 */
    val |= (1 << 3);    /* 设置为输出 */
    writel(val, GPIO1_GDIR);

    /* 5、默认关闭LED */
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);
#if 0
    /* register the char device with old solution*/
    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if(retvalue < 0){
        printk("register chrdev failed!\r\n");
        return -EIO;
    }
#else
    ret = alloc_chrdev_region(&led_data->dev, 20, LED_DEV_NUM, LED_NAME);
    printk("allocate char device number for led, major=%d, minor=%d\n",
            MAJOR(led_data->dev), MINOR(led_data->dev));

    //    create char devices
    cdev_init(&led_data->cdev, &led_fops);
    led_data->cdev.owner = THIS_MODULE;
    cdev_add(&led_data->cdev, led_data->dev, LED_DEV_NUM);

    // create class and devices
    led_data->class = class_create(THIS_MODULE, LED_CLASS_NAME);
    led_data->device = device_create(led_data->class, NULL, led_data->dev, NULL, LED_NAME);

#endif
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
    /* 取消映射 */
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

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
