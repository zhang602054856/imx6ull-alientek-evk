#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>





#define DEVICE_NAME        "key"            /*device name*/
#define CLASS_NAME        "key_class"        /*class name*/

#define KEY0_ACTIVE     0x01
#define INVALID_KEY        0xFF

#define DEV_NUM            1    // number of device
#define KEY_DEBOUNCE    15    //ms


/* gpio irq desc struct*/
struct irq_key_desc {
    int gpio_numb;                // use of_get_named_gpio()
    struct gpio_desc *gpiod;    // use devm_gpiod_get()
    int irq_numb;
    char name[10];

    atomic_t key_value;
    atomic_t key_released;
    struct timer_list timer;
    wait_queue_head_t wait;

    irqreturn_t (*handler)(int, void *);    /*interrupt handle*/
};

/*device driver struct*/
struct key_dev {
    dev_t dev;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *node;

    unsigned char current_key;
    struct irq_key_desc keys[DEV_NUM];
};

static struct key_dev *pkey_data = NULL;

/*
 * @description        : 打开设备
 * @param - inode     : 传递给驱动的inode
 * @param - filp     : 设备文件，file结构体有个叫做private_data的成员变量
 *                       一般在open的时候将private_data指向设备结构体。
 * @return             : 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
    if(pkey_data != NULL) {
        filp->private_data = pkey_data;
        return 0;
    }
    return -1;
}

/*
 * @description        : 从设备读取数据
 * @param - filp     : 要打开的设备文件(文件描述符)
 * @param - buf     : 返回给用户空间的数据缓冲区
 * @param - cnt     : 要读取的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return             : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char value;
    struct key_dev *data = filp->private_data;
    struct irq_key_desc *key_desc = NULL;

    DECLARE_WAITQUEUE(wait, current);
    if(data == NULL) {
        printk("%s|null filp->private_data\n", __func__);
         return -1;
    }

    key_desc = &data->keys[data->current_key];
     // ret = wait_event_interruptible(key_desc->wait, atomic_read(&key_desc->key_released));
    // if (ret) {
    //     goto wait_error;
    // }
    if(atomic_read(&key_desc->key_released) == 0) {    // there is no key pressed
        add_wait_queue(&key_desc->wait, &wait);
        __set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        if(signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto wait_error;
        }
        __set_current_state(TASK_RUNNING);
        remove_wait_queue(&key_desc->wait, &wait);
    }
    value = atomic_read(&key_desc->key_value);
    if(value & 0x80) {    // key already released
        value &= ~0x80;
        ret = copy_to_user(buf, &value, sizeof(value));
        atomic_set(&key_desc->key_released, 0);
    }
    else {
        return -EINVAL;
    }

    return sizeof(value);
/*
    gpiod_set_value(pkey_data->gpiod, stat);
    if(stat == DEVICE_ON) {
        gpio_set_value(data->gpio_numb, 0);
    }
    else if(stat == DEVICE_OFF) {
        gpio_set_value(data->gpio_numb, 1);
    }
    else {
        printk("%s|unknow paramters %d !\n", __func__, stat);
    }
*/
wait_error:
    set_current_state(TASK_RUNNING);
    remove_wait_queue(&key_desc->wait, &wait);
    return ret;
}

/*
 * @description        : 向设备写数据
 * @param - filp     : 设备文件，表示打开的文件描述符
 * @param - buf     : 要写给设备写入的数据
 * @param - cnt     : 要写入的数据长度
 * @param - offt     : 相对于文件首地址的偏移
 * @return             : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/* 设备操作函数 */
static struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open,
    .read = key_read,
    .write = key_write,
};

static irqreturn_t key_handler(int irq, void *key_data)
{
    struct irq_key_desc *key_desc = NULL;
    struct key_dev *pkey_dev = NULL;
    int i = 0;

    if(key_data != NULL) {
        pkey_dev = (struct key_dev *)key_data;
        for(i = 0; i<DEV_NUM; i++) {
            if((key_desc = &pkey_dev->keys[i]) &&
                irq == key_desc->irq_numb) {
                pkey_dev->current_key = i;
                mod_timer(    &key_desc->timer,
                            jiffies + msecs_to_jiffies(KEY_DEBOUNCE));
                printk("%s|find key(%s), irq(%d)\n", __func__,
                                                     key_desc->name, irq);
                break;
            }
        }
        //key_desc->timer.flags = (u32)key_desc;
    }
    return IRQ_RETVAL(IRQ_HANDLED);
}

/* use to debounce the key */
static void timer_handler(struct timer_list * timer)
{
    unsigned char state;
    struct irq_key_desc *key_desc = &pkey_data->keys[pkey_data->current_key];

    //state = gpio_get_value(key_desc->gpio_numb);
    state = gpiod_get_value(key_desc->gpiod);
    if(state == 1) {    // key pressed
        atomic_set(&key_desc->key_value, KEY0_ACTIVE);
        printk("%s|key pressed\n", __func__);
    }
    else {                // key released
        atomic_set(&key_desc->key_value, KEY0_ACTIVE|0x80);
        atomic_set(&key_desc->key_released, 1);
        printk("%s|key released\n", __func__);
    }

    if(atomic_read(&key_desc->key_released)) {    /* a valid key recognized */
        wake_up_interruptible(&key_desc->wait);
    }
}


