
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#include "ap3216c_reg.h"

#define DEV_MINOR       123              /*minor num*/
#define DEV_NAME        "misc_ap3216"    /*device name*/

#define DEV_DEV_NUM        1    // number of device
 
/*led device struct*/
struct ap32xx_dev {
    //dev_t dev;
    //struct cdev cdev;
    //struct class *class;
    //struct device *device;
    //struct device_node *node;
    uint16_t value[3]; // ir,als,ps
    struct i2c_client *client;
};

static struct ap32xx_dev *ap32xx_data = NULL;


static int ap32xx_read_data(struct i2c_client *client,
                            uint8_t reg,
                            uint8_t* read_buf,
                            uint8_t len)
{

    struct i2c_msg _msg[2];

    //printk("%s|read reg:0x%x, len=%d\n", __func__, reg, len);
    /* before read register, write register first */
    _msg[0].addr = client->addr;
    _msg[0].flags = 0;
    _msg[0].buf = &reg;
    _msg[0].len = 1;

    /*read i2c data finaly*/
    _msg[1].addr = client->addr;
    _msg[1].flags = I2C_M_RD;
    _msg[1].buf = read_buf;
    _msg[1].len = len;

    return i2c_transfer(client->adapter, _msg, 2);
}

static int ap32xx_write_data(struct i2c_client *client,
                            uint8_t reg,
                            uint8_t* write_buf,
                            uint8_t len)
{

    struct i2c_msg _msg;

    uint8_t buf[255] = {0};

    //printk("%s|reg:0x%x, len=%d\n", __func__, reg, len);

    buf[0] = reg;
    memcpy(&buf[1], write_buf, len);

    _msg.addr = client->addr;
    _msg.flags = 0;
    _msg.buf = buf;
    _msg.len = len + 1;

    return i2c_transfer(client->adapter, &_msg, 1);
}

/*
 * @description      : 打开设备
 * @param - inode    : 传递给驱动的inode
 * @param - filp     : 设备文件，file结构体有个叫做private_data的成员变量
 *                     一般在open的时候将private_data指向设备结构体。
 * @return           : 0 成功;其他 失败
 */
static int ap32xx_open(struct inode *inode, struct file *filp)
{
    int8_t reg;
    int ret;
    struct i2c_client *client;
    printk("%s\n", __func__);
    if(ap32xx_data) {
        client = ap32xx_data->client;
        reg = 0x04;  // ap3216c sw reset
        ret = ap32xx_write_data(ap32xx_data->client,
                                AP3216C_SYSTEM_CONG,
                                &reg,
                                1);
        printk("%s|sw reset, ret=%d\n", __func__, ret);
        msleep_interruptible(10);//defined in IC datasheet
        reg = 0x03; // ALS + PS + IR functions active
        ret = ap32xx_write_data(ap32xx_data->client,
                                AP3216C_SYSTEM_CONG,
                                &reg,
                                1);
        printk("%s|sensor init, ret=%d\n", __func__, ret);
        filp->private_data = ap32xx_data;
        ret = 0;
    }
    else {
        ret = -EINVAL;
    }
    return ret;
}

/*
 * @description      : 从设备读取数据 
 * @param - filp     : 要打开的设备文件(文件描述符)
 * @param - buf      : 返回给用户空间的数据缓冲区
 * @param - cnt      : 要读取的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return           : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t ap32xx_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    struct ap32xx_dev *ap32xx_data = (struct ap32xx_dev *)filp->private_data;
    uint8_t _reg_buf[6] = {0};
    int ret, i;

    for(i=0; i<6; i++) {
        ret = ap32xx_read_data(ap32xx_data->client,
                            AP3216C_IR_DATALOW + i,
                            &_reg_buf[i],
                            1);
        if(ret < 0) {
            printk("%s|read error for reg:%d\n",
                    __func__, AP3216C_IR_DATALOW + i);
            return -EINVAL;
        }
    }

    // ir is a 10bit value
    if(_reg_buf[0] & 0x80) {//ir overflow flag
        ap32xx_data->value[0] = 0;
    }
    else {
        ap32xx_data->value[0] = (_reg_buf[1]<<2) | (_reg_buf[0]&0x03);
    }
    // als is a 16bit value
    ap32xx_data->value[1] = (_reg_buf[3] << 8) | _reg_buf[2];
    
    // ps is a 10bit value
    if(_reg_buf[4] & 0x40) {// ps overflow flag
        ap32xx_data->value[2] = 0;
    }
    else {    // ps is a 10bit value
        ap32xx_data->value[2] = ((_reg_buf[5] & 0X3F) << 4) |
                  (_reg_buf[4] & 0X0F);
    }
    /* printk("%s|ir=%d, als=%d, ps=%d\n", __func__,
                                    ap32xx_data->value[0],
                                    ap32xx_data->value[1],
                                    ap32xx_data->value[2]); */
    ret = copy_to_user(buf, ap32xx_data->value, sizeof(ap32xx_data->value));
    return ret;
}

/*
 * @description      : 向设备写数据 
 * @param - filp     : 设备文件，表示打开的文件描述符
 * @param - buf      : 要写给设备写入的数据
 * @param - cnt      : 要写入的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return           : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t ap32xx_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description      : 关闭/释放设备
 * @param - filp     : 要关闭的设备文件(文件描述符)
 * @return           : 0 成功;其他 失败
 */
static int ap32xx_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* file operation struct */
static struct file_operations ap32xx_fops = {
    .owner    = THIS_MODULE,
    .open     = ap32xx_open,
    .read     = ap32xx_read,
    .write    = ap32xx_write,
    .release  = ap32xx_release,
};

/*misc device declare*/
static struct miscdevice ap32xx_misc_dev = {
    /* data */
    .minor = DEV_MINOR,
    .name = DEV_NAME,
    .fops = &ap32xx_fops,
};

/*
 * @description   : i2c probe function
 * @param         :
 * @return        :
 */
static int ap32xx_probe(struct i2c_client *client, 
                        const struct i2c_device_id *id)
{
    printk("%s|%s probed and initialing\n", __func__, client->name);
    ap32xx_data = kcalloc(1, sizeof(struct ap32xx_dev), GFP_KERNEL);
    misc_register(&ap32xx_misc_dev);
    ap32xx_data->client = client;
    return 0;
}

/*
 * @description   : i2c remove function
 * @param         :
 * @return        :
 */
static int ap32xx_remove(struct i2c_client *client)
{
    printk("%s\n", __func__);
    misc_deregister(&ap32xx_misc_dev);
    kfree(ap32xx_data);
    return 0;
}

/*
*    i2c driver relations 
*/
static const struct of_device_id ap32xx_of_match[] = {
    { .compatible = "alientek,ap3216c" },
    { /* must be end of null*/ }
};

static struct i2c_driver ap32xx_driver = {
    .driver = {
        .name = "alientek.ap32xx",
        .of_match_table = ap32xx_of_match,
    },
    .probe = ap32xx_probe,
    .remove = ap32xx_remove,
};

/*
static int __init ap32xx_init(void)
{
    return i2c_add_driver(&ap32xx_driver);
}

static void __exit ap32xx_exit(void)
{
    i2c_del_driver(&ap32xx_driver);
}

module_init(ap32xx_init);
module_exit(ap32xx_exit);
*/
module_i2c_driver(ap32xx_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jzhang35");
