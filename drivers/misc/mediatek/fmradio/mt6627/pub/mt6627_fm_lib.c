#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "osal_typedef.h"
#include "stp_exp.h"
#include "wmt_exp.h"

#include "fm_typedef.h"
#include "fm_dbg.h"
#include "fm_err.h"
#include "fm_interface.h"
#include "fm_stdlib.h"
#include "fm_patch.h"
#include "fm_utils.h"
#include "fm_link.h"
#include "fm_config.h"
#include "fm_private.h"

#include "mt6627_fm_reg.h"
#include "mt6627_fm.h"
#include "mt6627_fm_lib.h"
#include "mt6627_fm_cmd.h"
#include "mt6627_fm_cust_cfg.h"
/* #include "mach/mt_gpio.h" */
extern fm_cust_cfg mt6627_fm_config;

/* #define MT6627_FM_PATCH_PATH "/vendor/firmw/mt6627/mt6627_fm_patch.bin" */
/* #define MT6627_FM_COEFF_PATH "/vendor/firmw/mt6627/mt6627_fm_coeff.bin" */
/* #define MT6627_FM_HWCOEFF_PATH "/vendor/firmw/mt6627/mt6627_fm_hwcoeff.bin" */
/* #define MT6627_FM_ROM_PATH "/vendor/firmw/mt6627/mt6627_fm_rom.bin" */

static struct fm_patch_tbl mt6627_patch_tbl[5] = {
	{FM_ROM_V1, "/vendor/firmw/mt6627/mt6627_fm_v1_patch.bin",
	 "/vendor/firmw/mt6627/mt6627_fm_v1_coeff.bin", NULL, NULL},
	{FM_ROM_V2, "/vendor/firmw/mt6627/mt6627_fm_v2_patch.bin",
	 "/vendor/firmw/mt6627/mt6627_fm_v2_coeff.bin", NULL, NULL},
	{FM_ROM_V3, "/vendor/firmw/mt6627/mt6627_fm_v3_patch.bin",
	 "/vendor/firmw/mt6627/mt6627_fm_v3_coeff.bin", NULL, NULL},
	{FM_ROM_V4, "/vendor/firmw/mt6627/mt6627_fm_v4_patch.bin",
	 "/vendor/firmw/mt6627/mt6627_fm_v4_coeff.bin", NULL, NULL},
	{FM_ROM_V5, "/vendor/firmw/mt6627/mt6627_fm_v5_patch.bin",
	 "/vendor/firmw/mt6627/mt6627_fm_v5_coeff.bin", NULL, NULL},
};

static struct fm_hw_info mt6627_hw_info = {
	.chip_id = 0x00006627,
	.eco_ver = 0x00000000,
	.rom_ver = 0x00000000,
	.patch_ver = 0x00000000,
	.reserve = 0x00000000,
};

#define PATCH_SEG_LEN 512

static fm_u8 *cmd_buf;
static struct fm_lock *cmd_buf_lock;
static struct fm_callback *fm_cb_op;
static struct fm_res_ctx *mt6627_res;
/* static fm_s32 Chip_Version = mt6627_E1; */

/* static fm_bool rssi_th_set = fm_false; */


#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
static struct fm_fifo *cqi_fifo;
#endif
static fm_s32 mt6627_is_dese_chan(fm_u16 freq);
static fm_bool mt6627_I2S_hopping_check(fm_u16 freq);

#if 0
static fm_s32 mt6627_mcu_dese(fm_u16 freq, void *arg);
static fm_s32 mt6627_gps_dese(fm_u16 freq, void *arg);
static fm_s32 mt6627_I2s_Setting(fm_s32 onoff, fm_s32 mode, fm_s32 sample);
#endif
static fm_u16 mt6627_chan_para_get(fm_u16 freq);
static fm_s32 mt6627_desense_check(fm_u16 freq, fm_s32 rssi);
static fm_bool mt6627_TDD_chan_check(fm_u16 freq);
static fm_s32 mt6627_soft_mute_tune(fm_u16 freq, fm_s32 *rssi, fm_bool *valid);
static fm_s32 mt6627_pwron(fm_s32 data)
{
	/*//Turn on FM on 6627 chip by WMT driver
	   if(MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_LPBK)){
	   WCN_DBG(FM_ALT|CHIP,"WMT turn on LPBK Fail!\n");
	   return -FM_ELINK;
	   }else{
	   WCN_DBG(FM_ALT|CHIP,"WMT turn on LPBK OK!\n");
	   //return 0;
	   } */
	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_on(WMTDRV_TYPE_FM)) {
		WCN_DBG(FM_ALT | CHIP, "WMT turn on FM Fail!\n");
		return -FM_ELINK;
	} else {
		WCN_DBG(FM_ALT | CHIP, "WMT turn on FM OK!\n");
		return 0;
	}
}


static fm_s32 mt6627_pwroff(fm_s32 data)
{
	if (MTK_WCN_BOOL_FALSE == mtk_wcn_wmt_func_off(WMTDRV_TYPE_FM)) {
		WCN_DBG(FM_ALT | CHIP, "WMT turn off FM Fail!\n");
		return -FM_ELINK;
	} else {
		WCN_DBG(FM_NTC | CHIP, "WMT turn off FM OK!\n");
		return 0;
	}
}

static fm_s32 Delayms(fm_u32 data)
{
	WCN_DBG(FM_DBG | CHIP, "delay %dms\n", data);
	msleep(data);
	return 0;
}

static fm_s32 Delayus(fm_u32 data)
{
	WCN_DBG(FM_DBG | CHIP, "delay %dus\n", data);
	udelay(data);
	return 0;
}

fm_s32 mt6627_get_read_result(struct fm_res_ctx *result)
{
	FMR_ASSERT(result);
	mt6627_res = result;

	return 0;
}

