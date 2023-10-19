
#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

/* Pre-defined definition */
#define TPD_TYPE_CAPACITIVE
#define TPD_TYPE_RESISTIVE
//#define TPD_POWER_SOURCE         
#define TPD_I2C_NUMBER           0
#define TPD_WAKEUP_TRIAL         60
#define TPD_WAKEUP_DELAY         100

#define TPD_DELAY                (2*HZ/100)

#define TPD_CALIBRATION_MATRIX  {962,0,0,0,1600,0,0,0};

#define LCM_WIDTH                480
#define LCM_HEIGHT                854
#define MTK_LCM_PHYSICAL_ROTATION 0

//#define TPD_HAVE_CALIBRATION
//#define TPD_HAVE_TREMBLE_ELIMINATION

//#define TPD_HAVE_BUTTON

#define TPD_BUTTON_HEIGH 		(100)
#define TPD_KEY_COUNT 			4
#define TPD_KEYS 				{KEY_MENU,KEY_BACK,KEY_HOME,KEY_SEARCH}
#define TPD_KEYS_DIM 			{{60,LCM_HEIGHT*850/800,120,TPD_BUTTON_HEIGH},{180,LCM_HEIGHT*850/800,120,TPD_BUTTON_HEIGH},{300,LCM_HEIGHT*850/800,120,TPD_BUTTON_HEIGH}}

/* for different ctp resolution */
#define TPD_RES_X                LCM_WIDTH
#define TPD_RES_Y                LCM_HEIGHT


#endif /* TOUCHPANEL_H__ */

