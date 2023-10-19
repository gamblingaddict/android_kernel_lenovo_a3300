#include <linux/types.h>
#include <cust_acc.h>
#include <mach/mt_pm_ldo.h>


/*---------------------------------------------------------------------------*/
/*you must set the type of cust_acc_power function to static when project need be compatible with multiple gsensor*/
static int cust_acc_power(struct acc_hw *hw, unsigned int on, char* devname) //wangliangfeng 20130702
{
    if (hw->power_id == MT65XX_POWER_NONE)
        return 0;
    if (on)
        return hwPowerOn(hw->power_id, hw->power_vol, devname);
    else
        return hwPowerDown(hw->power_id, devname); 
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static struct acc_hw cust_acc_hw = {
    .i2c_num = 2,
    .direction = 7,
    .power_id = MT65XX_POWER_NONE,  /*!< LDO is not used */
    .power_vol= VOL_DEFAULT,        /*!< LDO is not used */
    .firlen = 0, //old value 16                /*!< don't enable low pass fileter */
//    .power = cust_acc_power,
};
/*---------------------------------------------------------------------------*/
struct acc_hw* get_cust_bma222e_acc_hw(void) 
{
    return &cust_acc_hw;
}
