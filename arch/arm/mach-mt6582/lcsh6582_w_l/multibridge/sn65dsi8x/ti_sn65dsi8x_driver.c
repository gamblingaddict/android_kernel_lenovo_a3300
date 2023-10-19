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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include "ti_sn65dsi8x_driver.h"

static size_t ti_sn65dsi8x_log_on = true;
#define ti_sn65dsi8x_LOG(fmt, arg...) \
	do { \
		if (ti_sn65dsi8x_log_on) {printk("[ti_sn65dsi8x]%s,#%d ", __func__, __LINE__); printk(fmt, ##arg);} \
	}while (0)
        
#define ti_sn65dsi8x_FUNC()	\
    do { \
        if(ti_sn65dsi8x_log_on) printk("[ti_sn65dsi8x] %s\n", __func__); \
    }while (0)
/**********************************************************
  *
  *   [I2C Slave Setting] 
  *
  *********************************************************/
#define sn65dsi8x_SLAVE_ADDR_WRITE   0x5a
#define sn65dsi8x_SLAVE_ADDR_Read    0x5b

static struct i2c_client *sn65dsi8x_iic_client = NULL;
static const struct i2c_device_id sn65dsi8x_i2c_id[] = {{"sn65dsi8x",0},{}};   
unsigned int g_sn65dsi8x_rdy_flag = 0;
static int sn65dsi8x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver sn65dsi8x_driver = {
    .driver = {
        .name    = "sn65dsi8x",
    },
    .probe       = sn65dsi8x_driver_probe,
    .id_table    = sn65dsi8x_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable] 
  *
  *********************************************************/

static DEFINE_MUTEX(sn65dsi8x_i2c_access);
/**********************************************************
  *
  *   [I2C Function For Read/Write sn65dsi8x] 
  *
  *********************************************************/
int sn65dsi8x_read_byte(kal_uint8 cmd, kal_uint8 *returnData)
{
    char     readData = 0;
    int      ret=0;

    mutex_lock(&sn65dsi8x_i2c_access);
    ti_sn65dsi8x_LOG("read cmd=%x \n",cmd);

    ret = i2c_master_send(sn65dsi8x_iic_client, (char *)&cmd,1);
    if (ret < 0) 
    {    
	ti_sn65dsi8x_LOG("send faild\n");
        mutex_unlock(&sn65dsi8x_i2c_access);
        return 0;
    }
    ret = i2c_master_recv(sn65dsi8x_iic_client, &readData, 1);
    if (ret < 0) {
	ti_sn65dsi8x_LOG("reve faild\n");
	mutex_unlock(&sn65dsi8x_i2c_access);
        return 0;
     }

    *returnData = readData;
    
    mutex_unlock(&sn65dsi8x_i2c_access);    
    return 1;
}
EXPORT_SYMBOL_GPL(sn65dsi8x_read_byte);
int sn65dsi8x_write_byte(kal_uint8 cmd, kal_uint8 Data)
{
    char    write_data[2] = {cmd,Data};
    int     ret=0;
    
    mutex_lock(&sn65dsi8x_i2c_access);
    ti_sn65dsi8x_LOG("write:cmd=0x%x data=0x%x \n",write_data[0],write_data[1]);
    
    ret = i2c_master_send(sn65dsi8x_iic_client,(const char*)write_data,2);
    if (ret < 0) 
    {
	ti_sn65dsi8x_LOG("write faild\n");
        mutex_unlock(&sn65dsi8x_i2c_access);
        return 0;
    }
    
    mutex_unlock(&sn65dsi8x_i2c_access);
    return 1;
}
EXPORT_SYMBOL_GPL(sn65dsi8x_write_byte);
/**********************************************************
  *
  *   [Internal Function] 
  *
  *********************************************************/
void sn65dsi8x_dump_register(void)
{
    printk("[sn65dsi8x] sn65dsi8x_dump_register do nothing");
}


void sn65dsi8x_hw_init(void)
{    
     printk("sn65dsi8x_hw_init do nothing now \n");
}

static int sn65dsi8x_driver_probe(struct i2c_client *client, const struct i2c_device_id *id) 
{             
    int err=0; 

    printk("[sn65dsi8x_driver_probe] \n");

    if (!(sn65dsi8x_iic_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL))) {
        err = -ENOMEM;
        goto exit;
    }    
    memset(sn65dsi8x_iic_client, 0, sizeof(struct i2c_client));

    sn65dsi8x_iic_client = client;    

    //---------------------
    sn65dsi8x_hw_init();
    sn65dsi8x_dump_register();
    g_sn65dsi8x_rdy_flag = 1;

    return 0;                                                                                       

exit:
    return err;

}

/**********************************************************
  *
  *   [platform_driver API] 
  *
  *********************************************************/
kal_uint8 g_reg_value_sn65dsi8x=0;
static ssize_t show_sn65dsi8x_access(struct device *dev,struct device_attribute *attr, char *buf)
{
    printk("[show_sn65dsi8x_access] 0x%x\n", g_reg_value_sn65dsi8x);
    return sprintf(buf, "%u\n", g_reg_value_sn65dsi8x);
}
static ssize_t store_sn65dsi8x_access(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
    int ret=0;
    char *pvalue = NULL;
    unsigned int reg_value = 0;
    unsigned int reg_address = 0;
    
    printk("[store_sn65dsi8x_access] \n");
    
    return size;
}
static DEVICE_ATTR(sn65dsi8x_access, 0664, show_sn65dsi8x_access, store_sn65dsi8x_access); //664

static int sn65dsi8x_user_space_probe(struct platform_device *dev)    
{    
    int ret_device_file = 0;

    printk("******** sn65dsi8x_user_space_probe!! ********\n" );
    
    ret_device_file = device_create_file(&(dev->dev), &dev_attr_sn65dsi8x_access);
    
    return 0;
}

struct platform_device sn65dsi8x_user_space_device = {
    .name   = "sn65dsi8x-bridge",
    .id     = -1,
};

static struct platform_driver sn65dsi8x_user_space_driver = {
    .probe      = sn65dsi8x_user_space_probe,
    .driver     = {
        .name = "sn65dsi8x-bridge",
    },
};

#define sn65dsi8x_BUSNUM 0
static struct i2c_board_info __initdata i2c_sn65dsi8x = { I2C_BOARD_INFO("sn65dsi8x", (0x5a>>1))};

static int __init sn65dsi8x_init(void)
{    
    int ret=0;
    
    printk("[sn65dsi8x_init] init start\n");
    
    i2c_register_board_info(sn65dsi8x_BUSNUM, &i2c_sn65dsi8x, 1);

    if(i2c_add_driver(&sn65dsi8x_driver)!=0)
    {
        printk("[sn65dsi8x_init] failed to register sn65dsi8x i2c driver.\n");
    }
    else
    {
        printk("[sn65dsi8x_init] Success to register sn65dsi8x i2c driver.\n");
    }

    // sn65dsi8x user space access interface
    ret = platform_device_register(&sn65dsi8x_user_space_device);
    if (ret) {
        printk("****[sn65dsi8x_init] Unable to device register(%d)\n", ret);
        return ret;
    }    
    ret = platform_driver_register(&sn65dsi8x_user_space_driver);
    if (ret) {
        printk("****[sn65dsi8x_init] Unable to register driver (%d)\n", ret);
        return ret;
    }
    
    return 0;        
}

static void __exit sn65dsi8x_exit(void)
{
    i2c_del_driver(&sn65dsi8x_driver);
}

module_init(sn65dsi8x_init);
module_exit(sn65dsi8x_exit);
   
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C sn65dsi8x Driver");
MODULE_AUTHOR("jet.chen<chenguangjian@huaqin.com>");
