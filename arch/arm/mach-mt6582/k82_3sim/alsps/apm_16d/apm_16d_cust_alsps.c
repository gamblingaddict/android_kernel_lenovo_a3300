#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
//#include <mach/mt6577_pm_ldo.h>
#if defined(CONFIG_MTK_AUTO_DETECT_ALSPS)
static struct alsps_hw apm_16d_cust_alsps_hw = {
    .i2c_num    = 2,
    //.polling_mode = 0,
    .polling_mode_ps =0,//1     
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},
    /*Lenovo-sw chenlj2 add 2011-06-03,modify parameter below two lines*/
    //.als_level  = {5,  40,  80,   120,   160,  250,  400,  800, 1200,  1600,  2000,  3000,  5000,  10000, 65535},
    //.als_value  = {10, 40,  40,   120,   120,  280,  280,  280, 1600,  1600,  1600,  6000,  6000,  9000,  10240},

    .als_level  = { 4, 10,  20,  80,  120,  160, 250, 600,   800,  1200, 2000, 3000, 4000, 10000, 65535},
    .als_value  = {10, 40,  40,  280, 280,  280, 1600, 1600,  1600, 4000, 4000, 6000, 6000, 9000,  10240},
#if defined(AGOLD_DEFINED_APM_16D_PS_THD_VALUE)
	.ps_threshold_high = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
    .ps_threshold_low = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
    .ps_threshold = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
#else
    .ps_threshold_high = 70,
    .ps_threshold_low = 70,
    .ps_threshold = 45,
#endif
};
struct alsps_hw *apm_16d_get_cust_alsps_hw(void) {
    return &apm_16d_cust_alsps_hw;
}
#else
static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
    //.polling_mode = 0,
    .polling_mode_ps =0,//1     
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},
    /*Lenovo-sw chenlj2 add 2011-06-03,modify parameter below two lines*/
    .als_level  = {5,  40,  80,   120,   160,  250,  400,  800, 1200,  1600,  2000,  3000,  5000,  10000, 65535},
    .als_value  = {10, 40,  40,   120,   120,  280,  280,  280, 1600,  1600,  1600,  6000,  6000,  9000,  10240},
	#if defined(AGOLD_DEFINED_APM_16D_PS_THD_VALUE)
	.ps_threshold_high = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
    .ps_threshold_low = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
    .ps_threshold = AGOLD_DEFINED_APM_16D_PS_THD_VALUE,
	#else
    .ps_threshold_high = 95,
    .ps_threshold_low = 45,
    .ps_threshold = 45,
	#endif
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
#endif
