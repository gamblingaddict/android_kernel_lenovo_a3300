/*****************************************************************************
*                E X T E R N A L      R E F E R E N C E S
******************************************************************************
*/
#include <asm/uaccess.h>
#include <linux/xlog.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "yusu_android_speaker.h"
#include <mach/mt_gpio.h>

/*****************************************************************************
*                          DEBUG INFO
******************************************************************************
*/

static bool eamp_log_on = true;

#define EAMP_PRINTK(fmt, arg...) \
	do { \
		if (eamp_log_on) xlog_printk(ANDROID_LOG_INFO,"EAMP", "[EAMP]: %s() "fmt"\n", __func__,##arg); \
	}while (0)


/*****************************************************************************
*				For I2C defination
******************************************************************************
*/

// device address
#define EAMP_SLAVE_ADDR_WRITE    0xB0
#define EAMP_SLAVE_ADDR_READ      0xB1
#define EAMP_I2C_DEVNAME "TPA2028D1"
#define SOUND_I2C_CHANNEL 2
#define USE_ANALOG_SWITCH 0

//define registers
#define EAMP_REG_IC_FUNCTION_CONTROL             0x01
#define EAMP_REG_AGC_ATTACK_CONTROL	              0x02
#define EAMP_REG_AGC_RELEASE_CONTROL             0x03
#define EAMP_REG_AGC_HOLD_TIME_CONTROL        0x04
#define EAMP_REG_AGC_FIXED_GAIN_CONTROL        0x05
#define EAMP_REG_AGC_CONTROL1                          0x06
#define EAMP_REG_AGC_CONTROL2                          0x07

// I2C variable
static struct i2c_client *new_client = NULL;

// new I2C register method
static const struct i2c_device_id eamp_i2c_id[] = {{EAMP_I2C_DEVNAME,0},{}};
static struct i2c_board_info __initdata  eamp_dev={I2C_BOARD_INFO(EAMP_I2C_DEVNAME,(EAMP_SLAVE_ADDR_WRITE>>1))};

//function declration
//static int eamp_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int eamp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int eamp_i2c_remove(struct i2c_client *client);
//i2c driver
struct i2c_driver eamp_i2c_driver = {
    .probe = eamp_i2c_probe,
    .remove = eamp_i2c_remove,
    //.detect = eamp_i2c_detect,
    .driver = {
        .name =EAMP_I2C_DEVNAME,
    },
    .id_table = eamp_i2c_id,
    //.address_data = &addr_data,
};

// speaker, earpiece, headphone status and path;
static bool gsk_on = false;
static bool gep_on = false;
static bool ghp_on = false;

//mode
static u32 gMode	 = 0;
static u32 gPreMode  = 0; 

//kernal to open speaker
static bool gsk_forceon = false;
static bool gsk_preon   = false;
static bool gep_preon   = false;

//AGC fixed gain
static u8 gAgc_fixed_gain_audio = 0x12;    //18dB
static u8 gAgc_fixed_gain_voice = 0x12;    //18dB
static u8 gAgc_fixed_gain_dual = 0x18;    //24dB

//AGC control
static u8 gAgc_compression_rate = 0x00;    //1:1
static u8 gAgc_output_limiter_disable = 0x01;    //disable

// function implementation

//read one register
ssize_t static eamp_read_byte(u8 addr, u8 *returnData)
{
    char cmd_buf[1]={0x00};
    char readData = 0;
    int ret=0;

    if(!new_client)
    {
        EAMP_PRINTK("I2C client not initialized!!");
        return -1;
    }

    cmd_buf[0] = addr;

    ret = i2c_master_send(new_client, &cmd_buf[0], 1);
    if (ret < 0) {
        EAMP_PRINTK("read sends command error!!");
        return -1;
    }
    ret = i2c_master_recv(new_client, &readData, 1);
    if (ret < 0) {
        EAMP_PRINTK("reads recv data error!!");
        return -1;
    }
    *returnData = readData;
    EAMP_PRINTK("addr 0x%x data 0x%x",addr, readData);
    return 0;
}

//read one register
static u8 I2CRead(u8 addr)
{
    u8 regvalue = 0;
    eamp_read_byte(addr,&regvalue);
    return regvalue;
}

// write register
static ssize_t I2CWrite(u8 addr, u8 writeData)
{
    char    write_data[2] = {0};
    int    ret=0;

    if(!new_client)
    {
        EAMP_PRINTK("I2C client not initialized!!");
        return -1;
    }

    write_data[0] = addr;		  // ex. 0x01
    write_data[1] = writeData;

    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) {
        EAMP_PRINTK("write sends command error!!");
        return -1;
    }
    EAMP_PRINTK("addr 0x%x data 0x%x",addr,writeData);
    return 0;
}

