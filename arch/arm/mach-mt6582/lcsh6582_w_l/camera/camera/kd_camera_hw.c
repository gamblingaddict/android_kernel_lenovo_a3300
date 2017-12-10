#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif

u32 pinSetIdx = 0;//default main sensor

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4

#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3

u32 pinSet[2][8] = {
        //for main sensor
        {GPIO_CAMERA_CMRST_PIN,
            GPIO_CAMERA_CMRST_PIN_M_GPIO,   /* mode */
            GPIO_OUT_ONE,                   /* ON state */
            GPIO_OUT_ZERO,                  /* OFF state */
         GPIO_CAMERA_CMPDN_PIN,
            GPIO_CAMERA_CMPDN_PIN_M_GPIO,
            GPIO_OUT_ONE,
            GPIO_OUT_ZERO,
        },
        //for sub sensor
        {GPIO_CAMERA_CMRST1_PIN,
         GPIO_CAMERA_CMRST1_PIN_M_GPIO,
            GPIO_OUT_ONE,
            GPIO_OUT_ZERO,
         GPIO_CAMERA_CMPDN1_PIN,
            GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
            GPIO_OUT_ONE,
            GPIO_OUT_ZERO,
        },
 	};

static void disable_inactive_sensor(void)
{
    //disable inactive sensor
    if (GPIO_CAMERA_INVALID != pinSet[1-pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST],pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF]))
        {
            PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");
        } //low == reset sensor
        
        if(mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN],pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF]))
        {
            PK_DBG("[CAMERA LENS] set gpio failed!! \n");
        } //high == power down lens module
    }        
}

static int kd_poweron_sub_devices(MT65XX_POWER_VOLTAGE VOL_D2, MT65XX_POWER_VOLTAGE VOL_A, MT65XX_POWER_VOLTAGE VOL_D, MT65XX_POWER_VOLTAGE VOL_A2, char *mode_name)
{
    int ret = 0;

    if(VOL_D2 >= 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_D2, mode_name);

    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D2\n");
        goto poweronerr0;
    }

    if(VOL_A > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_A,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A\n");
        goto poweronerr1;
    }

    /*if(VOL_D > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_D_SUB, VOL_D,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_D\n");
        goto poweronerr2;
    }
    */

    if(VOL_A2 > 0)
        ret = hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_A2,mode_name);
    if(ret != TRUE){
        PK_DBG("[CAMERA SENSOR] Fail to enable digital power VCAM_A2\n");
        goto poweronerr3;
    }

poweronerr3:
poweronerr2:
poweronerr1:
poweronerr0:
    return ret;
}

static int kd_powerdown_sub_devices(char *mode_name)
{
    int ret = 0;
    
/*  ret = hwPowerDown(CAMERA_POWER_VCAM_D_SUB, mode_name);

    if(TRUE != ret ) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }
*/  
    ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
    if(TRUE != ret) {
        PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }

    ret = hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name);
    if(TRUE != ret)
    {
        PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
        //return -EIO;
        goto _kd_powerdown_sub_exit_;
    }

_kd_powerdown_sub_exit_:
    return ret;
}


/****************************gc0329**********************************/ 
static int kd_gc0329_poweron( char *mode_name)
{
    int ret;
    printk("statr to run kd_gc0329_poweron! %d\n\n",pinSetIdx);
        ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800,0/*VOL_2800*/, mode_name);
    
    /*ergate-008*/
    mdelay(5);// wait power to be stable  
    disable_inactive_sensor();//disable inactive sensor

    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
     
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;

}
static int kd_gc0329yuv_powerdown( char *mode_name)
{
    int ret;
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        }
    ret = kd_powerdown_sub_devices(mode_name);
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {

    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }

    return ret;
}

/****************************gc2235**********************************/
static int kd_gc2235mipiraw_poweron( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235_poweron] start,pinSetIdx:%d\n",pinSetIdx);
    //ret = kd_poweron_sub_devices(VOL_1800, VOL_2800, VOL_1800, 0, mode_name);

        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name);
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
        ret = hwPowerOn(MT6323_POWER_LDO_VGP3, VOL_1800,mode_name);

    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}