/*
 * @description    :
 * @param         : 无
 * @return         : 无
 */
static int key_probe(struct platform_device *platform_dev)
{
    int ret = 0;
    unsigned char i = 0;
    struct property *proper;
    struct irq_key_desc *key_desc = NULL;
    struct device *dev = &platform_dev->dev;

    pkey_data = kcalloc(1, sizeof(struct key_dev), GFP_KERNEL);
    /*get of device root node*/
    pkey_data->node = of_find_node_by_path("/plat_key");
    if(!pkey_data->node) {
        printk("cannot find /plat_key in dts\n");
        return -EINVAL;
    }
    printk("get dts node:%s\n", pkey_data->node->name);

    proper = of_find_property(pkey_data->node, "compatible", NULL);
    if(!proper) {
        printk("get compatible in node failed\n");
        return -EINVAL;
    }
    printk("get compatible: %s\n", (char *)proper->value);

    /*request GPIO for "key" in dts*/
    for (i=0; i < DEV_NUM; i++) {
        key_desc = &pkey_data->keys[i];
        /*
        key_desc->gpio_numb = of_get_named_gpio(pkey_data->node, "key-gpio", i);
        if(key_desc->gpio_numb < 0) {
            printk("get gpio number failed\n");
            return -EINVAL;
        }
        printk("get gpio number %d\n", key_desc->gpio_numb);
        gpio_request(key_desc->gpio_numb, key_desc->name);
        gpio_direction_input(key_desc->gpio_numb);
        */

        key_desc->gpiod = devm_gpiod_get(dev, "key", GPIOD_IN);
        if (IS_ERR(key_desc->gpiod)) {
            dev_warn(dev, "Failed to get key-gpio\n");
            return -EINVAL;
        }
        memset(key_desc->name, 0, 10);
        sprintf(key_desc->name, "key%d", i);
        //key_desc->irq_numb = irq_of_parse_and_map(pkey_data->node, i);
        key_desc->irq_numb = gpiod_to_irq(key_desc->gpiod);
        //key_desc->irq_numb = gpio_to_irq(key_desc->gpio_numb);
        printk("gpio(%s)=> irq(%d)\n", key_desc->name,key_desc->irq_numb);
        key_desc->handler = key_handler;
        /* initial atomic for key */
        atomic_set(&key_desc->key_value, 0);
        atomic_set(&key_desc->key_released, 0);

        ret = request_irq(    key_desc->irq_numb,
                            key_desc->handler,
                            IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                            key_desc->name,
                            pkey_data);
        if(ret < 0) {
            printk("request_irq failed for key%d\n", i);
            return -EINVAL;
        }
        /* initial wait head for read */
        init_waitqueue_head(&key_desc->wait);
        /* initial the timer for key debounce */
        timer_setup(&key_desc->timer, timer_handler, 0);
    }

    /* register as char device*/
    ret = alloc_chrdev_region(&pkey_data->dev, 20, DEV_NUM, DEVICE_NAME);
    printk("allocate char device number for key, major=%d, minor=%d\n",
            MAJOR(pkey_data->dev), MINOR(pkey_data->dev));

    //    create char devices
    cdev_init(&pkey_data->cdev, &key_fops);
    pkey_data->cdev.owner = THIS_MODULE;
    cdev_add(&pkey_data->cdev, pkey_data->dev, DEV_NUM);

    // create class and devices
    pkey_data->class = class_create(THIS_MODULE, CLASS_NAME);
    pkey_data->device = device_create(pkey_data->class, NULL,
                                pkey_data->dev, NULL, DEVICE_NAME);

    return 0;
}

/*
 * @description    : 驱动出口函数
 * @param         : 无
 * @return         : 无
 */
static int key_remove(struct platform_device *dev)
{
    int i = 0;
    struct irq_key_desc *key_desc = NULL;

    printk("%s\n", __func__);

    for(i = 0; i < DEV_NUM; i++) {
        key_desc = &pkey_data->keys[i];
        del_timer_sync(&key_desc->timer);
        free_irq(key_desc->irq_numb, pkey_data);
    }
    /* 注销字符设备驱动 */
    //unregister_chrdev(LED_MAJOR, DEVICE_NAME);
    cdev_del(&pkey_data->cdev);
    unregister_chrdev_region(pkey_data->dev, DEV_NUM);
    device_destroy(pkey_data->class, pkey_data->dev);
    class_destroy(pkey_data->class);
    kfree(pkey_data);
    return 0;
}


static const struct of_device_id key_of_match[] = {
    { .compatible = "evk-plat-key" },
    { /* must be end of null*/ }
};

static struct platform_driver key_driver = {
    .driver = {
        .name = "alientek-key",
        .of_match_table = key_of_match,
    },
    .probe = key_probe,
    .remove = key_remove,
};

static int __init alientek_key_init(void)
{
    return platform_driver_register(&key_driver);
}

static void __exit alientek_key_exit(void)
{
    platform_driver_unregister(&key_driver);
}

module_init(alientek_key_init);
module_exit(alientek_key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("jzhang35");
