/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/
#ifndef BUILD_LK
#include <linux/string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>

#elif (defined BUILD_UBOOT)
#include <asm/arch/mt6577_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/upmu_common.h>
#endif
#include "lcm_drv.h"
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define SN65DSI_DEBUG
#define FRAME_WIDTH  (600)
#define FRAME_HEIGHT (1024)


#define LVDS_LCM_STBY       GPIO118

#define GPIO_LCM1990_RESET      GPIO119
#define GPIO_LCM1990_STBY       GPIO8


#define LCM_DSI_6589_PLL_CLOCK_201_5 0

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (mt_set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))
#define REGFLAG_DELAY 0xAB

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)			lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)											lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)						lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)											lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   				lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)


typedef unsigned char    kal_uint8;



static struct sn65dsi8x_setting_table {
    unsigned char cmd;
    unsigned char data;
};

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{	

		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		params->dsi.mode   =BURST_VDO_MODE;// BURST_VDO_MODE;

	//	    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
	//params->dsi.mode   =SYNC_EVENT_VDO_MODE ;
		// DSI
		/* Command mode setting */  
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

       params->dsi.word_count=1024*3;
		// params->dsi.word_count=480*3;

		params->dsi.vertical_sync_active= 10; //10;//10;
		params->dsi.vertical_backporch= 10 ;//==========10;//12;
		params->dsi.vertical_frontporch= 15 ;//=========15;//9;
		params->dsi.vertical_active_line= FRAME_HEIGHT;//hight

		params->dsi.horizontal_sync_active				= 110 ; //110;//100;  //
		params->dsi.horizontal_backporch				= 110 ;//===========110;//80; //
		params->dsi.horizontal_frontporch				=  100 ;//===============100;//100; //
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;//=wight

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
		params->dsi.pll_select=0;	//0: MIPI_PLL; 1: LVDS_PLL
		params->dsi.PLL_CLOCK = 156;//this value must be in MTK suggested table
        params->dsi.cont_clock = 1;//if not config this para, must config other 7 or 3 paras to gen. PLL
}

static void lcm_init(void)
{

	unsigned int data_array[64];

	mt_set_gpio_mode(GPIO_LCM1990_STBY, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCM1990_STBY, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM1990_STBY, GPIO_OUT_ONE);
	MDELAY(5);

	mt_set_gpio_mode(GPIO_LCM1990_RESET, GPIO_MODE_00);	  
	mt_set_gpio_dir(GPIO_LCM1990_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM1990_RESET, GPIO_OUT_ONE); // LCM_STBY
	MDELAY(15);
	mt_set_gpio_mode(GPIO_LCM1990_RESET, GPIO_MODE_00);    
	mt_set_gpio_dir(GPIO_LCM1990_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM1990_RESET, GPIO_OUT_ZERO); // LCM_RST -h
	MDELAY(70);
	mt_set_gpio_mode(GPIO_LCM1990_RESET, GPIO_MODE_00);    
	mt_set_gpio_dir(GPIO_LCM1990_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCM1990_RESET, GPIO_OUT_ONE); // LCM_RST -h

	MDELAY(15);


	data_array[0] = 0x00023902;
	data_array[1] = 0x00000089;
	dsi_set_cmdq(data_array, 2, 1);

	MDELAY(5);

	data_array[0] = 0x00023902;
	data_array[1] = 0x00008A8C;
	dsi_set_cmdq(data_array, 2, 1);


	data_array[0] = 0x00023902;
	data_array[1] = 0x000023C5;
	dsi_set_cmdq(data_array, 2, 1);
	// 6	
	data_array[0] = 0x00023902;
	data_array[1] = 0x000023C7;
	dsi_set_cmdq(data_array, 2, 1);


	data_array[0] = 0x00023902;
	data_array[1] = 0x00005CFD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00023902;
	data_array[1] = 0x000014FA;
	dsi_set_cmdq(data_array, 2, 1);

}


static void lcm_suspend(void)
{
	unsigned int data_array[64];
	
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000010;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(20);
}


static void lcm_resume(void)
{
	unsigned int data_array[64];
	
	data_array[0] = 0x00023902;
	data_array[1] = 0x00000011;
	dsi_set_cmdq(data_array, 2, 1);
	MDELAY(120);
}
static unsigned int lcm_compare_id(void)
{
#if defined(BUILD_LK)
		printf("NT51021B  lcm_compare_id \n");
#endif

    return 1;
}

LCM_DRIVER nt51021b_mipi_dsi_lcm_drv = 
{
    .name			= "NT51021B",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id    = lcm_compare_id,
};