#if 0
//write register
static ssize_t eamp_write_byte(u8 addr, u8 writeData)
{
    char    write_data[2] = {0};
    int    ret=0;

    if(!new_client)
    {
        EAMP_PRINTK("I2C client not initialized!!");
        return -1;
    }	

    write_data[0] = addr;		  // ex. 0x01
    write_data[1] = writeData;
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0) {
        EAMP_PRINTK("write sends command error!!");
        return -1;
    }
    EAMP_PRINTK("addr 0x%x data 0x%x",addr,writeData);
    return 0;
}
#endif

/*****************************************************************************
*                  F U N C T I O N        D E F I N I T I O N
******************************************************************************
*/
extern void Yusu_Sound_AMP_Switch(BOOL enable);

static ssize_t eamp_openEarpiece(void)
{
    EAMP_PRINTK("eamp_openEarpiece");

    #if USE_ANALOG_SWITCH
    mt_set_gpio_out(GPIO_AUDIO_SEL, GPIO_OUT_ONE);
    #endif

    gep_on=true;
    return 0;
}

static ssize_t eamp_closeEarpiece(void)
{
    EAMP_PRINTK("eamp_closeEarpiece");

    #if USE_ANALOG_SWITCH
    mt_set_gpio_out(GPIO_AUDIO_SEL, GPIO_OUT_ZERO);
    #endif

    gep_on=false;
    return 0;
}

static ssize_t eamp_openheadPhone(void)
{
    EAMP_PRINTK("eamp_openheadPhone");

    mt_set_gpio_out(GPIO_HP_AMP_EN, GPIO_OUT_ONE);
    ghp_on = true;
    return 0;
}

static ssize_t eamp_closeheadPhone(void)
{
    EAMP_PRINTK("eamp_closeheadPhone");

    mt_set_gpio_out(GPIO_HP_AMP_EN, GPIO_OUT_ZERO);
    ghp_on = false;
    return 0;
}

static ssize_t eamp_openspeaker(void)
{
    EAMP_PRINTK("gMode=%d", gMode);

    msleep(5);
    mt_set_gpio_out(GPIO_SPK_AMP_EN, GPIO_OUT_ONE);
    msleep(100);

    I2CWrite(EAMP_REG_IC_FUNCTION_CONTROL, 0xE3);
    I2CWrite(EAMP_REG_AGC_ATTACK_CONTROL, 0x05);
    I2CWrite(EAMP_REG_AGC_RELEASE_CONTROL, 0x0B);
    I2CWrite(EAMP_REG_AGC_HOLD_TIME_CONTROL, 0x00);
    if (gMode == 2)    //MODE_IN_CALL
    {
        I2CWrite(EAMP_REG_AGC_FIXED_GAIN_CONTROL, gAgc_fixed_gain_voice);
    }
    else if (gMode == 1)    //MODE_RINGTONE
    {
        I2CWrite(EAMP_REG_AGC_FIXED_GAIN_CONTROL, gAgc_fixed_gain_dual);
    }
    else
    {
        I2CWrite(EAMP_REG_AGC_FIXED_GAIN_CONTROL, gAgc_fixed_gain_audio);
    }
    I2CWrite(EAMP_REG_AGC_CONTROL1, 0x3A|(gAgc_output_limiter_disable << 7));
    I2CWrite(EAMP_REG_AGC_CONTROL2, 0xC0|gAgc_compression_rate);
    I2CWrite(EAMP_REG_IC_FUNCTION_CONTROL, 0xC3);

    gsk_on = true;
    return 0;
}

static ssize_t eamp_closespeaker(void)
{
    EAMP_PRINTK("eamp_closespeaker");

    mt_set_gpio_out(GPIO_SPK_AMP_EN, GPIO_OUT_ZERO);
    gsk_on = false;
    return 0;
}

static ssize_t eamp_resetRegister(void)
{
    EAMP_PRINTK("eamp_resetRegister");

    eamp_closespeaker();
    eamp_closeheadPhone();
    return 0;
}

static ssize_t eamp_suspend(void)
{
    EAMP_PRINTK("eamp_suspend");

    eamp_resetRegister();
    return 0;
}

