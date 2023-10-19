#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

int gsensor_driver_load = 0;
EXPORT_SYMBOL_GPL(gsensor_driver_load);

static struct platform_driver *gsensor_drivers[10];
static int gsensor_driver_count = 0;

int gsensor_driver_register(struct platform_driver *pdrv)
{
    gsensor_drivers[gsensor_driver_count] = pdrv;
    gsensor_driver_count++;
    return 0;
}
EXPORT_SYMBOL_GPL(gsensor_driver_register);


int gsensor_search_driver(void)
{
    int i;
    
    for(i = 0; i < gsensor_driver_count; i++)
    {
        platform_driver_register(gsensor_drivers[i]);
        printk("gsensor_driver_load=%d\n",gsensor_driver_load);
        if(gsensor_driver_load)
            break;
        else
            platform_driver_unregister(gsensor_drivers[i]);
    }
    return 0;
}
EXPORT_SYMBOL_GPL(gsensor_search_driver);

/*----------------------------------------------------------------------------*/
static int __init gsensor_init(void)
{

    return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit gsensor_exit(void)
{
    
}
/*----------------------------------------------------------------------------*/
module_init(gsensor_init);
module_exit(gsensor_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("huangyisong");
MODULE_DESCRIPTION("gsensor driver");
MODULE_LICENSE("GPL");
