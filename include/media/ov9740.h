/**
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV9740_H__
#define __OV9740_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define OV9740_IOCTL_SET_MODE		_IOW('o', 1, struct ov9740_mode)
#define OV9740_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define OV9740_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3,  enum ov9740_ColorEffect_mode)
#define OV9740_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, enum ov9740_balance_mode)
#define OV9740_IOCTL_SET_CONTRAST       _IOW('o', 5,  enum ov9740_Contrast_mode)
#define OV9740_IOCTL_SET_BRIGHTNESS  _IOW('o', 6, enum ov9740_Brightness_mode)

enum {
        OV9740_ColorEffect = 0,
        OV9740_Whitebalance,
        OV9740_SceneMode,
        OV9740_Exposure,
        OV9740_Brightness ,
	 OV9740_Iso,
        OV9740_Contrast
};

enum ov9740_Brightness_mode{
        OV9740_Brightness_Level1= 30,
        OV9740_Brightness_Level2 = 40,
        OV9740_Brightness_Level3 = 50,
        OV9740_Brightness_Level4 = 60,
        OV9740_Brightness_Level5 = 70,
        OV9740_Brightness_Level6 = 80
};
enum ov9740_balance_mode{
        OV9740_Whitebalance_Invalid = 0,
        OV9740_Whitebalance_Auto,
        OV9740_Whitebalance_Incandescent,
        OV9740_Whitebalance_Fluorescent,
        OV9740_Whitebalance_WarmFluorescent,
        OV9740_Whitebalance_Daylight,
        OV9740_Whitebalance_Cloudy
};
enum ov9740_Contrast_mode{
        OV9740_Contrast_Level1 =1, //-100,
        OV9740_Contrast_Level2 =2,// -50,
        OV9740_Contrast_Level3 = 0,
        OV9740_Contrast_Level4 = 50,
        OV9740_Contrast_Level5 = 100
};
enum ov9740_ColorEffect_mode{
	OV9740_ColorEffect_Invalid = 0,
	OV9740_ColorEffect_Aqua,
	OV9740_ColorEffect_Blackboard,
	OV9740_ColorEffect_Mono,
	OV9740_ColorEffect_Negative,
	OV9740_ColorEffect_None,
	OV9740_ColorEffect_Posterize,
	OV9740_ColorEffect_Sepia,
	OV9740_ColorEffect_Solarize,
	OV9740_ColorEffect_Whiteboard,
	OV9740_ColorEffect_vivid,
	OV9740_YUV_ColorEffect_Emboss,
	OV9740_ColorEffect_redtint,
	OV9740_ColorEffect_bluetint,
	OV9740_ColorEffect_greentint
     
};

struct ov9740_mode {
	int xres;
	int yres;
};

struct ov9740_status {
	int data;
	int status;
};

#ifdef __KERNEL__
struct ov9740_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);
};
#endif /* __KERNEL__ */

#endif  /* __OV9740_H__ */