static ssize_t eamp_resume(void)
{
    EAMP_PRINTK("eamp_resume");

    if(gsk_on)
    {
        eamp_openspeaker();
    }
    if(ghp_on)
    {
        eamp_openheadPhone();
    }
    if(gep_on)
    {
        eamp_openEarpiece();
    }
    return 0;
}

static ssize_t eamp_getRegister(unsigned int regName)
{
    EAMP_PRINTK("Regname=%u",regName);

    if(regName >7)
        return -1;
    return I2CRead(regName);
}

static ssize_t eamp_setRegister(unsigned long int param)
{
    AMP_Control * p = (AMP_Control*)param;

    EAMP_PRINTK("eamp_setRegister");

    if(p->param1 >7)
        return -1;

    return I2CWrite(p->param1,p->param2);
}

static ssize_t eamp_setMode(unsigned long int param)
{
    EAMP_PRINTK("mode(%u)",param);

    gMode = param;
    return 0;
}

static int eamp_command(unsigned int type, unsigned long args, unsigned int count)
{
    EAMP_PRINTK("type(%u)",type);
    switch(type)
    {
        case EAMP_SPEAKER_CLOSE:
        {
            eamp_closespeaker();
            break;
        }
        case EAMP_SPEAKER_OPEN:
        {
            eamp_openspeaker();
            break;
        }
        case EAMP_HEADPHONE_CLOSE:
        {
            eamp_closeheadPhone();
            break;
        }
        case EAMP_HEADPHONE_OPEN:
        {
            eamp_openheadPhone();
            break;
        }
        case EAMP_EARPIECE_OPEN:
        {
            eamp_openEarpiece();
            break;
        }
        case EAMP_EARPIECE_CLOSE:
        {
            eamp_closeEarpiece();
            break;
        }
        case EAMP_GETREGISTER_VALUE:
        {
            return eamp_getRegister(args);
            break;
        }
        case EAMP_GETAMP_GAIN:
        {
            //return eamp_getGainVolume();
            break;
        }
        case EAMP_SETAMP_GAIN:
        {
            //eamp_changeGainVolume(args);
            break;
        }
        case EAMP_SETREGISTER_VALUE:
        {
            eamp_setRegister(args);
            break;
        }
        case EAMP_GET_CTRP_NUM:
        {
            //return eamp_getCtrlPointNum();
            break;
        }
        case EAMP_GET_CTRP_BITS:
        {
            //return eamp_getCtrPointBits(args);
            break;
        }
        case EAMP_GET_CTRP_TABLE:
        {
            //eamp_getCtrlPointTable(args);
            break;
        }
        case EAMP_SETMODE:
        {
            eamp_setMode(args);
            break;
        }
        default:
            EAMP_PRINTK("Not support command=%d", type);
            return 0;
    }	
    return 0;
}

int Audio_eamp_command(unsigned int type, unsigned long args, unsigned int count)
{
    return eamp_command(type,args,count);
}

#if 0
static int eamp_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
    strcpy(info->type, EAMP_I2C_DEVNAME);
    return 0;
}
#endif

static int eamp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    EAMP_PRINTK("eamp_i2c_probe");

    new_client = client;
    eamp_resetRegister();
    EAMP_PRINTK("client=%x !!",client);
    return 0;
}

static int eamp_i2c_remove(struct i2c_client *client)
{
    EAMP_PRINTK("eamp_i2c_remove");

    new_client = NULL;
    i2c_unregister_device(client);
    i2c_del_driver(&eamp_i2c_driver);
    return 0;
}

static void eamp_poweron(void)
{
    EAMP_PRINTK("eamp_poweron");
    return;
}

static void eamp_powerdown(void)
{
    EAMP_PRINTK("eamp_powerdown");

    eamp_closeheadPhone();
    eamp_closespeaker();
    return;
}

static int eamp_init(void)
{
    EAMP_PRINTK("eamp_init");

    eamp_poweron();

    mt_set_gpio_mode(GPIO_HP_AMP_EN, GPIO_HP_AMP_EN_M_GPIO);
    mt_set_gpio_pull_enable(GPIO_HP_AMP_EN, GPIO_PULL_ENABLE);
    mt_set_gpio_dir(GPIO_HP_AMP_EN, GPIO_DIR_OUT);

    mt_set_gpio_mode(GPIO_SPK_AMP_EN, GPIO_SPK_AMP_EN_M_GPIO);
    mt_set_gpio_pull_enable(GPIO_SPK_AMP_EN, GPIO_PULL_ENABLE);
    mt_set_gpio_dir(GPIO_SPK_AMP_EN, GPIO_DIR_OUT);

    return 0;
}