static fm_s32 mt6627_read(fm_u8 addr, fm_u16 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_RD, SW_RETRY_CNT, FSPI_RD_TIMEOUT,
		      mt6627_get_read_result);

	if (!ret && mt6627_res) {
		*val = mt6627_res->fspi_rd;
	}

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6627_write(fm_u8 addr, fm_u16 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_FSPI_WR, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6627_set_bits(fm_u8 addr, fm_u16 bits, fm_u16 mask)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_set_bits_reg(cmd_buf, TX_BUF_SIZE, addr, bits, mask);
	ret = fm_cmd_tx(cmd_buf, pkt_size, (1 << 0x11), SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);	/* 0x11 this opcode won't be parsed as an opcode, so set here as spcial case. */
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6627_top_read(fm_u16 addr, fm_u32 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_top_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_CSPI_READ, SW_RETRY_CNT, FSPI_RD_TIMEOUT,
		      mt6627_get_read_result);

	if (!ret && mt6627_res) {
		*val = mt6627_res->cspi_rd;
	}

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6627_top_write(fm_u16 addr, fm_u32 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_top_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_CSPI_WRITE, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

/*static fm_s32 mt6627_top_set_bits(fm_u16 addr, fm_u32 bits, fm_u32 mask)
{
    fm_s32 ret = 0;
    fm_u32 val;

    ret = mt6627_top_read(addr, &val);

    if (ret)
	return ret;

    val = ((val & (mask)) | bits);
    ret = mt6627_top_write(addr, val);

    return ret;
}*/

static fm_s32 mt6627_host_read(fm_u32 addr, fm_u32 *val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_host_get_reg(cmd_buf, TX_BUF_SIZE, addr);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_READ, SW_RETRY_CNT, FSPI_RD_TIMEOUT,
		      mt6627_get_read_result);

	if (!ret && mt6627_res) {
		*val = mt6627_res->cspi_rd;
	}

	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

static fm_s32 mt6627_host_write(fm_u32 addr, fm_u32 val)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_host_set_reg(cmd_buf, TX_BUF_SIZE, addr, val);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_HOST_WRITE, SW_RETRY_CNT, FSPI_WR_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	return ret;
}

/*static fm_s32 mt6627_DSP_write(fm_u16 addr, fm_u16 val)
{
    mt6627_write(0xE2, addr);
    mt6627_write(0xE3, val);
    mt6627_write(0xE1, 0x0002);
    return 0;
}
static fm_s32 mt6627_DSP_read(fm_u16 addr, fm_u16 *val)
{
    fm_s32 ret=-1;
    mt6627_write(0xE2, addr);
    mt6627_write(0xE1, 0x0001);
    ret = mt6627_read(0xE4, val);
    return ret;
}*/
static fm_u16 mt6627_get_chipid(void)
{
	return 0x6627;
}

/*  MT6627_SetAntennaType - set Antenna type
 *  @type - 1,Short Antenna;  0, Long Antenna
 */
static fm_s32 mt6627_SetAntennaType(fm_s32 type)
{
	fm_u16 dataRead;

	WCN_DBG(FM_DBG | CHIP, "set ana to %s\n", type ? "short" : "long");
	mt6627_read(FM_MAIN_CG2_CTRL, &dataRead);

	if (type) {
		dataRead |= ANTENNA_TYPE;
	} else {
		dataRead &= (~ANTENNA_TYPE);
	}

	mt6627_write(FM_MAIN_CG2_CTRL, dataRead);

	return 0;
}

static fm_s32 mt6627_GetAntennaType(void)
{
	fm_u16 dataRead;

	mt6627_read(FM_MAIN_CG2_CTRL, &dataRead);
	WCN_DBG(FM_DBG | CHIP, "get ana type: %s\n", (dataRead & ANTENNA_TYPE) ? "short" : "long");

	if (dataRead & ANTENNA_TYPE)
		return FM_ANA_SHORT;	/* short antenna */
	else
		return FM_ANA_LONG;	/* long antenna */
}


static fm_s32 mt6627_Mute(fm_bool mute)
{
	fm_s32 ret = 0;
	fm_u16 dataRead;

	WCN_DBG(FM_DBG | CHIP, "set %s\n", mute ? "mute" : "unmute");
	/* mt6627_read(FM_MAIN_CTRL, &dataRead); */
	mt6627_read(0x9C, &dataRead);

	/* mt6627_top_write(0x0050,0x00000007); */
	if (mute == 1) {
		ret = mt6627_write(0x9C, (dataRead & 0xFFFC) | 0x0003);
	} else {
		ret = mt6627_write(0x9C, (dataRead & 0xFFFC));
	}
	/* mt6627_top_write(0x0050,0x0000000F); */

	return ret;
}


/*static fm_s32 mt6627_set_RSSITh(fm_u16 TH_long, fm_u16 TH_short)
{
    mt6627_write(0xE2, 0x3072);
    mt6627_write(0xE3, TH_long);
    mt6627_write(0xE1, 0x0002);
    Delayms(1);
    mt6627_write(0xE2, 0x307A);
    mt6627_write(0xE3, TH_short);
    mt6627_write(0xE1, 0x0002);

    WCN_DBG(FM_DBG | CHIP, "RSSI TH, long:0x%04x, short:0x%04x", TH_long, TH_short);
    return 0;
}
*/
/*
static fm_s32 mt6627_set_SMGTh(fm_s32 ver, fm_u16 TH_smg)
{
    if (mt6627_E1 == ver) {
	mt6627_write(0xE2, 0x321E);
	mt6627_write(0xE3, TH_smg);
	mt6627_write(0xE1, 0x0002);
    } else {
	mt6627_write(0xE2, 0x3218);
	mt6627_write(0xE3, TH_smg);
	mt6627_write(0xE1, 0x0002);
    }

    WCN_DBG(FM_DBG | CHIP, "Soft-mute gain TH %d\n", (int)TH_smg);
    return 0;
}
*/
static fm_s32 mt6627_RampDown(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	/* fm_u16 tmp; */

	WCN_DBG(FM_DBG | CHIP, "ramp down\n");
	/* pwer up sequence 0425 */
	ret = mt6627_top_write(0x0050, 0x00000007);	
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr top 0x50 failed\n");
		return ret;
	}
	
	ret = mt6627_set_bits(0x0F, 0x0000, 0xF800);	
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr 0x0f failed\n");
		return ret;
	}
	
	ret = mt6627_top_write(0x0050, 0x0000000F);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr top 0x50 failed\n");
		return ret;
	}
	
	/* mt6627_read(FM_MAIN_INTRMASK, &tmp); */
	ret = mt6627_write(FM_MAIN_INTRMASK, 0x0000);	
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_INTRMASK failed\n");
		return ret;
	}
	
	ret = mt6627_write(FM_MAIN_EXTINTRMASK, 0x0000);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_rampdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_RAMPDOWN, SW_RETRY_CNT, RAMPDOWN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down failed\n");
		return ret;
	}

	ret = mt6627_write(FM_MAIN_EXTINTRMASK, 0x0021);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_EXTINTRMASK failed\n");
		return ret;
	}
	
	ret = mt6627_write(FM_MAIN_INTRMASK, 0x0021);	
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "ramp down wr FM_MAIN_INTRMASK failed\n");
	}
	
	return ret;
}

static fm_s32 mt6627_get_rom_version(void)
{
	fm_u16 tmp;
	fm_s32 ret;

	/* DSP rom code version request enable --- set 0x61 b15=1 */
	mt6627_set_bits(0x61, 0x8000, 0x7FFF);

	/* Release ASIP reset --- set 0x61 b1=1 */
	mt6627_set_bits(0x61, 0x0002, 0xFFFD);

	/* Enable ASIP power --- set 0x61 b0=0 */
	mt6627_set_bits(0x61, 0x0000, 0xFFFE);

	/* Wait DSP code version ready --- wait 1ms */
	do {
		Delayus(1000);
		ret = mt6627_read(0x84, &tmp);
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		if (ret) {
			return ret;
		}
		WCN_DBG(FM_NTC | CHIP, "0x84=%x\n", tmp);
	} while (tmp != 0x0001);

	/* Get FM DSP code version --- rd 0x83[15:8] */
	mt6627_read(0x83, &tmp);
	tmp = (tmp >> 8);

	/* DSP rom code version request disable --- set 0x61 b15=0 */
	mt6627_set_bits(0x61, 0x0000, 0x7FFF);

	/* Reset ASIP --- set 0x61[1:0] = 1 */
	mt6627_set_bits(0x61, 0x0001, 0xFFFC);

	/* WCN_DBG(FM_NTC | CHIP, "ROM version: v%d\n", (fm_s32)tmp); */
	return (fm_s32) tmp;
}

static fm_s32 mt6627_get_patch_path(fm_s32 ver, const fm_s8 **ppath)
{
	fm_s32 i;
	fm_s32 max = sizeof(mt6627_patch_tbl) / sizeof(mt6627_patch_tbl[0]);

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if ((mt6627_patch_tbl[i].idx == ver)
		    && (fm_file_exist(mt6627_patch_tbl[i].patch) == 0)) {
			*ppath = mt6627_patch_tbl[i].patch;
			WCN_DBG(FM_NTC | CHIP, "Get ROM version OK\n");
			return 0;
		}
	}

	/* the ROM version isn't defined, find a latest patch instead */
	for (i = max; i > 0; i--) {
		if (fm_file_exist(mt6627_patch_tbl[i - 1].patch) == 0) {
			*ppath = mt6627_patch_tbl[i - 1].patch;
			WCN_DBG(FM_WAR | CHIP, "undefined ROM version\n");
			return 1;
		}
	}

	/* get path failed */
	WCN_DBG(FM_ERR | CHIP, "No valid patch file\n");
	return -FM_EPATCH;
}


static fm_s32 mt6627_get_coeff_path(fm_s32 ver, const fm_s8 **ppath)
{
	fm_s32 i;
	fm_s32 max = sizeof(mt6627_patch_tbl) / sizeof(mt6627_patch_tbl[0]);

	/* check if the ROM version is defined or not */
	for (i = 0; i < max; i++) {
		if ((mt6627_patch_tbl[i].idx == ver)
		    && (fm_file_exist(mt6627_patch_tbl[i].coeff) == 0)) {
			*ppath = mt6627_patch_tbl[i].coeff;
			WCN_DBG(FM_NTC | CHIP, "Get ROM version OK\n");
			return 0;
		}
	}


	/* the ROM version isn't defined, find a latest patch instead */
	for (i = max; i > 0; i--) {
		if (fm_file_exist(mt6627_patch_tbl[i - 1].coeff) == 0) {
			*ppath = mt6627_patch_tbl[i - 1].coeff;
			WCN_DBG(FM_WAR | CHIP, "undefined ROM version\n");
			return 1;
		}
	}

	/* get path failed */
	WCN_DBG(FM_ERR | CHIP, "No valid coeff file\n");
	return -FM_EPATCH;
}


/*
*  mt6627_DspPatch - DSP download procedure
*  @img - source dsp bin code
*  @len - patch length in byte
*  @type - rom/patch/coefficient/hw_coefficient
*/
static fm_s32 mt6627_DspPatch(const fm_u8 *img, fm_s32 len, enum IMG_TYPE type)
{
	fm_u8 seg_num;
	fm_u8 seg_id = 0;
	fm_s32 seg_len;
	fm_s32 ret = 0;
	fm_u16 pkt_size;

	FMR_ASSERT(img);

	if (len <= 0) {
		return -1;
	}

	seg_num = len / PATCH_SEG_LEN + 1;
	WCN_DBG(FM_NTC | CHIP, "binary len:%d, seg num:%d\n", len, seg_num);

	switch (type) {
#if 0
	case IMG_ROM:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "rom,[seg_id:%d],  [seg_len:%d]\n", seg_id, seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6627_rom_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						&img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_ROM, SW_RETRY_CNT, ROM_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ALT | CHIP, "mt6627_rom_download failed\n");
				return ret;
			}
		}

		break;
#endif
	case IMG_PATCH:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "patch,[seg_id:%d],  [seg_len:%d]\n", seg_id,
				seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6627_patch_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_PATCH, SW_RETRY_CNT, PATCH_TIMEOUT,
				      NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ALT | CHIP, "mt6627_patch_download failed\n");
				return ret;
			}
		}

		break;