static int kd_gc2235mipiraw_powerdown( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

    //ret = kd_powerdown_sub_devices(mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
        ret = hwPowerDown(MT6323_POWER_LDO_VGP3,mode_name);
       
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}

//niyangadd
static int kd_gc2235raw_poweron( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235raw_poweron] start,pinSetIdx:%d\n",pinSetIdx);
 
        ret = hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800, mode_name);
        ret = hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name);
        ret = hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800, mode_name);

    mdelay(5);// wait power to be stable  
    //disable_inactive_sensor();//disable inactive sensor
    
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    mdelay(1);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(10);
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
        mdelay(5);
    }
poweronerr:
    return ret;
}

static int kd_gc2235raw_powerdown( char *mode_name)
{
    int ret;

    PK_DBG("[kd_gc2235_powerdown] start,pinSetIdx:%d\n",pinSetIdx);

 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
      }

        ret = hwPowerDown(CAMERA_POWER_VCAM_D2, mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_A,mode_name);
        ret = hwPowerDown(CAMERA_POWER_VCAM_D, mode_name);
       
    if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
        //PDN pin
        if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
    }
poweronerr:
    return ret;
}
//niyangend

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{
    if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
		if(currSensorName && 
		  (0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI_RAW,currSensorName))// for main camera(huaquan) 
		  ||(0 == strcmp(SENSOR_DRVNAME_GC2235_ST_MIPI_RAW,currSensorName))//for main camera(shengtai)
                  ||(0 == strcmp(SENSOR_DRVNAME_GC2235_RAW,currSensorName))//niyangadd for main camera(huaquan1990)
		  )
          	pinSetIdx = 0;
		else{
	        PK_DBG("kdCISModulePowerOn main get in---  sensorIdx not compare with sensro ++currSensorName=%s\n",currSensorName);
	        goto _kdCISModulePowerOn_exit_;
    	}
		
    }
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
		if(currSensorName && 
		  (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName))) //wangliangfeng 20140304 added for sub camera
		  	pinSetIdx = 1;
		else
    	{
	        PK_DBG("kdCISModulePowerOn sub get in ---  sensorIdx not compare with sensro ++currSensorName=%s\n",currSensorName);
	        goto _kdCISModulePowerOn_exit_;
    	}
    }

   
    //power ON
    if (On) {
		
		PK_DBG("kdCISModulePowerOn -on:currSensorName=%s;\n",currSensorName);
		PK_DBG("kdCISModulePowerOn -on:pinSetIdx=%d\n",pinSetIdx);
		
		if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI_RAW,currSensorName)))
        {	//wangliangfeng add for huaquan module
         PK_DBG("is gc2235 on\n");
            if(TRUE != kd_gc2235mipiraw_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_ST_MIPI_RAW,currSensorName)))
        {	//wangliangfeng add for shengtai module
         PK_DBG("is gc2236 on for shengtai module.\n");
            if(TRUE != kd_gc2235mipiraw_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_RAW,currSensorName)))//niyangadd
        {	
         PK_DBG("is gc2235 on for huaquan1990 module.\n");
            if(TRUE != kd_gc2235raw_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName)))          
        {  //wangliangfeng 20140304 added for sub camera
         	PK_DBG("is gc0329 on!\n");
            if(TRUE != kd_gc0329_poweron(mode_name))
                 goto _kdCISModulePowerOn_exit_;
        }else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW, currSensorName)))
		{
			 //enable active sensor
			 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				 //RST pin,active low
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				 mdelay(5);
		 
				 //PDN pin,high
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				 mdelay(5);
			 }
		 
			 //DOVDD
			 PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //AVDD
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
			 {
				 PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				 //return -EIO;
				 //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //DVDD
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
			 {
				  PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
				  //return -EIO;
				  //goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
			 
			 //AF_VCC
			 if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
			 {
				  PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
				  //return -EIO;
				  goto _kdCISModulePowerOn_exit_;
			 }
			 mdelay(5);
		 
			 //enable active sensor
			 if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
				 //RST pin
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
				 mdelay(5);
		 
				 //PDN pin
				 if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
				 if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
				 if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
				 mdelay(5);
			 }

#if 1
			 //disable inactive sensor
			 if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
				 if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
					 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
					 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
					 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
					 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
				 }
			 }
			 else {
				 if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
					 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
					 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
					 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
					 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
					 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
				 }
			 }