static int eamp_deinit(void)
{
    EAMP_PRINTK("eamp_deinit");

    eamp_powerdown();
    return 0;
}

static int eamp_register(void)
{
    EAMP_PRINTK("eamp_register");

    i2c_register_board_info(SOUND_I2C_CHANNEL,&eamp_dev,1);
    if (i2c_add_driver(&eamp_i2c_driver)){
        EAMP_PRINTK("fail to add device into i2c");
        return -1;
    }
    return 0;
}

bool Speaker_Init(void)
{
    EAMP_PRINTK("Speaker_Init");

    eamp_init();
    return true;
}

bool Speaker_Register(void)
{
    EAMP_PRINTK("Speaker_Register");

    eamp_register();
    return true;
}

int ExternalAmp(void)
{
    return 1;
}

void Sound_Speaker_Turnon(int channel)
{
    EAMP_PRINTK("channel = %d",channel);
    eamp_command(EAMP_SPEAKER_OPEN,channel,1);
}

void Sound_Speaker_Turnoff(int channel)
{
    EAMP_PRINTK("channel = %d",channel);
    eamp_command(EAMP_SPEAKER_CLOSE,channel,1);
}

void Sound_Speaker_SetVolLevel(int level)
{
    EAMP_PRINTK("level = %d",level);
}

void Sound_Headset_Turnon(void)
{
    EAMP_PRINTK("Sound_Headset_Turnon");
    eamp_command(EAMP_HEADPHONE_OPEN,0,1);
}

void Sound_Headset_Turnoff(void)
{
    EAMP_PRINTK("Sound_Headset_Turnoff");
    eamp_command(EAMP_HEADPHONE_CLOSE,0,1);
}

//kernal use
void AudioAMPDevice_Suspend(void)
{
    EAMP_PRINTK("AudioAMPDevice_Suspend");
    eamp_suspend();
}

void AudioAMPDevice_Resume(void)
{
    EAMP_PRINTK("AudioAMPDevice_Resume");
    eamp_resume();
}

// for AEE beep sound
void AudioAMPDevice_SpeakerLouderOpen(void)
{
    EAMP_PRINTK("AudioAMPDevice_SpeakerLouderOpen");

    if(gsk_on && gMode != 2) //speaker on and not incall mode
        return;
    gsk_forceon = true;
    gPreMode = gMode;
    gsk_preon = gsk_on;
    gep_preon = gep_on;
    if(gsk_on)
    {
        eamp_closespeaker();
    }
    gMode = 0;
    eamp_openspeaker();
    return ;
}

// for AEE beep sound
void AudioAMPDevice_SpeakerLouderClose(void)
{
    EAMP_PRINTK("AudioAMPDevice_SpeakerLouderClose");

    if(gsk_forceon)
    {
        eamp_closespeaker();
        gMode = gPreMode;
        if(gep_preon)
        {
            eamp_openEarpiece();
        }
        else if(gsk_preon)
        {
            eamp_openspeaker();
        }
    }
    gsk_forceon = false;
}

// mute device when INIT_DL1_STREAM
void AudioAMPDevice_mute(void)
{
    EAMP_PRINTK("AudioAMPDevice_mute");

    if(ghp_on)
        eamp_closeheadPhone();
    if(gsk_on)
        eamp_closespeaker();
    // now not control earpiece.
}

bool Speaker_DeInit(void)
{
    EAMP_PRINTK("Speaker_DeInit");

    eamp_deinit();
    return true;
}

static char *ExtFunArray[] =
{
    "InfoMATVAudioStart",
    "InfoMATVAudioStop",
    "End",
};

kal_int32 Sound_ExtFunction(const char* name, void* param, int param_size)
{
    int i = 0;
    int funNum = -1;

    //Search the supported function defined in ExtFunArray
    while(strcmp("End",ExtFunArray[i]) != 0 ) {		//while function not equal to "End"

        if (strcmp(name,ExtFunArray[i]) == 0 ) {		//When function name equal to table, break
            funNum = i;
            break;
        }
        i++;
    }

    switch (funNum) {
        case 0:			//InfoMATVAudioStart
            printk("InfoMATVAudioStart");
            break;

        case 1:			//InfoMATVAudioStop
            printk("InfoMATVAudioStop");
            break;

        default:
            break;
    }
    return 1;
}