#if 0
	case IMG_HW_COEFFICIENT:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "hwcoeff,[seg_id:%d],  [seg_len:%d]\n", seg_id,
				seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6627_hwcoeff_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						    &img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_HWCOEFF, SW_RETRY_CNT,
				      HWCOEFF_TIMEOUT, NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ALT | CHIP, "mt6627_hwcoeff_download failed\n");
				return ret;
			}
		}

		break;
#endif
	case IMG_COEFFICIENT:

		for (seg_id = 0; seg_id < seg_num; seg_id++) {
			seg_len = ((seg_id + 1) < seg_num) ? PATCH_SEG_LEN : (len % PATCH_SEG_LEN);
			WCN_DBG(FM_NTC | CHIP, "coeff,[seg_id:%d],  [seg_len:%d]\n", seg_id,
				seg_len);
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size =
			    mt6627_coeff_download(cmd_buf, TX_BUF_SIZE, seg_num, seg_id,
						  &img[seg_id * PATCH_SEG_LEN], seg_len);
			WCN_DBG(FM_NTC | CHIP, "pkt_size:%d\n", (fm_s32) pkt_size);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_COEFF, SW_RETRY_CNT, COEFF_TIMEOUT,
				      NULL);
			FM_UNLOCK(cmd_buf_lock);

			if (ret) {
				WCN_DBG(FM_ALT | CHIP, "mt6627_coeff_download failed\n");
				return ret;
			}
		}

		break;
	default:
		break;
	}

	return 0;
}


static fm_s32 mt6627_PowerUp(fm_u16 *chip_id, fm_u16 *device_id)
{
#define PATCH_BUF_SIZE 4096*6
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 tmp_reg = 0;
	fm_u32 host_reg = 0;

	const fm_s8 *path_patch = NULL;
	const fm_s8 *path_coeff = NULL;
	/* const fm_s8 *path_hwcoeff = NULL; */
	/* fm_s32 coeff_len = 0; */
	fm_s32 patch_len = 0;
	fm_u8 *dsp_buf = NULL;

	FMR_ASSERT(chip_id);
	FMR_ASSERT(device_id);

	WCN_DBG(FM_DBG | CHIP, "pwr on seq......\n");

	/* Wholechip FM Power Up: step 1, set common SPI parameter */
	ret = mt6627_host_write(0x8013000C, 0x0000801F);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup set CSPI failed\n");
		return ret;
	}

	ret = mt6627_host_read(0x80101030,&host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup read 0x80100030 failed\n");
		return ret;
	}
	ret = mt6627_host_write(0x80101030, host_reg|(1<<1));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwrup enable top_ck_en_adie failed\n");
		return ret;
	}
	
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_pwrup_clock_on(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_clock_on failed\n");
		return ret;
	}
/* #ifdef FM_DIGITAL_INPUT */
	/* mt6627_I2s_Setting(MT6627_I2S_ON, MT6627_I2S_MASTER, MT6627_I2S_44K); */
	/* mt_combo_audio_ctrl(COMBO_AUDIO_STATE_2); */
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2); */
/* #endif */

	/* Wholechip FM Power Up: step 2, read HW version */
	mt6627_read(0x62, &tmp_reg);
	/* *chip_id = tmp_reg; */
	if ((tmp_reg == 0x6625) || (tmp_reg == 0x6627))
		*chip_id = 0x6627;
	*device_id = tmp_reg;
	mt6627_hw_info.chip_id = (fm_s32) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "chip_id:0x%04x\n", tmp_reg);

	if ((mt6627_hw_info.chip_id != 0x6627) && (mt6627_hw_info.chip_id != 0x6625)) {
		WCN_DBG(FM_NTC | CHIP, "fm sys error, reset hw\n");
		return (-FM_EFW);
	}

	mt6627_hw_info.eco_ver = (fm_s32) mtk_wcn_wmt_hwver_get();
	WCN_DBG(FM_NTC | CHIP, "ECO version:0x%08x\n", mt6627_hw_info.eco_ver);
	mt6627_hw_info.eco_ver += 1;

	/* get mt6627 DSP rom version */
	if ((ret = mt6627_get_rom_version()) >= 0) {
		mt6627_hw_info.rom_ver = ret;
		WCN_DBG(FM_NTC | CHIP, "ROM version: v%d\n", mt6627_hw_info.rom_ver);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get ROM version failed\n");
		/* ret=-4 means signal got when control FM. usually get sig 9 to kill FM process. */
		/* now cancel FM power up sequence is recommended. */
		return ret;
	}

	/* Wholechip FM Power Up: step 3, download patch */
	if (!(dsp_buf = fm_vmalloc(PATCH_BUF_SIZE))) {
		WCN_DBG(FM_ALT | CHIP, "-ENOMEM\n");
		return -ENOMEM;
	}

	ret = mt6627_get_patch_path(mt6627_hw_info.rom_ver, &path_patch);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " mt6627_get_patch_path failed\n");
		return ret;
	}
	patch_len = fm_file_read(path_patch, dsp_buf, PATCH_BUF_SIZE, 0);
	ret = mt6627_DspPatch((const fm_u8 *)dsp_buf, patch_len, IMG_PATCH);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " DL DSPpatch failed\n");
		return ret;
	}

	ret = mt6627_get_coeff_path(mt6627_hw_info.rom_ver, &path_coeff);
	patch_len = fm_file_read(path_coeff, dsp_buf, PATCH_BUF_SIZE, 0);

	mt6627_hw_info.rom_ver += 1;

	tmp_reg = dsp_buf[38] | (dsp_buf[39] << 8);	/* to be confirmed */
	mt6627_hw_info.patch_ver = (fm_s32) tmp_reg;
	WCN_DBG(FM_NTC | CHIP, "Patch version: 0x%08x\n", mt6627_hw_info.patch_ver);

	if (ret == 1) {
		dsp_buf[4] = 0x00;	/* if we found rom version undefined, we should disable patch */
		dsp_buf[5] = 0x00;
	}

	ret = mt6627_DspPatch((const fm_u8 *)dsp_buf, patch_len, IMG_COEFFICIENT);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " DL DSPcoeff failed\n");
		return ret;
	}
	mt6627_write(0x92, 0x0000);	/* ? */
	mt6627_write(0x90, 0x0040);
	mt6627_write(0x90, 0x0000);

	if (dsp_buf) {
		fm_vfree(dsp_buf);
		dsp_buf = NULL;
	}
	/* Wholechip FM Power Up: step 4, FM Digital Init: fm_rgf_maincon */
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_pwrup_digital_init(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_digital_init failed\n");
		return ret;
	}
	/* Wholechip FM Power Up: step 5, FM RF fine tune setting */
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_pwrup_fine_tune(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrup_fine_tune failed\n");
		return ret;
	}
	/* enable connsys FM 2 wire RX */
	mt6627_write(0x9B, 0xF9AB);
	mt6627_host_write(0x80101054, 0x00003f35);

	WCN_DBG(FM_NTC | CHIP, "pwr on seq ok\n");

	return ret;
}

static fm_s32 mt6627_PowerDown(void)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 dataRead;
	fm_u32 tem;
	fm_u32 host_reg = 0;

	WCN_DBG(FM_DBG | CHIP, "pwr down seq\n");
	/*SW work around for MCUFA issue.
	 *if interrupt happen before doing rampdown, DSP can't switch MCUFA back well.
	 * In case read interrupt, and clean if interrupt found before rampdown.
	 */
	mt6627_read(FM_MAIN_INTR, &dataRead);

	if (dataRead & 0x1) {
		mt6627_write(FM_MAIN_INTR, dataRead);	/* clear status flag */
	}

	//mt6627_RampDown();

/* #ifdef FM_DIGITAL_INPUT */
/* mt6627_I2s_Setting(MT6627_I2S_OFF, MT6627_I2S_SLAVE, MT6627_I2S_44K); */
/* #endif */
	/* pwer up sequence 0425 */
	/* A0:set audio output I2X Rx mode: */
	mt6627_host_read(0x80101054, &tem);
	tem = tem & 0xFFFF9FFF;
	mt6627_host_write(0x80101054, tem);

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_pwrdown(cmd_buf, TX_BUF_SIZE);
	ret = fm_cmd_tx(cmd_buf, pkt_size, FLAG_EN, SW_RETRY_CNT, EN_TIMEOUT, NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_pwrdown failed\n");
		return ret;
	}
	/* FIX_ME, disable ext interrupt */
	mt6627_write(FM_MAIN_EXTINTRMASK, 0x00);

	
	ret = mt6627_host_read(0x80101030,&host_reg);
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwroff read 0x80100030 failed\n");
		return ret;
	}
	ret = mt6627_host_write(0x80101030, host_reg&(~(0x1<<1)));
	if (ret) {
		WCN_DBG(FM_ALT | CHIP, " pwroff diable top_ck_en_adie failed\n");
		return ret;
	}

/* rssi_th_set = fm_false; */
	return ret;
}

