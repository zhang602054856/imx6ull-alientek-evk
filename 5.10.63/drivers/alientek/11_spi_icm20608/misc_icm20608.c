
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#include "icm20608_reg.h"

#define DEV_MINOR       35               /*minor num*/
#define DEV_NAME        "misc_icm20608"  /*device name*/

#define DEV_DEV_NUM        1    // number of device
 
/*led device struct*/
struct icm20608_dev {
    //dev_t dev;
    //struct cdev cdev;
    //struct class *class;
    //struct device *device;
    //struct device_node *node;
    int16_t gyro_x_adc;
    int16_t gyro_y_adc;
    int16_t gyro_z_adc;
    int16_t accel_x_adc;
    int16_t accel_y_adc;
    int16_t accel_z_adc;
    int16_t temperature_adc;

    struct gpio_desc *cs_gpiod;
    struct spi_device *spi;
};

static struct icm20608_dev *spi_data = NULL;


static int icm20608_read_reg(struct icm20608_dev *spi_data,
                            uint8_t reg,
                            uint8_t* read_buf,
                            uint8_t len)
{
#if 0
    //uint8_t buf[20];
    //struct spi_message _msg;
    //struct spi_transfer _xfer[2];
    int ret;

    /* before read register, set cs as low first */
    //gpiod_set_value(spi_data->cs_gpiod, 1);

    /*write the register want to read*/
    buf[0] = reg | 0x80;
    _xfer[0].tx_buf = buf;
    _xfer[0].len = 1;
    spi_message_init(&_msg);
    spi_message_add_tail(&_xfer[0], &_msg);
    //ret = spi_sync(spi_data->spi, &_msg);

    /*read the register*/
    buf[0] = 0xFF;
    _xfer[1].rx_buf = read_buf;
    _xfer[1].len = len;
    //spi_message_init(&_msg);
    spi_message_add_tail(&_xfer[1], &_msg);
    ret = spi_sync(spi_data->spi, &_msg);
    //gpiod_set_value(spi_data->cs_gpiod, 0);
#endif

    uint8_t tx_data = reg | 0x80;
    return spi_write_then_read(spi_data->spi, &tx_data, 1, read_buf, len);
}

static int icm20608_write_reg(struct icm20608_dev *spi_data,
                            uint8_t reg, uint8_t value)
{
  //  struct spi_message _msg;
  //  struct spi_transfer _xfer;
    int ret;
    uint8_t buf[2] = {reg & 0x7F, value};
    ret = spi_write(spi_data->spi, buf, sizeof(buf));
/*
    _xfer.tx_buf = buf;
    _xfer.len = sizeof(buf);
    //spi_message_init_with_transfers(&_msg, &_xfer, 1);

    spi_message_init(&_msg);
    spi_message_add_tail(&_xfer, &_msg);

    ret = spi_sync(spi_data->spi, &_msg);
*/
    return ret;
}

/*
 * @description      : 打开设备
 * @param - inode    : 传递给驱动的inode
 * @param - filp     : 设备文件，file结构体有个叫做private_data的成员变量
 *                     一般在open的时候将private_data指向设备结构体。
 * @return           : 0 成功;其他 失败
 */
