/*****************************************************************************
*
* Filename:
* ---------
*   sn65dsi8x.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   sn65dsi8x header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _sn65dsi8x_SW_H_
#define _sn65dsi8x_SW_H_
#ifndef BUILD_LK
/*----------------------------------------------------------------------------*/
// IIC APIs
/*----------------------------------------------------------------------------*/
 int sn65dsi8x_read_byte(kal_uint8 cmd, kal_uint8 *returnData);
 int sn65dsi8x_write_byte(kal_uint8 cmd, kal_uint8 Data);
#endif
#endif // _sn65dsi8x_SW_H_