/* just for dgb */
#if 0
static void mt6627_bt_write(fm_u32 addr, fm_u32 val)
{
	fm_u32 tem, i = 0;
	mt6627_host_write(0x80103020, addr);
	mt6627_host_write(0x80103024, val);
	mt6627_host_read(0x80103000, &tem);
	while ((tem == 4) && (i < 1000)) {
		i++;
		mt6627_host_read(0x80103000, &tem);
	}
	return;
}
#endif
static fm_bool mt6627_SetFreq(fm_u16 freq)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 chan_para = 0;
	fm_u32 reg_val = 0;
	fm_u16 freq_reg = 0;

	fm_cb_op->cur_freq_set(freq);

#if 0
	/* MCU clock adjust if need */
	if ((ret = mt6627_mcu_dese(freq, NULL)) < 0) {
		WCN_DBG(FM_ERR | MAIN, "mt6627_mcu_dese FAIL:%d\n", ret);
	}

	WCN_DBG(FM_INF | MAIN, "MCU %d\n", ret);

	/* GPS clock adjust if need */
	if ((ret = mt6627_gps_dese(freq, NULL)) < 0) {
		WCN_DBG(FM_ERR | MAIN, "mt6627_gps_dese FAIL:%d\n", ret);
	}

	WCN_DBG(FM_INF | MAIN, "GPS %d\n", ret);
#endif
	/* pwer up sequence 0425 */
	ret = mt6627_top_write(0x0050, 0x00000007);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr top 0x50 failed\n");
	}
	
	ret = mt6627_set_bits(0x0F, 0x0455, 0xF800);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x0f failed\n");
	}
	
	if (mt6627_TDD_chan_check(freq)) {
		ret = mt6627_set_bits(0x30, 0x0008, 0xFFF3);	/* use TDD solution */
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x30 failed\n");
	}
	else {
		ret = mt6627_set_bits(0x30, 0x0000, 0xFFF3);	/* default use FDD solution */
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x30 failed\n");
	}
	ret = mt6627_top_write(0x0050, 0x0000000F);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr top 0x50 failed\n");
	}

/* if (fm_cb_op->chan_para_get) { */
	chan_para = mt6627_chan_para_get(freq);
	WCN_DBG(FM_DBG | CHIP, "%d chan para = %d\n", (fm_s32) freq, (fm_s32) chan_para);
/* } */

	freq_reg = freq;
	if (0 == fm_get_channel_space(freq_reg)) {
		freq_reg *= 10;
	}
	freq_reg = (freq_reg - 6400) * 2 / 10;
	ret = mt6627_set_bits(0x65, freq_reg, 0xFC00);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x65 failed\n");
		return fm_false;
	}
	
	ret = mt6627_set_bits(0x65, (chan_para << 12), 0x0FFF);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "set freq wr 0x65 failed\n");
		return fm_false;
	}
	
	if ((mt6627_hw_info.chip_id == 0x6625) && ((mtk_wcn_wmt_chipid_query() == 0x6592) || (mtk_wcn_wmt_chipid_query() == 0x6752))) {
		if (mt6627_I2S_hopping_check(freq)) {
			/* set i2s TX desense mode */
			 ret = mt6627_set_bits(0x9C, 0x80, 0xFFFF);
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x9C failed\n");
			}
			
			/* set i2s RX desense mode */
			ret = mt6627_host_read(0x80101054, &reg_val);
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq rd 0x80101054 failed\n");
			}
			
			reg_val |= 0x8000;
			ret = mt6627_host_write(0x80101054, reg_val);
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x80101054 failed\n");
			}
		} else {
			ret = mt6627_set_bits(0x9C, 0x0, 0xFF7F);
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x9C failed\n");
			}
			
			ret = mt6627_host_read(0x80101054, &reg_val);
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq rd 0x80101054 failed\n");
			}
			
			reg_val &= 0x7FFF;
			ret = mt6627_host_write(0x80101054, reg_val);			
			if (ret) {
				WCN_DBG(FM_ERR | CHIP, "set freq wr 0x80101054 failed\n");
			}
		}
	}

	if (FM_LOCK(cmd_buf_lock))
		return fm_false;
	pkt_size = mt6627_tune(cmd_buf, TX_BUF_SIZE, freq, chan_para);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_TUNE | FLAG_TUNE_DONE, SW_RETRY_CNT, TUNE_TIMEOUT,
		      NULL);
	FM_UNLOCK(cmd_buf_lock);

	if (ret) {
		WCN_DBG(FM_ALT | CHIP, "mt6627_tune failed\n");
		return fm_false;
	}

	WCN_DBG(FM_DBG | CHIP, "set freq to %d ok\n", freq);
#if 0
	/* ADPLL setting for dbg */
	mt6627_top_write(0x0050, 0x00000007);
	mt6627_top_write(0x0A08, 0xFFFFFFFF);
	mt6627_bt_write(0x82, 0x11);
	mt6627_bt_write(0x83, 0x11);
	mt6627_bt_write(0x84, 0x11);
	mt6627_top_write(0x0040, 0x1C1C1C1C);
	mt6627_top_write(0x0044, 0x1C1C1C1C);
	mt6627_write(0x70, 0x0010);
	/*0x0806 DCO clk
	   0x0802 ref clk
	   0x0804 feedback clk
	 */
	mt6627_write(0xE0, 0x0806);
#endif
	return fm_true;
}

#if 0
/*
* mt6627_Seek
* @pFreq - IN/OUT parm, IN start freq/OUT seek valid freq
* @seekdir - 0:up, 1:down
* @space - 1:50KHz, 2:100KHz, 4:200KHz
* return fm_true:seek success; fm_false:seek failed
*/
static fm_bool mt6627_Seek(fm_u16 min_freq, fm_u16 max_freq, fm_u16 *pFreq, fm_u16 seekdir,
			   fm_u16 space)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size, temp;

	mt6627_RampDown();
	mt6627_read(FM_MAIN_CTRL, &temp);
	mt6627_Mute(fm_true);

	if (FM_LOCK(cmd_buf_lock))
		return fm_false;
	pkt_size = mt6627_seek(cmd_buf, TX_BUF_SIZE, seekdir, space, max_freq, min_freq);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SEEK | FLAG_SEEK_DONE, SW_RETRY_CNT, SEEK_TIMEOUT,
		      mt6627_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6627_res) {
		*pFreq = mt6627_res->seek_result;
		/* fm_cb_op->cur_freq_set(*pFreq); */
	} else {
		WCN_DBG(FM_ALT | CHIP, "mt6627_seek failed\n");
		return ret;
	}

	/* get the result freq */
	WCN_DBG(FM_NTC | CHIP, "seek, result freq:%d\n", *pFreq);
	mt6627_RampDown();
	if ((temp & 0x0020) == 0) {
		mt6627_Mute(fm_false);
	}

	return fm_true;
}
#endif
#define FM_CQI_LOG_PATH "/mnt/sdcard/fmcqilog"

static fm_s32 mt6627_full_cqi_get(fm_s32 min_freq, fm_s32 max_freq, fm_s32 space, fm_s32 cnt)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	fm_u16 freq, orig_freq;
	fm_s32 i, j, k;
	fm_s32 space_val, max, min, num;
	struct mt6627_full_cqi *p_cqi;
	fm_u8 *cqi_log_title =
	    "Freq, RSSI, PAMD, PR, FPAMD, MR, ATDC, PRX, ATDEV, SMGain, DltaRSSI\n";
	fm_u8 cqi_log_buf[100] = { 0 };
	fm_s32 pos;
	fm_u8 cqi_log_path[100] = { 0 };

	WCN_DBG(FM_NTC | CHIP, "6627 cqi log start\n");
	/* for soft-mute tune, and get cqi */
	freq = fm_cb_op->cur_freq_get();
	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}
	/* get cqi */
	orig_freq = freq;
	if (0 == fm_get_channel_space(min_freq)) {
		min = min_freq * 10;
	} else {
		min = min_freq;
	}
	if (0 == fm_get_channel_space(max_freq)) {
		max = max_freq * 10;
	} else {
		max = max_freq;
	}
	if (space == 0x0001) {
		space_val = 5;	/* 50Khz */
	} else if (space == 0x0002) {
		space_val = 10;	/* 100Khz */
	} else if (space == 0x0004) {
		space_val = 20;	/* 200Khz */
	} else {
		space_val = 10;
	}
	num = (max - min) / space_val + 1;	/* Eg, (8760 - 8750) / 10 + 1 = 2 */
	for (k = 0; (10000 == orig_freq) && (0xffffffff == g_dbg_level) && (k < cnt); k++) {
		WCN_DBG(FM_NTC | CHIP, "cqi file:%d\n", k + 1);
		freq = min;
		pos = 0;
		fm_memcpy(cqi_log_path, FM_CQI_LOG_PATH, strlen(FM_CQI_LOG_PATH));
		sprintf(&cqi_log_path[strlen(FM_CQI_LOG_PATH)], "%d.txt", k + 1);
		fm_file_write(cqi_log_path, cqi_log_title, strlen(cqi_log_title), &pos);
		for (j = 0; j < num; j++) {
			if (FM_LOCK(cmd_buf_lock))
				return (-FM_ELOCK);
			pkt_size = mt6627_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
			ret =
			    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT,
				      SM_TUNE_TIMEOUT, mt6627_get_read_result);
			FM_UNLOCK(cmd_buf_lock);

			if (!ret && mt6627_res) {
				WCN_DBG(FM_NTC | CHIP, "smt cqi size %d\n", mt6627_res->cqi[0]);
				p_cqi = (struct mt6627_full_cqi *)&mt6627_res->cqi[2];
				for (i = 0; i < mt6627_res->cqi[1]; i++) {
					/* just for debug */
					WCN_DBG(FM_NTC | CHIP,
						"freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
						p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
						p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
						p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
						p_cqi[i].smg, p_cqi[i].drssi);
					/* format to buffer */
					sprintf(cqi_log_buf,
						"%04d,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,\n",
						p_cqi[i].ch, p_cqi[i].rssi, p_cqi[i].pamd,
						p_cqi[i].pr, p_cqi[i].fpamd, p_cqi[i].mr,
						p_cqi[i].atdc, p_cqi[i].prx, p_cqi[i].atdev,
						p_cqi[i].smg, p_cqi[i].drssi);
					/* write back to log file */
					fm_file_write(cqi_log_path, cqi_log_buf,
						      strlen(cqi_log_buf), &pos);
				}
			} else {
				WCN_DBG(FM_ALT | CHIP, "smt get CQI failed\n");
				ret = -1;
			}
			freq += space_val;
		}
		fm_cb_op->cur_freq_set(0);	/* avoid run too much times */
	}
	WCN_DBG(FM_NTC | CHIP, "6627 cqi log done\n");

	return ret;
}

