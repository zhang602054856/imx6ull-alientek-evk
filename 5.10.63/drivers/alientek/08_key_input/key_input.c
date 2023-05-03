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
#include <linux/input.h>




#define DEVICE_NAME        "key"            /*device name*/
#define CLASS_NAME        "key_class"        /*class name*/

#define KEY0_ACTIVE     0x01
#define INVALID_KEY        0xFF

#define DEV_NUM            1    // number of device
#define KEY_DEBOUNCE    15    //ms


/* gpio  desc struct*/
struct key_desc_t {
    //int gpio_numb;                // use of_get_named_gpio()
    struct gpio_desc *gpiod;    // use devm_gpiod_get()
    int irq_numb;
    char name[10];

    //atomic_t key_value;
    //atomic_t key_released;
    struct timer_list timer;
    //wait_queue_head_t wait;
    int value;
    irqreturn_t (*handler)(int, void *);    /*interrupt handle*/
    struct input_dev *input;
};

/*device driver struct*/
struct key_dev {
    dev_t dev;
    //struct cdev cdev;
    //struct class *class;
    //struct device *device;
    struct device_node *node;

    unsigned char current_key;
    struct key_desc_t keys[DEV_NUM];
};

static struct key_dev *pkey_data = NULL;

/* key input irq handle function */
static irqreturn_t key_handler(int irq, void *key_data)
{
    struct key_desc_t *key_desc = NULL;
    struct key_dev *pkey_dev = NULL;
    int i = 0;

    pkey_dev = (struct key_dev *)key_data;
    for(i = 0; i<DEV_NUM; i++) {
        if((key_desc = &pkey_dev->keys[i]) &&
            irq == key_desc->irq_numb) {
            pkey_dev->current_key = i;
            mod_timer(    &key_desc->timer,
                        jiffies + msecs_to_jiffies(KEY_DEBOUNCE));
            printk("%s|find %s and irq(%d)\n", __func__,
                                        key_desc->name, irq);
            break;
        }
    }
    //key_desc->timer.flags = (u32)key_desc;
    return IRQ_RETVAL(IRQ_HANDLED);
}

/* use to debounce the key */
static void timer_handler(struct timer_list * timer)
{
    unsigned char state;
    struct key_desc_t *key_desc = &pkey_data->keys[pkey_data->current_key];

    //state = gpio_get_value(key_desc->gpio_numb);
    //state = gpiod_get_value(key_desc->gpiod);    // the state return according dts
    state = gpiod_get_raw_value(key_desc->gpiod);
    if(state == 0) {    // key pressed
        //atomic_set(&key_desc->key_value, KEY0_ACTIVE);
        input_report_key(key_desc->input, key_desc->value, 1);
        input_sync(key_desc->input);
        printk("%s|key pressed, gpio_state=%d\n", __func__, state);
    }
    else {                // key released
        //atomic_set(&key_desc->key_value, KEY0_ACTIVE|0x80);
        //atomic_set(&key_desc->key_released, 1);
        input_report_key(key_desc->input, key_desc->value, 0);
        input_sync(key_desc->input);
        printk("%s|key released, gpio_state=%d\n", __func__, state);
    }
    /* a valid key recognized */
    // if(atomic_read(&key_desc->key_released)) {
    //     wake_up_interruptible(&key_desc->wait);
    // }
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
    struct key_desc_t *key_desc = NULL;
    struct input_dev *input_dev;
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
        key_desc->irq_numb = irq_of_parse_and_map(pkey_data->node, i);
        //key_desc->irq_numb = gpio_to_irq(key_desc->gpio_numb);
        printk("gpio(%s)=> irq(%d)\n", key_desc->name,key_desc->irq_numb);
        key_desc->handler = key_handler;
        key_desc->value = KEY_0;
        /* initial atomic for key */
        //atomic_set(&key_desc->key_value, 0);
        //atomic_set(&key_desc->key_released, 0);

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
        //init_waitqueue_head(&key_desc->wait);
        /* initial the timer for key debounce */
        timer_setup(&key_desc->timer, timer_handler, 0);

        input_dev = input_allocate_device();
        input_dev->name = "key_input_jzhang";
        __set_bit(EV_KEY, input_dev->evbit); /* key event*/
        __set_bit(EV_REP, input_dev->evbit); /* repeat event */
        //input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
        input_set_capability(input_dev, EV_KEY, KEY_0);
        key_desc->input = input_dev;
        ret = input_register_device(input_dev);
        if(ret) {
            printk("KEY input register failed\n");
            return ret;
        }
    }

    /* register as char device*/
/*
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
*/

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
    struct key_desc_t *key_desc = NULL;

    printk("%s\n", __func__);

    for(i = 0; i < DEV_NUM; i++) {
        key_desc = &pkey_data->keys[i];
        del_timer_sync(&key_desc->timer);
        free_irq(key_desc->irq_numb, pkey_data);
        input_unregister_device(key_desc->input);
        input_free_device(key_desc->input);
    }
    /* unregister char device */
    //unregister_chrdev(LED_MAJOR, DEVICE_NAME);
    /*
    cdev_del(&pkey_data->cdev);
    unregister_chrdev_region(pkey_data->dev, DEV_NUM);
    device_destroy(pkey_data->class, pkey_data->dev);
    class_destroy(pkey_data->class);
    */

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