static int icm20608_open(struct inode *inode, struct file *filp)
{
    int ret;
    uint8_t id = 0;
    struct spi_device *spi;

    printk("%s\n", __func__);
    if(spi_data) {
        spi = spi_data->spi;

        ret = icm20608_read_reg(spi_data, ICM20_WHO_AM_I, &id, 1);
        printk("read ICM20608 ID = %#X\r\n", id);

        icm20608_write_reg(spi_data, ICM20_PWR_MGMT_1, 0x80);
        printk("%s | device reset\n", __func__);
        mdelay(50);

        icm20608_write_reg(spi_data, ICM20_PWR_MGMT_1, 0x01);
        printk("%s | device power on\n", __func__);
        mdelay(50);

        icm20608_write_reg(spi_data, ICM20_SMPLRT_DIV,      0x00); //0x19
        icm20608_write_reg(spi_data, ICM20_GYRO_CONFIG,     0x18); //0x1B
        icm20608_write_reg(spi_data, ICM20_ACCEL_CONFIG,    0x18); //0x1C
        icm20608_write_reg(spi_data, ICM20_CONFIG,          0x04); //0x1A
        icm20608_write_reg(spi_data, ICM20_ACCEL_CONFIG2,   0x04); //0x1D
        icm20608_write_reg(spi_data, ICM20_PWR_MGMT_2,      0x00); //0x6c
        icm20608_write_reg(spi_data, ICM20_LP_MODE_CFG,     0x00); //0x1E
        icm20608_write_reg(spi_data, ICM20_FIFO_EN,         0x00); //0x23

        filp->private_data = spi_data;
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
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    struct icm20608_dev *spi_data = (struct icm20608_dev *)filp->private_data;
    uint8_t _data[14] = {0};

    icm20608_read_reg(spi_data, ICM20_ACCEL_XOUT_H, _data, sizeof(_data));
    spi_data->gyro_x_adc    = _data[8]<<8  | _data[9];
    spi_data->gyro_y_adc    = _data[10]<<8 | _data[11];
    spi_data->gyro_z_adc    = _data[12]<<8 | _data[13];
    spi_data->accel_x_adc   = _data[0]<<8  | _data[1];
    spi_data->accel_y_adc   = _data[2]<<8  | _data[3];
    spi_data->accel_z_adc   = _data[4]<<8  | _data[5];
    spi_data->temperature_adc= _data[6]<<8 | _data[7];
/*
    printk("read icm20608 gyro raw data: x[%d], y[%d], z[%d]\n",
             spi_data->gyro_x_adc, spi_data->gyro_y_adc,spi_data->gyro_z_adc);
    printk("read icm20608 accel raw data: x[%d], y[%d], z[%d]\n",
             spi_data->accel_x_adc, spi_data->accel_y_adc,spi_data->accel_z_adc);
    printk("read icm20608 temperature raw data: %d\n", spi_data->temperature_adc);
*/
    return copy_to_user(buf, _data, cnt);
}

/*
 * @description      : 向设备写数据 
 * @param - filp     : 设备文件，表示打开的文件描述符
 * @param - buf      : 要写给设备写入的数据
 * @param - cnt      : 要写入的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return           : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t icm20608_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description      : 关闭/释放设备
 * @param - filp     : 要关闭的设备文件(文件描述符)
 * @return           : 0 成功;其他 失败
 */
static int icm20608_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* file operation struct */
static struct file_operations icm20608_fops = {
    .owner    = THIS_MODULE,
    .open     = icm20608_open,
    .read     = icm20608_read,
    .write    = icm20608_write,
    .release  = icm20608_release,
};

/*misc device declare*/
static struct miscdevice icm20608_misc_dev = {
    /* data */
    .minor = DEV_MINOR,
    .name = DEV_NAME,
    .fops = &icm20608_fops,
};

/*
 * @description   : spi driver probe function
 * @param         :
 * @return        :
 */
static int icm20608_probe(struct spi_device *spi)
{
    struct device_node *parent_nd = of_get_parent(spi->dev.of_node);

    printk("%s|probed and initialing\n", __func__);
    spi_data = kcalloc(1, sizeof(struct icm20608_dev), GFP_KERNEL);

    spi->mode = SPI_MODE_0;
    spi_setup(spi);
    printk("%s | spi setup\n", __func__);
    misc_register(&icm20608_misc_dev);
    spi_data->spi = spi;

    return 0;
}

/*
 * @description   : spi driver remove function
 * @param         :
 * @return        :
 */
static int icm20608_remove(struct spi_device *spi)
{
    printk("%s\n", __func__);
    misc_deregister(&icm20608_misc_dev);
    kfree(spi_data);
    return 0;
}

/*
*    spi driver relations 
*/
static const struct of_device_id icm20608_of_match[] = {
    { .compatible = "alientek,icm20608" },
    { /* must be end of null*/ }
};

static struct spi_driver icm20608_driver = {
    .driver = {
        .name = "alientek,icm20608",
        .of_match_table = icm20608_of_match,
    },
    .probe = icm20608_probe,
    .remove = icm20608_remove,
};

/*
static int __init icm20608_init(void)
{
    return spi_register_driver(&icm20608_driver);
}

static void __exit icm20608_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
*/

module_spi_driver(icm20608_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jzhang35");