#if 0
static fm_bool mt6627_Scan(fm_u16 min_freq, fm_u16 max_freq, fm_u16 *pFreq, fm_u16 *pScanTBL,
			   fm_u16 *ScanTBLsize, fm_u16 scandir, fm_u16 space)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size, temp;
	fm_u16 offset = 0;
	fm_u16 tmp_scanTBLsize = *ScanTBLsize;

	if ((!pScanTBL) || (tmp_scanTBLsize == 0)) {
		WCN_DBG(FM_ALT | CHIP, "scan, failed:invalid scan table\n");
		return fm_false;
	}

	WCN_DBG(FM_NTC | CHIP,
		"start freq: %d, max_freq:%d, min_freq:%d, scan BTL size:%d, scandir:%d, space:%d\n",
		*pFreq, max_freq, min_freq, *ScanTBLsize, scandir, space);

	mt6627_RampDown();
	mt6627_read(FM_MAIN_CTRL, &temp);
	mt6627_Mute(fm_true);

	mt6627_full_cqi_get(min_freq, max_freq, space, 5);

	/* normal scan */
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_scan(cmd_buf, TX_BUF_SIZE, scandir, space, max_freq, min_freq);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SCAN | FLAG_SCAN_DONE, SW_RETRY_CNT, SCAN_TIMEOUT,
		      mt6627_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6627_res) {
		fm_memcpy(pScanTBL, mt6627_res->scan_result, sizeof(fm_u16) * FM_SCANTBL_SIZE);
		WCN_DBG(FM_NTC | CHIP, "Rx scan result:\n");

		for (offset = 0; offset < tmp_scanTBLsize; offset++) {
			WCN_DBG(FM_NTC | CHIP, "%d: %04x\n", (fm_s32) offset, *(pScanTBL + offset));
		}

		*ScanTBLsize = tmp_scanTBLsize;
	} else {
		WCN_DBG(FM_ALT | CHIP, "mt6627_scan failed\n");
		return ret;
	}

	mt6627_set_bits(FM_MAIN_CTRL, 0x0000, 0xFFF0);	/* make sure tune/seek/scan/cqi bits = 0 */
	if ((temp & 0x0020) == 0) {
		mt6627_Mute(fm_false);
	}

	return fm_true;
}

/* add for scan cancel case */
static fm_bool cqi_abort = fm_false;

static fm_s32 mt6627_CQI_Get(fm_s8 *buf, fm_s32 buf_len)
{
	fm_s32 ret = 0;
	fm_s32 i;
	fm_u16 pkt_size;
	struct mt6627_fm_cqi *pmt6627_cqi;
	struct adapt_fm_cqi *pcqi;

	if (!buf || buf_len < FM_CQI_BUF_SIZE) {
		return -FM_EBUF;
	}

	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_cqi_get(cmd_buf, TX_BUF_SIZE);
	if (cqi_abort == fm_true) {
		cqi_abort = fm_false;
		ret = -1;
	} else {
		ret =
		    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SCAN | FLAG_CQI_DONE, SW_RETRY_CNT,
			      SCAN_TIMEOUT, mt6627_get_read_result);
	}
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6627_res) {
		/* FIXEDME */
		pmt6627_cqi = (struct mt6627_fm_cqi *)mt6627_res->cqi;
		pcqi = (struct adapt_fm_cqi *)buf;

		for (i = 0; i < (sizeof(mt6627_res->cqi) / sizeof(struct mt6627_fm_cqi)); i++) {
			pcqi[i].ch = (pmt6627_cqi[i].ch * 10 / 2) + 6400;
			pcqi[i].rssi = (fm_s32) pmt6627_cqi[i].rssi;

			if (pcqi[i].rssi >= 32768) {
				pcqi[i].rssi = pcqi[i].rssi - 65536;
			}

			pcqi[i].rssi = ((pcqi[i].rssi * 6) >> 4);
			WCN_DBG(FM_NTC | CHIP, "%d --> %d(dbm)\n", pcqi[i].ch, pcqi[i].rssi);
		}
	} else {
		WCN_DBG(FM_ALT | CHIP, "mt6627 get CQI failed:%d\n", ret);
	}

	mt6627_set_bits(FM_MAIN_CTRL, 0x0000, 0xFFF0);	/* make sure tune/seek/scan/cqi bits = 0 */

	return ret;
}

static fm_bool scan_abort = fm_false;

#ifdef CONFIG_MTK_FM_50KHZ_SUPPORT
#define SCAN_SEG_LEN 250
static fm_s8 raw_buf[16 * sizeof(struct adapt_fm_cqi)] = { 0 };

static fm_bool mt6627_Scan_50KHz(fm_u16 min_freq, fm_u16 max_freq, fm_u16 *pFreq,
				 fm_u16 *pScanTBL, fm_u16 *ScanTBLsize, fm_u16 scandir,
				 fm_u16 space)
{
	fm_s32 ret = 0;
	fm_s32 num;
	fm_s32 seg;
	fm_s32 i, j;
	fm_u16 scan_tbl[FM_SCANTBL_SIZE];	/* need no less than the chip */
	fm_s32 start_freq, end_freq;
	fm_s32 ch_offset, step, tmp_val;
	fm_s32 chl_cnt = 0;
	fm_s32 word_offset, bit_offset;
	fm_s32 space_val = 5;
	struct adapt_fm_cqi *pCQI = (struct adapt_fm_cqi *)raw_buf;

	if (space == 0x0001) {
		space_val = 5;	/* 50Khz */
	} else if (space == 0x0002) {
		space_val = 10;	/* 100Khz */
	} else if (space == 0x0004) {
		space_val = 20;	/* 200Khz */
	}
	/* calculate segment number */
	num = (max_freq - min_freq) / space_val;	/* Eg, (10800 - 8750) / 5 = 410 */
	seg = (num / SCAN_SEG_LEN) + ((num % SCAN_SEG_LEN) ? 1 : 0);	/* Eg, (410 / 200) + ((410 % 200) ? 1 : 0) = 2 + 1 = 3 */

	FM_FIFO_RESET(cqi_fifo);
	fm_memset(pScanTBL, 0, sizeof(fm_u16) * (*ScanTBLsize));

	/* do scan */
	scan_abort = fm_false;	/* reset scan cancel flag */
	for (i = 0; i < seg; i++) {
		start_freq = min_freq + SCAN_SEG_LEN * space_val * i;
		end_freq = min_freq + SCAN_SEG_LEN * space_val * (i + 1) - space_val;
		end_freq = (end_freq > max_freq) ? max_freq : end_freq;
		chl_cnt = 0;

		if (fm_true == scan_abort) {
			scan_abort = fm_false;
			return fm_false;
		}

		if (fm_false ==
		    mt6627_Scan(start_freq, end_freq, pFreq, scan_tbl, ScanTBLsize, scandir,
				space)) {
			return fm_false;
		}
		/* get channel count */
		for (ch_offset = 0; ch_offset < FM_SCANTBL_SIZE; ch_offset++) {
			if (scan_tbl[ch_offset] == 0)
				continue;
			for (step = 0; step < 16; step++) {
				if (scan_tbl[ch_offset] & (1 << step)) {
					tmp_val = start_freq + (ch_offset * 16 + step) * space_val;
					WCN_DBG(FM_NTC | CHIP, "freq %d, end freq %d\n", tmp_val,
						end_freq);
					if (tmp_val <= end_freq) {
						chl_cnt++;
						/* set reult bitmap */
						word_offset = (tmp_val - min_freq) / space_val / 16;
						bit_offset = (tmp_val - min_freq) / space_val % 16;
						if ((word_offset < 26) && (word_offset >= 0)) {
							pScanTBL[word_offset] |= (1 << bit_offset);
						}
						WCN_DBG(FM_NTC | CHIP, "cnt %d, word %d, bit %d\n",
							chl_cnt, word_offset, bit_offset);
					}
				}
			}
		}

		/* get cqi info */
		while (chl_cnt > 0) {
			ret = mt6627_CQI_Get(raw_buf, 16 * sizeof(struct adapt_fm_cqi));
			if (ret) {
				return ret;
			}
			/* add valid channel to cqi_fifo */
			for (j = 0; j < sizeof(raw_buf) / sizeof(struct adapt_fm_cqi); j++) {
				if ((pCQI[j].ch >= start_freq) && (pCQI[j].ch <= end_freq)) {
					FM_FIFO_INPUT(cqi_fifo, pCQI + j);
					WCN_DBG(FM_NTC | CHIP, "%d %d(dbm) add to fifo\n",
						pCQI[j].ch, pCQI[j].rssi);
				}
			}

			chl_cnt -= 16;
		}
	}

	return fm_true;
}