#endif
		 }
		else if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV2659_YUV, currSensorName)))
   		 {
        //enable active sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            //RST pin,active low
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(5);

            //PDN pin,high
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
            mdelay(5);
        }

        //DOVDD
        PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

        //AVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

        //DVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
        {
             PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
             //return -EIO;
             //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(5);

		#if 0
       // AF_VCC
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
       {
             PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
             //return -EIO;
             goto _kdCISModulePowerOn_exit_;
        }
        #endif     

		//enable active sensor
		if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
			//RST pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
			mdelay(5);
		
			//PDN pin
			if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
			mdelay(5);
		}


	 //disable inactive sensor
	 if(pinSetIdx == 0 || pinSetIdx == 2) {//disable sub
		 if (GPIO_CAMERA_INVALID != pinSet[1][IDX_PS_CMRST]) {
			 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			 if(mt_set_gpio_mode(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[1][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMRST],pinSet[1][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			 if(mt_set_gpio_out(pinSet[1][IDX_PS_CMPDN],pinSet[1][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
		 }
	 }
	 else {
		 if (GPIO_CAMERA_INVALID != pinSet[0][IDX_PS_CMRST]) {
			 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
			 if(mt_set_gpio_mode(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
			 if(mt_set_gpio_dir(pinSet[0][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
			 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMRST],pinSet[0][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
			 if(mt_set_gpio_out(pinSet[0][IDX_PS_CMPDN],pinSet[0][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
		 }
	 }


	 }
        else
		{
		if( !(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K3H7Y_MIPI_RAW,currSensorName))))
        {
        //enable active sensor
        //RST pin
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);

            //PDN pin
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        }
        }
        //DOVDD
        if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV3640_YUV,currSensorName)))
        {
            PK_DBG("[ON_OV3640YUV case 1.5V]sensorIdx:%d \n",SensorIdx);
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
        }
        else //general case on
        {
            PK_DBG("[ON_general 1.8V]sensorIdx:%d \n",SensorIdx);
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
            {
                PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                //return -EIO;
                //goto _kdCISModulePowerOn_exit_;
            }
        }
        mdelay(10);
        //AVDD
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            //goto _kdCISModulePowerOn_exit_;
        }
        mdelay(10);
        //DVDD
        if ((currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV,currSensorName))) ||
			(currSensorName && (0 == strcmp(SENSOR_DRVNAME_S5K3H7Y_MIPI_RAW,currSensorName)))  	) {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1200,mode_name))
        {
             PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
             //return -EIO;
             //goto _kdCISModulePowerOn_exit_;
        }
        }
        else {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name))
        {
             PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
             //return -EIO;
             //goto _kdCISModulePowerOn_exit_;
        }
        }            
            
            
        mdelay(10);


        //AF_VCC
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            //return -EIO;
            goto _kdCISModulePowerOn_exit_;
        }




#if 1
        //enable active sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(10);
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            mdelay(1);

            //PDN pin
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_ON])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");}
        }