static fm_s32 mt6627_CQI_Get_50KHz(fm_s8 *buf, fm_s32 buf_len)
{
	fm_s32 ret = 0;
	fm_s32 i;
	struct adapt_fm_cqi tmp = {
		.ch = 0,
		.rssi = 0,
	};
	struct adapt_fm_cqi *pcqi = (struct adapt_fm_cqi *)buf;


	if (!buf || buf_len < FM_CQI_BUF_SIZE) {
		return -FM_EBUF;
	}

	for (i = 0; ((i < (buf_len / sizeof(struct adapt_fm_cqi))) &&
		     (fm_false == FM_FIFO_IS_EMPTY(cqi_fifo))); i++) {
		FM_FIFO_OUTPUT(cqi_fifo, &tmp);
		pcqi[i].ch = tmp.ch;
		pcqi[i].rssi = tmp.rssi;
		WCN_DBG(FM_NTC | CHIP, "%d %d(dbm) get from fifo\n", pcqi[i].ch, pcqi[i].rssi);
	}

	return ret;
}

#endif				/* CONFIG_MTK_FM_50KHZ_SUPPORT */
static fm_s32 mt6627_SeekStop(void)
{
	return fm_force_active_event(FLAG_SEEK_DONE);
}

static fm_s32 mt6627_ScanStop(void)
{
	cqi_abort = fm_true;
	scan_abort = fm_true;
	fm_force_active_event(FLAG_SCAN_DONE | FLAG_CQI_DONE);

	return 0;
}

#endif

/*
 * mt6627_GetCurRSSI - get current freq's RSSI value
 * RS=RSSI
 * If RS>511, then RSSI(dBm)= (RS-1024)/16*6
 *				   else RSSI(dBm)= RS/16*6
 */
static fm_s32 mt6627_GetCurRSSI(fm_s32 *pRSSI)
{
	fm_u16 tmp_reg;

	mt6627_read(FM_RSSI_IND, &tmp_reg);
	tmp_reg = tmp_reg & 0x03ff;

	if (pRSSI) {
		*pRSSI = (tmp_reg > 511) ? (((tmp_reg - 1024) * 6) >> 4) : ((tmp_reg * 6) >> 4);
		WCN_DBG(FM_DBG | CHIP, "rssi:%d, dBm:%d\n", tmp_reg, *pRSSI);
	} else {
		WCN_DBG(FM_ERR | CHIP, "get rssi para error\n");
		return -FM_EPARA;
	}

	return 0;
}

static fm_u16 mt6627_vol_tbl[16] = { 0x0000, 0x0519, 0x066A, 0x0814,
	0x0A2B, 0x0CCD, 0x101D, 0x1449,
	0x198A, 0x2027, 0x287A, 0x32F5,
	0x4027, 0x50C3, 0x65AD, 0x7FFF
};

static fm_s32 mt6627_SetVol(fm_u8 vol)
{
	fm_s32 ret = 0;

	vol = (vol > 15) ? 15 : vol;
	ret = mt6627_write(0x7D, mt6627_vol_tbl[vol]);
	if (ret) {
		WCN_DBG(FM_ERR | CHIP, "Set vol=%d Failed\n", vol);
		return ret;
	} else {
		WCN_DBG(FM_DBG | CHIP, "Set vol=%d OK\n", vol);
	}

	if (vol == 10) {
		fm_print_cmd_fifo();	/* just for debug */
		fm_print_evt_fifo();
	}
	return 0;
}

static fm_s32 mt6627_GetVol(fm_u8 *pVol)
{
	int ret = 0;
	fm_u16 tmp;
	fm_s32 i;

	FMR_ASSERT(pVol);

	ret = mt6627_read(0x7D, &tmp);
	if (ret) {
		*pVol = 0;
		WCN_DBG(FM_ERR | CHIP, "Get vol Failed\n");
		return ret;
	}

	for (i = 0; i < 16; i++) {
		if (mt6627_vol_tbl[i] == tmp) {
			*pVol = i;
			break;
		}
	}

	WCN_DBG(FM_DBG | CHIP, "Get vol=%d OK\n", *pVol);
	return 0;
}

static fm_s32 mt6627_dump_reg(void)
{
	fm_s32 i;
	fm_u16 TmpReg;
	for (i = 0; i < 0xff; i++) {
		mt6627_read(i, &TmpReg);
		WCN_DBG(FM_NTC | CHIP, "0x%02x=0x%04x\n", i, TmpReg);
	}
	return 0;
}

/*0:mono, 1:stereo*/
static fm_bool mt6627_GetMonoStereo(fm_u16 *pMonoStereo)
{
#define FM_BF_STEREO 0x1000
	fm_u16 TmpReg;

	if (pMonoStereo) {
		mt6627_read(FM_RSSI_IND, &TmpReg);
		*pMonoStereo = (TmpReg & FM_BF_STEREO) >> 12;
	} else {
		WCN_DBG(FM_ERR | CHIP, "MonoStero: para err\n");
		return fm_false;
	}

	FM_LOG_NTC(CHIP, "Get MonoStero:0x%04x\n", *pMonoStereo);
	return fm_true;
}

static fm_s32 mt6627_SetMonoStereo(fm_s32 MonoStereo)
{
	fm_s32 ret = 0;

	FM_LOG_NTC(CHIP, "set to %s\n", MonoStereo ? "mono" : "auto");
	mt6627_top_write(0x50, 0x0007);

	if (MonoStereo) {	/*mono */
		ret = mt6627_set_bits(0x75, 0x0008, ~0x0008);
	} else {		/*auto switch */

		ret = mt6627_set_bits(0x75, 0x0000, ~0x0008);
	}

	mt6627_top_write(0x50, 0x000F);
	return ret;
}

static fm_s32 mt6627_GetCapArray(fm_s32 *ca)
{
	fm_u16 dataRead;
	fm_u16 tmp = 0;

	FMR_ASSERT(ca);
	mt6627_read(0x60, &tmp);
	mt6627_write(0x60, tmp & 0xFFF7);	/* 0x60 D3=0 */

	mt6627_read(0x26, &dataRead);
	*ca = dataRead;

	mt6627_write(0x60, tmp);	/* 0x60 D3=1 */
	return 0;
}

/*
 * mt6627_GetCurPamd - get current freq's PAMD value
 * PA=PAMD
 * If PA>511 then PAMD(dB)=  (PA-1024)/16*6,
 *				else PAMD(dB)=PA/16*6
 */
static fm_bool mt6627_GetCurPamd(fm_u16 *pPamdLevl)
{
	fm_u16 tmp_reg;
	fm_u16 dBvalue, valid_cnt = 0;
	int i, total = 0;
	for (i = 0; i < 8; i++) {
		if (mt6627_read(FM_ADDR_PAMD, &tmp_reg)) {
			*pPamdLevl = 0;
			return fm_false;
		}

		tmp_reg &= 0x03FF;
		dBvalue = (tmp_reg > 256) ? ((512 - tmp_reg) * 6 / 16) : 0;
		if (dBvalue != 0) {
			total += dBvalue;
			valid_cnt++;
			WCN_DBG(FM_DBG | CHIP, "[%d]PAMD=%d\n", i, dBvalue);
		}
		Delayms(3);
	}
	if (valid_cnt != 0) {
		*pPamdLevl = total / valid_cnt;
	} else {
		*pPamdLevl = 0;
	}
	WCN_DBG(FM_NTC | CHIP, "PAMD=%d\n", *pPamdLevl);
	return fm_true;
}

static fm_s32 mt6627_i2s_info_get(fm_s32 *ponoff, fm_s32 *pmode, fm_s32 *psample)
{
	FMR_ASSERT(ponoff);
	FMR_ASSERT(pmode);
	FMR_ASSERT(psample);

	*ponoff = mt6627_fm_config.aud_cfg.i2s_info.status;
	*pmode = mt6627_fm_config.aud_cfg.i2s_info.mode;
	*psample = mt6627_fm_config.aud_cfg.i2s_info.rate;

	return 0;
}

static fm_s32 mt6627fm_get_audio_info(fm_audio_info_t *data)
{
	memcpy(data, &mt6627_fm_config.aud_cfg, sizeof(fm_audio_info_t));
	return 0;
}

static fm_s32 mt6627_hw_info_get(struct fm_hw_info *req)
{
	FMR_ASSERT(req);

	req->chip_id = mt6627_hw_info.chip_id;
	req->eco_ver = mt6627_hw_info.eco_ver;
	req->patch_ver = mt6627_hw_info.patch_ver;
	req->rom_ver = mt6627_hw_info.rom_ver;

	return 0;
}

static fm_s32 mt6627_pre_search(void)
{
	mt6627_RampDown();
	/* disable audio output I2S Rx mode */
	mt6627_host_write(0x80101054, 0x00000000);
	/* disable audio output I2S Tx mode */
	mt6627_write(0x9B, 0x0000);
	/*FM_LOG_NTC(FM_NTC | CHIP, "search threshold: RSSI=%d,de-RSSI=%d,smg=%d %d\n",
		   mt6627_fm_config.rx_cfg.long_ana_rssi_th, mt6627_fm_config.rx_cfg.desene_rssi_th,
		   mt6627_fm_config.rx_cfg.smg_th);*/
	return 0;
}

static fm_s32 mt6627_restore_search(void)
{
	mt6627_RampDown();
	/* set audio output I2S Tx mode */
	mt6627_write(0x9B, 0xF9AB);
	/* set audio output I2S Rx mode */
	mt6627_host_write(0x80101054, 0x00003f35);
	return 0;
}

static fm_s32 mt6627_soft_mute_tune(fm_u16 freq, fm_s32 *rssi, fm_bool *valid)
{
	fm_s32 ret = 0;
	fm_u16 pkt_size;
	/* fm_u16 freq;//, orig_freq; */
	struct mt6627_full_cqi *p_cqi;
	fm_s32 RSSI = 0, PAMD = 0, MR = 0, ATDC = 0;
	fm_u32 PRX = 0, ATDEV = 0;
	fm_u16 softmuteGainLvl = 0;

	ret = mt6627_chan_para_get(freq);
	if (ret == 2) {
		ret = mt6627_set_bits(FM_CHANNEL_SET, 0x2000, 0x0FFF);	/* mdf HiLo */
	} else {
		ret = mt6627_set_bits(FM_CHANNEL_SET, 0x0000, 0x0FFF);	/* clear FA/HL/ATJ */
	}
	if (FM_LOCK(cmd_buf_lock))
		return (-FM_ELOCK);
	pkt_size = mt6627_full_cqi_req(cmd_buf, TX_BUF_SIZE, &freq, 1, 1);
	ret =
	    fm_cmd_tx(cmd_buf, pkt_size, FLAG_SM_TUNE, SW_RETRY_CNT, SM_TUNE_TIMEOUT,
		      mt6627_get_read_result);
	FM_UNLOCK(cmd_buf_lock);

	if (!ret && mt6627_res) {
		WCN_DBG(FM_NTC | CHIP, "smt cqi size %d\n", mt6627_res->cqi[0]);
		p_cqi = (struct mt6627_full_cqi *)&mt6627_res->cqi[2];
		/* just for debug */
		WCN_DBG(FM_NTC | CHIP,
			"freq %d, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n",
			p_cqi->ch, p_cqi->rssi, p_cqi->pamd, p_cqi->pr, p_cqi->fpamd, p_cqi->mr,
			p_cqi->atdc, p_cqi->prx, p_cqi->atdev, p_cqi->smg, p_cqi->drssi);
		RSSI =
		    ((p_cqi->rssi & 0x03FF) >=
		     512) ? ((p_cqi->rssi & 0x03FF) - 1024) : (p_cqi->rssi & 0x03FF);
		PAMD =
		    ((p_cqi->pamd & 0x1FF) >=
		     256) ? ((p_cqi->pamd & 0x01FF) - 512) : (p_cqi->pamd & 0x01FF);
		MR = ((p_cqi->mr & 0x01FF) >=
		      256) ? ((p_cqi->mr & 0x01FF) - 512) : (p_cqi->mr & 0x01FF);
		ATDC = (p_cqi->atdc >= 32768) ? (65536 - p_cqi->atdc) : (p_cqi->atdc);
		if (ATDC < 0) {
			ATDC = (~(ATDC)) - 1;	/* Get abs value of ATDC */
		}
		PRX = (p_cqi->prx & 0x00FF);
		ATDEV = p_cqi->atdev;
		softmuteGainLvl = p_cqi->smg;
		/* check if the channel is valid according to each CQIs */
		if ((RSSI >= mt6627_fm_config.rx_cfg.long_ana_rssi_th)
		    && (PAMD <= mt6627_fm_config.rx_cfg.pamd_th)
		    && (ATDC <= mt6627_fm_config.rx_cfg.atdc_th)
		    && (MR >= mt6627_fm_config.rx_cfg.mr_th)
		    && (PRX >= mt6627_fm_config.rx_cfg.prx_th)
		    && (ATDEV >= ATDC)	/* sync scan algorithm */
		    &&(softmuteGainLvl >= mt6627_fm_config.rx_cfg.smg_th)) {
			*valid = fm_true;
		} else {
			*valid = fm_false;
		}
		*rssi = RSSI;
/*		if(RSSI < -296)
			WCN_DBG(FM_NTC | CHIP, "rssi\n");
		else if(PAMD > -12)
			WCN_DBG(FM_NTC | CHIP, "PAMD\n");
		else if(ATDC > 3496)
			WCN_DBG(FM_NTC | CHIP, "ATDC\n");
		else if(MR < -67)
			WCN_DBG(FM_NTC | CHIP, "MR\n");
		else if(PRX < 80)
			WCN_DBG(FM_NTC | CHIP, "PRX\n");
		else if(ATDEV < ATDC)
			WCN_DBG(FM_NTC | CHIP, "ATDEV\n");
		else if(softmuteGainLvl < 16421)
			WCN_DBG(FM_NTC | CHIP, "softmuteGainLvl\n");
			*/
	} else {
		WCN_DBG(FM_ALT | CHIP, "smt get CQI failed\n");
		return fm_false;
	}
	WCN_DBG(FM_NTC | CHIP, "valid=%d\n", *valid);
	return fm_true;
}

static fm_bool mt6627_em_test(fm_u16 group_idx, fm_u16 item_idx, fm_u32 item_value)
{
	return fm_true;
}

/*
parm:
	parm.th_type: 0, RSSI. 1,desense RSSI. 2,SMG.
    parm.th_val: threshold value
*/
static fm_s32 mt6627_set_search_th(fm_s32 idx, fm_s32 val, fm_s32 reserve)
{
	switch (idx) {
	case 0:
		{
			mt6627_fm_config.rx_cfg.long_ana_rssi_th = val;
			WCN_DBG(FM_NTC | CHIP, "set rssi th =%d\n", val);
			break;
		}
	case 1:
		{
			mt6627_fm_config.rx_cfg.desene_rssi_th = val;
			WCN_DBG(FM_NTC | CHIP, "set desense rssi th =%d\n", val);
			break;
		}
	case 2:
		{
			mt6627_fm_config.rx_cfg.smg_th = val;
			WCN_DBG(FM_NTC | CHIP, "set smg th =%d\n", val);
			break;
		}
	default:
		break;
	}
	return 0;
}

static fm_s32 MT6627fm_low_power_wa_default(fm_s32 fmon)
{
	return 0;
}