#endif

    	}
    }
    else {//power OFF
		PK_DBG("kdCISModulePowerOn -off:currSensorName=%s\n",currSensorName);
		PK_DBG("kdCISModulePowerOn -off:pinSetIdx=%d\n",pinSetIdx);
		
#if 0 //TODO: depends on HW layout. Should be notified by SA.
        PK_DBG("Set GPIO 94 for power OFF\n");
        if (mt_set_gpio_pull_enable(GPIO_CAMERA_LDO_EN_PIN, GPIO_PULL_DISABLE)) {PK_DBG("[CAMERA SENSOR] Set GPIO94 PULL DISABLE ! \n"); }
        if(mt_set_gpio_mode(GPIO_CAMERA_LDO_EN_PIN, GPIO_CAMERA_LDO_EN_PIN_M_GPIO)){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
        if(mt_set_gpio_dir(GPIO_CAMERA_LDO_EN_PIN,GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
        if(mt_set_gpio_out(GPIO_CAMERA_LDO_EN_PIN,GPIO_OUT_ZERO)){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
#endif
		if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_MIPI_RAW,currSensorName)))
		{	//wangliangfeng add for huaquan module 
			PK_DBG("is gc2235 down\n");
			if(TRUE != kd_gc2235mipiraw_powerdown(mode_name))
				goto _kdCISModulePowerOn_exit_;
		}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_ST_MIPI_RAW,currSensorName)))
		{	//wangliangfeng add for shengtai module 
			PK_DBG("is gc2236 down for shengtai module.\n");
			if(TRUE != kd_gc2235mipiraw_powerdown(mode_name))
			goto _kdCISModulePowerOn_exit_;
	   	}
               else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC2235_RAW,currSensorName)))//niyangadd
		{	
			PK_DBG("is gc2235 down for huaquan1990 module.\n");
			if(TRUE != kd_gc2235raw_powerdown(mode_name))
			goto _kdCISModulePowerOn_exit_;
	   	}
		else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_GC0329_YUV,currSensorName)))          
	    {	//wangliangfeng 20140304 added for sub camera
	    	PK_DBG("is gc0329 down\n");
	       	if(TRUE != kd_gc0329yuv_powerdown(mode_name))
	           goto _kdCISModulePowerOn_exit_;
		}else{ 
	        //PK_DBG("[OFF]sensorIdx:%d \n",SensorIdx);
	        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
	            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
	            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_MODE])){PK_DBG("[CAMERA LENS] set gpio mode failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
	            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMPDN],GPIO_DIR_OUT)){PK_DBG("[CAMERA LENS] set gpio dir failed!! \n");}
	            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
	    	    if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN],pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF])){PK_DBG("[CAMERA LENS] set gpio failed!! \n");} //high == power down lens module
	        }

	    	if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
	            PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
	            //return -EIO;
	            goto _kdCISModulePowerOn_exit_;
	        }
			
			if (currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5647MIPI_RAW, currSensorName)))
			{
				if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
				{
					PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
					//return -EIO;
					goto _kdCISModulePowerOn_exit_;
				}
			}
			
	        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
	            PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
	            //return -EIO;
	            goto _kdCISModulePowerOn_exit_;
	        }
	        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
	        {
	            PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
	            //return -EIO;
	            goto _kdCISModulePowerOn_exit_;
	        }

			//For Power Saving
			if(pinSet[pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF] != GPIO_OUT_ZERO)
			{
				mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMPDN], GPIO_OUT_ZERO);
			}
			if(pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF] != GPIO_OUT_ZERO)
			{
				mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO);
			}

			if(pinSet[1-pinSetIdx][IDX_PS_CMPDN+IDX_PS_OFF] != GPIO_OUT_ZERO)
			{
				   mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMPDN], GPIO_OUT_ZERO);
			}
			if(pinSet[1-pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF] != GPIO_OUT_ZERO)
			{
				   mt_set_gpio_out(pinSet[1-pinSetIdx][IDX_PS_CMRST], GPIO_OUT_ZERO);
			}
	   }
		//~For Power Saving
    }//

	return 0;

_kdCISModulePowerOn_exit_:
    return -EIO;
}

EXPORT_SYMBOL(kdCISModulePowerOn);


//!--
//