fm_s32 MT6627fm_low_ops_register(struct fm_lowlevel_ops *ops)
{
	fm_s32 ret = 0;
	/* Basic functions. */

	FMR_ASSERT(ops);
	FMR_ASSERT(ops->cb.cur_freq_get);
	FMR_ASSERT(ops->cb.cur_freq_set);
	fm_cb_op = &ops->cb;

	ops->bi.pwron = mt6627_pwron;
	ops->bi.pwroff = mt6627_pwroff;
	ops->bi.msdelay = Delayms;
	ops->bi.usdelay = Delayus;
	ops->bi.read = mt6627_read;
	ops->bi.write = mt6627_write;
	ops->bi.top_read = mt6627_top_read;
	ops->bi.top_write = mt6627_top_write;
	ops->bi.host_read = mt6627_host_read;
	ops->bi.host_write = mt6627_host_write;
	ops->bi.setbits = mt6627_set_bits;
	ops->bi.chipid_get = mt6627_get_chipid;
	ops->bi.mute = mt6627_Mute;
	ops->bi.rampdown = mt6627_RampDown;
	ops->bi.pwrupseq = mt6627_PowerUp;
	ops->bi.pwrdownseq = mt6627_PowerDown;
	ops->bi.setfreq = mt6627_SetFreq;
	ops->bi.low_pwr_wa = MT6627fm_low_power_wa_default;
	ops->bi.get_aud_info = mt6627fm_get_audio_info;
#if 0
	ops->bi.seek = mt6627_Seek;
	ops->bi.seekstop = mt6627_SeekStop;
	ops->bi.scan = mt6627_Scan;
	ops->bi.cqi_get = mt6627_CQI_Get;
#ifdef CONFIG_MTK_FM_50KHZ_SUPPORT
	ops->bi.scan = mt6627_Scan_50KHz;
	ops->bi.cqi_get = mt6627_CQI_Get_50KHz;
#endif
	ops->bi.scanstop = mt6627_ScanStop;
	ops->bi.i2s_set = mt6627_I2s_Setting;
#endif
	ops->bi.rssiget = mt6627_GetCurRSSI;
	ops->bi.volset = mt6627_SetVol;
	ops->bi.volget = mt6627_GetVol;
	ops->bi.dumpreg = mt6627_dump_reg;
	ops->bi.msget = mt6627_GetMonoStereo;
	ops->bi.msset = mt6627_SetMonoStereo;
	ops->bi.pamdget = mt6627_GetCurPamd;
	ops->bi.em = mt6627_em_test;
	ops->bi.anaswitch = mt6627_SetAntennaType;
	ops->bi.anaget = mt6627_GetAntennaType;
	ops->bi.caparray_get = mt6627_GetCapArray;
	ops->bi.hwinfo_get = mt6627_hw_info_get;
	ops->bi.i2s_get = mt6627_i2s_info_get;
	ops->bi.is_dese_chan = mt6627_is_dese_chan;
	ops->bi.softmute_tune = mt6627_soft_mute_tune;
	ops->bi.desense_check = mt6627_desense_check;
	ops->bi.cqi_log = mt6627_full_cqi_get;
	ops->bi.pre_search = mt6627_pre_search;
	ops->bi.restore_search = mt6627_restore_search;
	ops->bi.set_search_th = mt6627_set_search_th;

	cmd_buf_lock = fm_lock_create("27_cmd");
	ret = fm_lock_get(cmd_buf_lock);

	cmd_buf = fm_zalloc(TX_BUF_SIZE + 1);

	if (!cmd_buf) {
		WCN_DBG(FM_ALT | CHIP, "6627 fm lib alloc tx buf failed\n");
		ret = -1;
	}
#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
	cqi_fifo = fm_fifo_create("6628_cqi_fifo", sizeof(struct adapt_fm_cqi), 640);
	if (!cqi_fifo) {
		WCN_DBG(FM_ALT | CHIP, "6627 fm lib create cqi fifo failed\n");
		ret = -1;
	}
#endif

	return ret;
}

fm_s32 MT6627fm_low_ops_unregister(struct fm_lowlevel_ops *ops)
{
	fm_s32 ret = 0;
	/* Basic functions. */
	FMR_ASSERT(ops);

#if 0				/* def CONFIG_MTK_FM_50KHZ_SUPPORT */
	fm_fifo_release(cqi_fifo);
#endif

	if (cmd_buf) {
		fm_free(cmd_buf);
		cmd_buf = NULL;
	}

	ret = fm_lock_put(cmd_buf_lock);
	fm_memset(&ops->bi, 0, sizeof(struct fm_basic_interface));
	return ret;
}

/* static struct fm_pub pub; */
/* static struct fm_pub_cb *pub_cb = &pub.pub_tbl; */

static const fm_u16 mt6627_mcu_dese_list[] = {
	7630, 7800, 7940, 8320, 9260, 9600, 9710, 9920, 10400, 10410
};

static const fm_u16 mt6627_gps_dese_list[] = {
	7850, 7860
};

static const fm_s8 mt6627_chan_para_map[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 6500~6595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6600~6695 */
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0,	/* 6700~6795 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6800~6895 */
	0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 6900~6995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7000~7095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,	/* 7100~7195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0,	/* 7200~7295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7300~7395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7400~7495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7500~7595 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,	/* 7600~7695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7700~7795 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 7800~7895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,	/* 7900~7995 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0,	/* 8000~8095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8100~8195 */
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8200~8295 */
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8300~8395 */
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8400~8495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8500~8595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8600~8695 */
	0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8700~8795 */
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8800~8895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 8900~8995 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9000~9095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9100~9195 */
	0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9200~9295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,	/* 9300~9395 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9400~9495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9500~9595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9600~9695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9700~9795 */
	0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 9800~9895 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0,	/* 9900~9995 */
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10000~10095 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10100~10195 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,	/* 10200~10295 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10300~10395 */
	8, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10400~10495 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10500~10595 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 10600~10695 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,	/* 10700~10795 */
	0			/* 10800 */
};


static const fm_u16 mt6627_scan_dese_list[] = {
	6910, 7680, 7800, 9210, 9220, 9230, 9600, 9980, 9990, 10400, 10750, 10760
};

static const fm_u16 mt6627_I2S_hopping_list[] = {
	6550, 6760, 6960, 6970, 7170, 7370, 7580, 7780, 7990, 8810, 9210, 9220, 10240
};

static const fm_u16 mt6627_TDD_list[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6500~6595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6600~6695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6700~6795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6800~6895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 6900~6995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7000~7095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7100~7195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7200~7295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7300~7395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7400~7495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7500~7595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7600~7695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7700~7795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7800~7895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 7900~7995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8000~8095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8100~8195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8200~8295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8300~8395 */
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8400~8495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8500~8595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8600~8695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8700~8795 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8800~8895 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 8900~8995 */
	0x0000, 0x0000, 0x0101, 0x0101, 0x0101,	/* 9000~9095 */
	0x0101, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9100~9195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9200~9295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9300~9395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9400~9495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9500~9595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 9600~9695 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 9700~9795 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 9800~9895 */
	0x0101, 0x0101, 0x0001, 0x0000, 0x0000,	/* 9900~9995 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10000~10095 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10100~10195 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10200~10295 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10300~10395 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10400~10495 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,	/* 10500~10595 */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0100,	/* 10600~10695 */
	0x0101, 0x0101, 0x0101, 0x0101, 0x0101,	/* 10700~10795 */
	0x0001			/* 10800 */
};

static const fm_u16 mt6627_TDD_Mask[] = {
	0x0001, 0x0010, 0x0100, 0x1000
};

/* return value: 0, not a de-sense channel; 1, this is a de-sense channel; else error no */
static fm_s32 mt6627_is_dese_chan(fm_u16 freq)
{
	fm_s32 size;

	/* return 0;//HQA only :skip desense channel check. */
	size = sizeof(mt6627_scan_dese_list) / sizeof(mt6627_scan_dese_list[0]);

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	while (size) {
		if (mt6627_scan_dese_list[size - 1] == freq)
			return 1;

		size--;
	}

	return 0;
}

/*  return value:
1, is desense channel and rssi is less than threshold;
0, not desense channel or it is but rssi is more than threshold.*/
static fm_s32 mt6627_desense_check(fm_u16 freq, fm_s32 rssi)
{
	if (mt6627_is_dese_chan(freq)) {
		if (rssi < mt6627_fm_config.rx_cfg.desene_rssi_th) {
			return 1;
		}
		WCN_DBG(FM_DBG | CHIP, "desen_rssi %d th:%d\n", rssi,
			mt6627_fm_config.rx_cfg.desene_rssi_th);
	}
	return 0;
}

static fm_bool mt6627_TDD_chan_check(fm_u16 freq)
{
	fm_u32 i = 0;
	fm_u16 freq_tmp = freq;
	fm_s32 ret = 0;

	ret = fm_get_channel_space(freq_tmp);
	if (0 == ret) {
		freq_tmp *= 10;
	} else if (-1 == ret)
		return fm_false;

	i = (freq_tmp - 6500) / 5;

	WCN_DBG(FM_NTC | CHIP, "Freq %d is 0x%4x, mask is 0x%4x\n", freq, (mt6627_TDD_list[i / 4]),
		mt6627_TDD_Mask[i % 4]);
	if (mt6627_TDD_list[i / 4] & mt6627_TDD_Mask[i % 4]) {
		WCN_DBG(FM_NTC | CHIP, "Freq %d use TDD solution\n", freq);
		return fm_true;
	} else
		return fm_false;
}


/* get channel parameter, HL side/ FA / ATJ */
static fm_u16 mt6627_chan_para_get(fm_u16 freq)
{
	fm_s32 pos, size;

	/* return 0;//for HQA only: skip FA/HL/ATJ */
	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}
	if (freq < 6500) {
		return 0;
	}
	pos = (freq - 6500) / 5;

	size = sizeof(mt6627_chan_para_map) / sizeof(mt6627_chan_para_map[0]);

	pos = (pos < 0) ? 0 : pos;
	pos = (pos > (size - 1)) ? (size - 1) : pos;

	return mt6627_chan_para_map[pos];
}

static fm_bool mt6627_I2S_hopping_check(fm_u16 freq)
{
	fm_s32 size;

	size = sizeof(mt6627_I2S_hopping_list) / sizeof(mt6627_I2S_hopping_list[0]);

	if (0 == fm_get_channel_space(freq)) {
		freq *= 10;
	}

	while (size) {
		if (mt6627_I2S_hopping_list[size - 1] == freq)
			return 1;
		size--;
	}

	return 0;
}
