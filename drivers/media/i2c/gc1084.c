// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2014-2017 Mentor Graphics Inc.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* min/typical/max system clock (xclk) frequencies */
#define GC1084_XCLK_MIN  6000000
#define GC1084_XCLK_MAX 54000000

#define GC1084_DEFAULT_SLAVE_ID 0x37

#define GC1084_REG_SYS_RESET02		0x3002
#define GC1084_REG_SYS_CLOCK_ENABLE02	0x3006
#define GC1084_REG_SYS_CTRL0		0x3008
#define GC1084_REG_SYS_CTRL0_SW_PWDN	0x42
#define GC1084_REG_SYS_CTRL0_SW_PWUP	0x02
#define GC1084_REG_CHIP_ID		0x300a
#define GC1084_CHIP_ID			0x1084
#define GC1084_REG_CHIP_ID_H		0x03F0
#define GC1084_REG_CHIP_ID_L		0x03F1
#define GC1084_REG_IO_MIPI_CTRL00	0x300e
#define GC1084_REG_PAD_OUTPUT_ENABLE01	0x3017
#define GC1084_REG_PAD_OUTPUT_ENABLE02	0x3018
#define GC1084_REG_PAD_OUTPUT00		0x3019
#define GC1084_REG_SYSTEM_CONTROL1	0x302e
#define GC1084_REG_SC_PLL_CTRL0		0x3034
#define GC1084_REG_SC_PLL_CTRL1		0x3035
#define GC1084_REG_SC_PLL_CTRL2		0x3036
#define GC1084_REG_SC_PLL_CTRL3		0x3037
#define GC1084_REG_SLAVE_ID		0x3100
#define GC1084_REG_SCCB_SYS_CTRL1	0x3103
#define GC1084_REG_SYS_ROOT_DIVIDER	0x3108
#define GC1084_REG_AWB_R_GAIN		0x3400
#define GC1084_REG_AWB_G_GAIN		0x3402
#define GC1084_REG_AWB_B_GAIN		0x3404
#define GC1084_REG_AWB_MANUAL_CTRL	0x3406
#define GC1084_REG_AEC_PK_EXPOSURE_HI	0x3500
#define GC1084_REG_AEC_PK_EXPOSURE_MED	0x3501
#define GC1084_REG_AEC_PK_EXPOSURE_LO	0x3502
#define GC1084_REG_AEC_PK_MANUAL	0x3503
#define GC1084_REG_AEC_PK_REAL_GAIN	0x350a
#define GC1084_REG_AEC_PK_VTS		0x350c
#define GC1084_REG_TIMING_DVPHO		0x3808
#define GC1084_REG_TIMING_DVPVO		0x380a
#define GC1084_REG_TIMING_HTS		0x380c
#define GC1084_REG_TIMING_VTS		0x380e
#define GC1084_REG_TIMING_TC_REG20	0x3820
#define GC1084_REG_TIMING_TC_REG21	0x3821
#define GC1084_REG_AEC_CTRL00		0x3a00
#define GC1084_REG_AEC_B50_STEP		0x3a08
#define GC1084_REG_AEC_B60_STEP		0x3a0a
#define GC1084_REG_AEC_CTRL0D		0x3a0d
#define GC1084_REG_AEC_CTRL0E		0x3a0e
#define GC1084_REG_AEC_CTRL0F		0x3a0f
#define GC1084_REG_AEC_CTRL10		0x3a10
#define GC1084_REG_AEC_CTRL11		0x3a11
#define GC1084_REG_AEC_CTRL1B		0x3a1b
#define GC1084_REG_AEC_CTRL1E		0x3a1e
#define GC1084_REG_AEC_CTRL1F		0x3a1f
#define GC1084_REG_HZ5060_CTRL00	0x3c00
#define GC1084_REG_HZ5060_CTRL01	0x3c01
#define GC1084_REG_SIGMADELTA_CTRL0C	0x3c0c
#define GC1084_REG_FRAME_CTRL01		0x4202
#define GC1084_REG_FORMAT_CONTROL00	0x4300
#define GC1084_REG_VFIFO_HSIZE		0x4602
#define GC1084_REG_VFIFO_VSIZE		0x4604
#define GC1084_REG_JPG_MODE_SELECT	0x4713
#define GC1084_REG_CCIR656_CTRL00	0x4730
#define GC1084_REG_POLARITY_CTRL00	0x4740
#define GC1084_REG_MIPI_CTRL00		0x4800
#define GC1084_REG_DEBUG_MODE		0x4814
#define GC1084_REG_ISP_FORMAT_MUX_CTRL	0x501f
#define GC1084_REG_PRE_ISP_TEST_SET1	0x503d
#define GC1084_REG_SDE_CTRL0		0x5580
#define GC1084_REG_SDE_CTRL1		0x5581
#define GC1084_REG_SDE_CTRL3		0x5583
#define GC1084_REG_SDE_CTRL4		0x5584
#define GC1084_REG_SDE_CTRL5		0x5585
#define GC1084_REG_AVG_READOUT		0x56a1

enum gc1084_mode_id {
	GC1084_MODE_QQVGA_160_120 = 0,
	GC1084_MODE_QCIF_176_144,
	GC1084_MODE_QVGA_320_240,
	GC1084_MODE_VGA_640_480,
	GC1084_MODE_NTSC_720_480,
	GC1084_MODE_PAL_720_576,
	GC1084_MODE_XGA_1024_768,
	GC1084_MODE_720P_1280_720,
	GC1084_MODE_1080P_1920_1080,
	GC1084_MODE_QSXGA_2592_1944,
	GC1084_NUM_MODES,
};

enum gc1084_frame_rate {
	GC1084_08_FPS = 0,
	GC1084_15_FPS,
	GC1084_30_FPS,
	GC1084_60_FPS,
	GC1084_NUM_FRAMERATES,
};

enum gc1084_format_mux {
	GC1084_FMT_MUX_YUV422 = 0,
	GC1084_FMT_MUX_RGB,
	GC1084_FMT_MUX_DITHER,
	GC1084_FMT_MUX_RAW_DPC,
	GC1084_FMT_MUX_SNR_RAW,
	GC1084_FMT_MUX_RAW_CIP,
};

struct gc1084_pixfmt {
	u32 code;
	u32 colorspace;
};

static const struct gc1084_pixfmt gc1084_formats[] = {
	{ MEDIA_BUS_FMT_JPEG_1X8, V4L2_COLORSPACE_JPEG, },
	{ MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_YUYV8_1X16, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_RGB565_2X8_LE, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_RGB565_2X8_BE, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_SGBRG8_1X8, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_SGRBG8_1X8, V4L2_COLORSPACE_SRGB, },
	{ MEDIA_BUS_FMT_SRGGB8_1X8, V4L2_COLORSPACE_SRGB, },
};

/*
 * FIXME: remove this when a subdev API becomes available
 * to set the MIPI CSI-2 virtual channel.
 */
static unsigned int virtual_channel;
module_param(virtual_channel, uint, 0444);
MODULE_PARM_DESC(virtual_channel,
		 "MIPI CSI-2 virtual channel (0..3), default 0");

static const int gc1084_framerates[] = {
	[GC1084_08_FPS] = 8,
	[GC1084_15_FPS] = 15,
	[GC1084_30_FPS] = 30,
	[GC1084_60_FPS] = 60,
};

/* regulator supplies */
static const char * const gc1084_supply_name[] = {
	"DOVDD", /* Digital I/O (1.8V) supply */
	"AVDD",  /* Analog (2.8V) supply */
	"DVDD",  /* Digital Core (1.5V) supply */
};

#define GC1084_NUM_SUPPLIES ARRAY_SIZE(gc1084_supply_name)

/*
 * Image size under 1280 * 960 are SUBSAMPLING
 * Image size upper 1280 * 960 are SCALING
 */
enum gc1084_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct reg_value {
	u16 reg_addr;
	u8 val;
	u8 mask;
	u32 delay_ms;
};

struct gc1084_mode_info {
	enum gc1084_mode_id id;
	enum gc1084_downsize_mode dn_mode;
	u32 hact;
	u32 htot;
	u32 vact;
	u32 vtot;
	const struct reg_value *reg_data;
	u32 reg_data_size;
	u32 max_fps;
};

struct gc1084_ctrls {
	struct v4l2_ctrl_handler handler;
	struct v4l2_ctrl *pixel_rate;
	struct {
		struct v4l2_ctrl *auto_exp;
		struct v4l2_ctrl *exposure;
	};
	struct {
		struct v4l2_ctrl *auto_wb;
		struct v4l2_ctrl *blue_balance;
		struct v4l2_ctrl *red_balance;
	};
	struct {
		struct v4l2_ctrl *auto_gain;
		struct v4l2_ctrl *gain;
	};
	struct v4l2_ctrl *brightness;
	struct v4l2_ctrl *light_freq;
	struct v4l2_ctrl *saturation;
	struct v4l2_ctrl *contrast;
	struct v4l2_ctrl *hue;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

struct gc1084_dev {
	struct i2c_client *i2c_client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_fwnode_endpoint ep; /* the parsed DT endpoint info */
	struct clk *xclk; /* system clock to GC1084 */
	u32 xclk_freq;

	struct regulator_bulk_data supplies[GC1084_NUM_SUPPLIES];
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pwdn_gpio;
	bool   upside_down;

	/* lock to protect all members below */
	struct mutex lock;

	int power_count;

	struct v4l2_mbus_framefmt fmt;
	bool pending_fmt_change;

	const struct gc1084_mode_info *current_mode;
	const struct gc1084_mode_info *last_mode;
	enum gc1084_frame_rate current_fr;
	struct v4l2_fract frame_interval;

	struct gc1084_ctrls ctrls;

	u32 prev_sysclk, prev_hts;
	u32 ae_low, ae_high, ae_target;

	bool pending_mode_change;
	bool streaming;
};

static inline struct gc1084_dev *to_gc1084_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc1084_dev, sd);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct gc1084_dev,
			     ctrls.handler)->sd;
}

/*
 * FIXME: all of these register tables are likely filled with
 * entries that set the register to their power-on default values,
 * and which are otherwise not touched by this driver. Those entries
 * should be identified and removed to speed register load time
 * over i2c.
 */
/* YUV422 UYVY VGA@30fps */
static const struct reg_value gc1084_init_setting_30fps_VGA[] = {
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0x00, 0, 0},
{0x03f2, 0x00, 0, 0},
{0x03f3, 0x00, 0, 0},
{0x03f4, 0x36, 0, 0},
{0x03f5, 0xc0, 0, 0},
{0x03f6, 0x13, 0, 0},
{0x03f7, 0x01, 0, 0},
{0x03f8, 0x32, 0, 0},
{0x03f9, 0x21, 0, 0},
{0x03fc, 0xae, 0, 0},
{0x0d05, 0x08, 0, 0},
{0x0d06, 0xae, 0, 0},
{0x0d08, 0x10, 0, 0},
{0x0d0a, 0x02, 0, 0},
{0x000c, 0x03, 0, 0},
{0x0d0d, 0x02, 0, 0},
{0x0d0e, 0xd4, 0, 0},
{0x000f, 0x05, 0, 0},
{0x0010, 0x08, 0, 0},
{0x0017, 0x08, 0, 0},
{0x0d73, 0x92, 0, 0},
{0x0076, 0x00, 0, 0},
{0x0d76, 0x00, 0, 0},
{0x0d41, 0x02, 0, 0},
{0x0d42, 0xee, 0, 0},
{0x0d7a, 0x0a, 0, 0},
{0x006b, 0x18, 0, 0},
{0x0db0, 0x9d, 0, 0},
{0x0db1, 0x00, 0, 0},
{0x0db2, 0xac, 0, 0},
{0x0db3, 0xd5, 0, 0},
{0x0db4, 0x00, 0, 0},
{0x0db5, 0x97, 0, 0},
{0x0db6, 0x09, 0, 0},
{0x00d2, 0xfc, 0, 0},
{0x0d19, 0x31, 0, 0},
{0x0d20, 0x40, 0, 0},
{0x0d25, 0xcb, 0, 0},
{0x0d27, 0x03, 0, 0},
{0x0d29, 0x40, 0, 0},
{0x0d43, 0x20, 0, 0},
{0x0058, 0x60, 0, 0},
{0x00d6, 0x66, 0, 0},
{0x00d7, 0x19, 0, 0},
{0x0093, 0x02, 0, 0},
{0x00d9, 0x14, 0, 0},
{0x00da, 0xc1, 0, 0},
{0x0d2a, 0x00, 0, 0},
{0x0d28, 0x04, 0, 0},
{0x0dc2, 0x84, 0, 0},
{0x0050, 0x30, 0, 0},
{0x0080, 0x07, 0, 0},
{0x008c, 0x05, 0, 0},
{0x008d, 0xa8, 0, 0},
{0x0077, 0x01, 0, 0},
{0x0078, 0xee, 0, 0},
{0x0079, 0x02, 0, 0},
{0x0067, 0xc0, 0, 0},
{0x0054, 0xff, 0, 0},
{0x0055, 0x02, 0, 0},
{0x0056, 0x00, 0, 0},
{0x0057, 0x04, 0, 0},
{0x005a, 0xff, 0, 0},
{0x005b, 0x07, 0, 0},
{0x00d5, 0x03, 0, 0},
{0x0102, 0xa9, 0, 0},
{0x0d03, 0x02, 0, 0},
{0x0d04, 0xd0, 0, 0},
{0x007a, 0x60, 0, 0},
{0x04e0, 0xff, 0, 0},
{0x0414, 0x75, 0, 0},
{0x0415, 0x75, 0, 0},
{0x0416, 0x75, 0, 0},
{0x0417, 0x75, 0, 0},
{0x0122, 0x00, 0, 0},
{0x0121, 0x80, 0, 0},
{0x0428, 0x10, 0, 0},
{0x0429, 0x10, 0, 0},
{0x042a, 0x10, 0, 0},
{0x042b, 0x10, 0, 0},
{0x042c, 0x14, 0, 0},
{0x042d, 0x14, 0, 0},
{0x042e, 0x18, 0, 0},
{0x042f, 0x18, 0, 0},
{0x0430, 0x05, 0, 0},
{0x0431, 0x05, 0, 0},
{0x0432, 0x05, 0, 0},
{0x0433, 0x05, 0, 0},
{0x0434, 0x05, 0, 0},
{0x0435, 0x05, 0, 0},
{0x0436, 0x05, 0, 0},
{0x0437, 0x05, 0, 0},
{0x0153, 0x00, 0, 0},
{0x0190, 0x01, 0, 0},
{0x0192, 0x02, 0, 0},
{0x0194, 0x04, 0, 0},
{0x0195, 0x02, 0, 0},
{0x0196, 0xd0, 0, 0},
{0x0197, 0x05, 0, 0},
{0x0198, 0x00, 0, 0},
{0x0201, 0x23, 0, 0},
{0x0202, 0x53, 0, 0},
{0x0203, 0xce, 0, 0},
{0x0208, 0x39, 0, 0},
{0x0212, 0x06, 0, 0},
{0x0213, 0x40, 0, 0},
{0x0215, 0x12, 0, 0},
{0x0229, 0x05, 0, 0},
{0x023e, 0x98, 0, 0},
{0x031e, 0x3e, 0, 0},
};

static const struct reg_value gc1084_setting_720P_1280_720[] = {
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0xf0, 0, 0},
{0x03fe, 0x00, 0, 0},
{0x03f2, 0x00, 0, 0},
{0x03f3, 0x00, 0, 0},
{0x03f4, 0x36, 0, 0},
{0x03f5, 0xc0, 0, 0},
{0x03f6, 0x13, 0, 0},
{0x03f7, 0x01, 0, 0},
{0x03f8, 0x32, 0, 0},
{0x03f9, 0x21, 0, 0},
{0x03fc, 0xae, 0, 0},
{0x0d05, 0x08, 0, 0},
{0x0d06, 0xae, 0, 0},
{0x0d08, 0x10, 0, 0},
{0x0d0a, 0x02, 0, 0},
{0x000c, 0x03, 0, 0},
{0x0d0d, 0x02, 0, 0},
{0x0d0e, 0xd4, 0, 0},
{0x000f, 0x05, 0, 0},
{0x0010, 0x08, 0, 0},
{0x0017, 0x08, 0, 0},
{0x0d73, 0x92, 0, 0},
{0x0076, 0x00, 0, 0},
{0x0d76, 0x00, 0, 0},
{0x0d41, 0x02, 0, 0},
{0x0d42, 0xee, 0, 0},
{0x0d7a, 0x0a, 0, 0},
{0x006b, 0x18, 0, 0},
{0x0db0, 0x9d, 0, 0},
{0x0db1, 0x00, 0, 0},
{0x0db2, 0xac, 0, 0},
{0x0db3, 0xd5, 0, 0},
{0x0db4, 0x00, 0, 0},
{0x0db5, 0x97, 0, 0},
{0x0db6, 0x09, 0, 0},
{0x00d2, 0xfc, 0, 0},
{0x0d19, 0x31, 0, 0},
{0x0d20, 0x40, 0, 0},
{0x0d25, 0xcb, 0, 0},
{0x0d27, 0x03, 0, 0},
{0x0d29, 0x40, 0, 0},
{0x0d43, 0x20, 0, 0},
{0x0058, 0x60, 0, 0},
{0x00d6, 0x66, 0, 0},
{0x00d7, 0x19, 0, 0},
{0x0093, 0x02, 0, 0},
{0x00d9, 0x14, 0, 0},
{0x00da, 0xc1, 0, 0},
{0x0d2a, 0x00, 0, 0},
{0x0d28, 0x04, 0, 0},
{0x0dc2, 0x84, 0, 0},
{0x0050, 0x30, 0, 0},
{0x0080, 0x07, 0, 0},
{0x008c, 0x05, 0, 0},
{0x008d, 0xa8, 0, 0},
{0x0077, 0x01, 0, 0},
{0x0078, 0xee, 0, 0},
{0x0079, 0x02, 0, 0},
{0x0067, 0xc0, 0, 0},
{0x0054, 0xff, 0, 0},
{0x0055, 0x02, 0, 0},
{0x0056, 0x00, 0, 0},
{0x0057, 0x04, 0, 0},
{0x005a, 0xff, 0, 0},
{0x005b, 0x07, 0, 0},
{0x00d5, 0x03, 0, 0},
{0x0102, 0xa9, 0, 0},
{0x0d03, 0x02, 0, 0},
{0x0d04, 0xd0, 0, 0},
{0x007a, 0x60, 0, 0},
{0x04e0, 0xff, 0, 0},
{0x0414, 0x75, 0, 0},
{0x0415, 0x75, 0, 0},
{0x0416, 0x75, 0, 0},
{0x0417, 0x75, 0, 0},
{0x0122, 0x00, 0, 0},
{0x0121, 0x80, 0, 0},
{0x0428, 0x10, 0, 0},
{0x0429, 0x10, 0, 0},
{0x042a, 0x10, 0, 0},
{0x042b, 0x10, 0, 0},
{0x042c, 0x14, 0, 0},
{0x042d, 0x14, 0, 0},
{0x042e, 0x18, 0, 0},
{0x042f, 0x18, 0, 0},
{0x0430, 0x05, 0, 0},
{0x0431, 0x05, 0, 0},
{0x0432, 0x05, 0, 0},
{0x0433, 0x05, 0, 0},
{0x0434, 0x05, 0, 0},
{0x0435, 0x05, 0, 0},
{0x0436, 0x05, 0, 0},
{0x0437, 0x05, 0, 0},
{0x0153, 0x00, 0, 0},
{0x0190, 0x01, 0, 0},
{0x0192, 0x02, 0, 0},
{0x0194, 0x04, 0, 0},
{0x0195, 0x02, 0, 0},
{0x0196, 0xd0, 0, 0},
{0x0197, 0x05, 0, 0},
{0x0198, 0x00, 0, 0},
{0x0201, 0x23, 0, 0},
{0x0202, 0x53, 0, 0},
{0x0203, 0xce, 0, 0},
{0x0208, 0x39, 0, 0},
{0x0212, 0x06, 0, 0},
{0x0213, 0x40, 0, 0},
{0x0215, 0x12, 0, 0},
{0x0229, 0x05, 0, 0},
{0x023e, 0x98, 0, 0},
{0x031e, 0x3e, 0, 0},
};

/* power-on sensor init reg table */
static const struct gc1084_mode_info gc1084_mode_init_data = {
	0, SUBSAMPLING, 1280, 1896, 720, 740,
	gc1084_init_setting_30fps_VGA,
	ARRAY_SIZE(gc1084_init_setting_30fps_VGA),
	GC1084_30_FPS,
};

static const struct gc1084_mode_info
gc1084_mode_data[GC1084_NUM_MODES] = {
	{GC1084_MODE_720P_1280_720, SUBSAMPLING,
	 1280, 1892, 720, 740,
	 gc1084_setting_720P_1280_720,
	 ARRAY_SIZE(gc1084_setting_720P_1280_720),
	 GC1084_30_FPS},
};

static int gc1084_init_slave_id(struct gc1084_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	if (client->addr == GC1084_DEFAULT_SLAVE_ID)
		return 0;

	buf[0] = GC1084_REG_SLAVE_ID >> 8;
	buf[1] = GC1084_REG_SLAVE_ID & 0xff;
	buf[2] = client->addr << 1;

	msg.addr = GC1084_DEFAULT_SLAVE_ID;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed with %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int gc1084_write_reg(struct gc1084_dev *sensor, u16 reg, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x, val=%x\n",
			__func__, reg, val);
		return ret;
	}

	return 0;
}

static int gc1084_read_reg(struct gc1084_dev *sensor, u16 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x\n",
			__func__, reg);
		return ret;
	}

	*val = buf[0];
	return 0;
}

static int gc1084_read_reg16(struct gc1084_dev *sensor, u16 reg, u16 *val)
{
	u8 hi, lo;
	int ret;

	ret = gc1084_read_reg(sensor, reg, &hi);
	if (ret)
		return ret;
	ret = gc1084_read_reg(sensor, reg + 1, &lo);
	if (ret)
		return ret;

	*val = ((u16)hi << 8) | (u16)lo;
	return 0;
}

static int gc1084_write_reg16(struct gc1084_dev *sensor, u16 reg, u16 val)
{
	int ret;

	ret = gc1084_write_reg(sensor, reg, val >> 8);
	if (ret)
		return ret;

	return gc1084_write_reg(sensor, reg + 1, val & 0xff);
}

static int gc1084_mod_reg(struct gc1084_dev *sensor, u16 reg,
			  u8 mask, u8 val)
{
	u8 readval;
	int ret;

	ret = gc1084_read_reg(sensor, reg, &readval);
	if (ret)
		return ret;

	readval &= ~mask;
	val &= mask;
	val |= readval;

	return gc1084_write_reg(sensor, reg, val);
}

/*
 * After trying the various combinations, reading various
 * documentations spread around the net, and from the various
 * feedback, the clock tree is probably as follows:
 *
 *   +--------------+
 *   |  Ext. Clock  |
 *   +-+------------+
 *     |  +----------+
 *     +->|   PLL1   | - reg 0x3036, for the multiplier
 *        +-+--------+ - reg 0x3037, bits 0-3 for the pre-divider
 *          |  +--------------+
 *          +->| System Clock |  - reg 0x3035, bits 4-7
 *             +-+------------+
 *               |  +--------------+
 *               +->| MIPI Divider | - reg 0x3035, bits 0-3
 *               |  +-+------------+
 *               |    +----------------> MIPI SCLK
 *               |    +  +-----+
 *               |    +->| / 2 |-------> MIPI BIT CLK
 *               |       +-----+
 *               |  +--------------+
 *               +->| PLL Root Div | - reg 0x3037, bit 4
 *                  +-+------------+
 *                    |  +---------+
 *                    +->| Bit Div | - reg 0x3034, bits 0-3
 *                       +-+-------+
 *                         |  +-------------+
 *                         +->| SCLK Div    | - reg 0x3108, bits 0-1
 *                         |  +-+-----------+
 *                         |    +---------------> SCLK
 *                         |  +-------------+
 *                         +->| SCLK 2X Div | - reg 0x3108, bits 2-3
 *                         |  +-+-----------+
 *                         |    +---------------> SCLK 2X
 *                         |  +-------------+
 *                         +->| PCLK Div    | - reg 0x3108, bits 4-5
 *                            ++------------+
 *                             +  +-----------+
 *                             +->|   P_DIV   | - reg 0x3035, bits 0-3
 *                                +-----+-----+
 *                                       +------------> PCLK
 *
 * This is deviating from the datasheet at least for the register
 * 0x3108, since it's said here that the PCLK would be clocked from
 * the PLL.
 *
 * There seems to be also (unverified) constraints:
 *  - the PLL pre-divider output rate should be in the 4-27MHz range
 *  - the PLL multiplier output rate should be in the 500-1000MHz range
 *  - PCLK >= SCLK * 2 in YUV, >= SCLK in Raw or JPEG
 *
 * In the two latter cases, these constraints are met since our
 * factors are hardcoded. If we were to change that, we would need to
 * take this into account. The only varying parts are the PLL
 * multiplier and the system clock divider, which are shared between
 * all these clocks so won't cause any issue.
 */

/*
 * This is supposed to be ranging from 1 to 8, but the value is always
 * set to 3 in the vendor kernels.
 */
#define GC1084_PLL_PREDIV	3

#define GC1084_PLL_MULT_MIN	4
#define GC1084_PLL_MULT_MAX	252

/*
 * This is supposed to be ranging from 1 to 16, but the value is
 * always set to either 1 or 2 in the vendor kernels.
 */
#define GC1084_SYSDIV_MIN	1
#define GC1084_SYSDIV_MAX	16

/*
 * Hardcode these values for scaler and non-scaler modes.
 * FIXME: to be re-calcualted for 1 data lanes setups
 */
#define GC1084_MIPI_DIV_PCLK	2
#define GC1084_MIPI_DIV_SCLK	1

/*
 * This is supposed to be ranging from 1 to 2, but the value is always
 * set to 2 in the vendor kernels.
 */
#define GC1084_PLL_ROOT_DIV			2
#define GC1084_PLL_CTRL3_PLL_ROOT_DIV_2		BIT(4)

/*
 * We only supports 8-bit formats at the moment
 */
#define GC1084_BIT_DIV				2
#define GC1084_PLL_CTRL0_MIPI_MODE_8BIT		0x08

/*
 * This is supposed to be ranging from 1 to 8, but the value is always
 * set to 2 in the vendor kernels.
 */
#define GC1084_SCLK_ROOT_DIV	2

/*
 * This is hardcoded so that the consistency is maintained between SCLK and
 * SCLK 2x.
 */
#define GC1084_SCLK2X_ROOT_DIV (GC1084_SCLK_ROOT_DIV / 2)

/*
 * This is supposed to be ranging from 1 to 8, but the value is always
 * set to 1 in the vendor kernels.
 */
#define GC1084_PCLK_ROOT_DIV			1
#define GC1084_PLL_SYS_ROOT_DIVIDER_BYPASS	0x00

static unsigned long gc1084_compute_sys_clk(struct gc1084_dev *sensor,
					    u8 pll_prediv, u8 pll_mult,
					    u8 sysdiv)
{
	unsigned long sysclk = sensor->xclk_freq / pll_prediv * pll_mult;

	/* PLL1 output cannot exceed 1GHz. */
	if (sysclk / 1000000 > 1000)
		return 0;

	return sysclk / sysdiv;
}

static unsigned long gc1084_calc_sys_clk(struct gc1084_dev *sensor,
					 unsigned long rate,
					 u8 *pll_prediv, u8 *pll_mult,
					 u8 *sysdiv)
{
	unsigned long best = ~0;
	u8 best_sysdiv = 1, best_mult = 1;
	u8 _sysdiv, _pll_mult;

	for (_sysdiv = GC1084_SYSDIV_MIN;
	     _sysdiv <= GC1084_SYSDIV_MAX;
	     _sysdiv++) {
		for (_pll_mult = GC1084_PLL_MULT_MIN;
		     _pll_mult <= GC1084_PLL_MULT_MAX;
		     _pll_mult++) {
			unsigned long _rate;

			/*
			 * The PLL multiplier cannot be odd if above
			 * 127.
			 */
			if (_pll_mult > 127 && (_pll_mult % 2))
				continue;

			_rate = gc1084_compute_sys_clk(sensor,
						       GC1084_PLL_PREDIV,
						       _pll_mult, _sysdiv);

			/*
			 * We have reached the maximum allowed PLL1 output,
			 * increase sysdiv.
			 */
			if (!_rate)
				break;

			/*
			 * Prefer rates above the expected clock rate than
			 * below, even if that means being less precise.
			 */
			if (_rate < rate)
				continue;

			if (abs(rate - _rate) < abs(rate - best)) {
				best = _rate;
				best_sysdiv = _sysdiv;
				best_mult = _pll_mult;
			}

			if (_rate == rate)
				goto out;
		}
	}

out:
	*sysdiv = best_sysdiv;
	*pll_prediv = GC1084_PLL_PREDIV;
	*pll_mult = best_mult;

	return best;
}

static int gc1084_check_valid_mode(struct gc1084_dev *sensor,
				   const struct gc1084_mode_info *mode,
				   enum gc1084_frame_rate rate)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;

	switch (mode->id) {
	case GC1084_MODE_QQVGA_160_120:
	case GC1084_MODE_QCIF_176_144:
	case GC1084_MODE_QVGA_320_240:
	case GC1084_MODE_NTSC_720_480:
	case GC1084_MODE_PAL_720_576 :
	case GC1084_MODE_XGA_1024_768:
	case GC1084_MODE_720P_1280_720:
		if ((rate != GC1084_15_FPS) &&
		    (rate != GC1084_30_FPS))
			ret = -EINVAL;
		break;
	case GC1084_MODE_VGA_640_480:
		if ((rate != GC1084_15_FPS) &&
		    (rate != GC1084_30_FPS))
			ret = -EINVAL;
		break;
	case GC1084_MODE_1080P_1920_1080:
		if (sensor->ep.bus_type == V4L2_MBUS_CSI2_DPHY) {
			if ((rate != GC1084_15_FPS) &&
			    (rate != GC1084_30_FPS))
				ret = -EINVAL;
		 } else {
			if ((rate != GC1084_15_FPS))
				ret = -EINVAL;
		 }
		break;
	case GC1084_MODE_QSXGA_2592_1944:
		if (rate != GC1084_08_FPS)
			ret = -EINVAL;
		break;
	default:
		dev_err(&client->dev, "Invalid mode (%d)\n", mode->id);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * gc1084_set_mipi_pclk() - Calculate the clock tree configuration values
 *			    for the MIPI CSI-2 output.
 *
 * @rate: The requested bandwidth per lane in bytes per second.
 *	  'Bandwidth Per Lane' is calculated as:
 *	  bpl = HTOT * VTOT * FPS * bpp / num_lanes;
 *
 * This function use the requested bandwidth to calculate:
 * - sample_rate = bpl / (bpp / num_lanes);
 *	         = bpl / (PLL_RDIV * BIT_DIV * PCLK_DIV * MIPI_DIV / num_lanes);
 *
 * - mipi_sclk   = bpl / MIPI_DIV / 2; ( / 2 is for CSI-2 DDR)
 *
 * with these fixed parameters:
 *	PLL_RDIV	= 2;
 *	BIT_DIVIDER	= 2; (MIPI_BIT_MODE == 8 ? 2 : 2,5);
 *	PCLK_DIV	= 1;
 *
 * The MIPI clock generation differs for modes that use the scaler and modes
 * that do not. In case the scaler is in use, the MIPI_SCLK generates the MIPI
 * BIT CLk, and thus:
 *
 * - mipi_sclk = bpl / MIPI_DIV / 2;
 *   MIPI_DIV = 1;
 *
 * For modes that do not go through the scaler, the MIPI BIT CLOCK is generated
 * from the pixel clock, and thus:
 *
 * - sample_rate = bpl / (bpp / num_lanes);
 *	         = bpl / (2 * 2 * 1 * MIPI_DIV / num_lanes);
 *		 = bpl / (4 * MIPI_DIV / num_lanes);
 * - MIPI_DIV	 = bpp / (4 * num_lanes);
 *
 * FIXME: this have been tested with 16bpp and 2 lanes setup only.
 * MIPI_DIV is fixed to value 2, but it -might- be changed according to the
 * above formula for setups with 1 lane or image formats with different bpp.
 *
 * FIXME: this deviates from the sensor manual documentation which is quite
 * thin on the MIPI clock tree generation part.
 */
static int gc1084_set_mipi_pclk(struct gc1084_dev *sensor,
				unsigned long rate)
{
	const struct gc1084_mode_info *mode = sensor->current_mode;
	u8 prediv, mult, sysdiv;
	u8 mipi_div;
	int ret;

	/*
	 * 1280x720 is reported to use 'SUBSAMPLING' only,
	 * but according to the sensor manual it goes through the
	 * scaler before subsampling.
	 */
	if (mode->dn_mode == SCALING ||
	   (mode->id == GC1084_MODE_720P_1280_720))
		mipi_div = GC1084_MIPI_DIV_SCLK;
	else
		mipi_div = GC1084_MIPI_DIV_PCLK;

	gc1084_calc_sys_clk(sensor, rate, &prediv, &mult, &sysdiv);

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL0,
			     0x0f, GC1084_PLL_CTRL0_MIPI_MODE_8BIT);

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL1,
			     0xff, sysdiv << 4 | mipi_div);
	if (ret)
		return ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL2, 0xff, mult);
	if (ret)
		return ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL3,
			     0x1f, GC1084_PLL_CTRL3_PLL_ROOT_DIV_2 | prediv);
	if (ret)
		return ret;

	return gc1084_mod_reg(sensor, GC1084_REG_SYS_ROOT_DIVIDER,
			      0x30, GC1084_PLL_SYS_ROOT_DIVIDER_BYPASS);
}

static unsigned long gc1084_calc_pclk(struct gc1084_dev *sensor,
				      unsigned long rate,
				      u8 *pll_prediv, u8 *pll_mult, u8 *sysdiv,
				      u8 *pll_rdiv, u8 *bit_div, u8 *pclk_div)
{
	unsigned long _rate = rate * GC1084_PLL_ROOT_DIV * GC1084_BIT_DIV *
				GC1084_PCLK_ROOT_DIV;

	_rate = gc1084_calc_sys_clk(sensor, _rate, pll_prediv, pll_mult,
				    sysdiv);
	*pll_rdiv = GC1084_PLL_ROOT_DIV;
	*bit_div = GC1084_BIT_DIV;
	*pclk_div = GC1084_PCLK_ROOT_DIV;

	return _rate / *pll_rdiv / *bit_div / *pclk_div;
}

static int gc1084_set_dvp_pclk(struct gc1084_dev *sensor, unsigned long rate)
{
	u8 prediv, mult, sysdiv, pll_rdiv, bit_div, pclk_div;
	int ret;

	gc1084_calc_pclk(sensor, rate, &prediv, &mult, &sysdiv, &pll_rdiv,
			 &bit_div, &pclk_div);

	if (bit_div == 2)
		bit_div = 0xA;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL0,
			     0x0f, bit_div);
	if (ret)
		return ret;

	/*
	 * We need to set sysdiv according to the clock, and to clear
	 * the MIPI divider.
	 */
	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL1,
			     0xff, sysdiv << 4);
	if (ret)
		return ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL2,
			     0xff, mult);
	if (ret)
		return ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SC_PLL_CTRL3,
			     0x1f, prediv | ((pll_rdiv - 1) << 4));
	if (ret)
		return ret;

	return gc1084_mod_reg(sensor, GC1084_REG_SYS_ROOT_DIVIDER, 0x30,
			      (ilog2(pclk_div) << 4));
}

/* set JPEG framing sizes */
static int gc1084_set_jpeg_timings(struct gc1084_dev *sensor,
				   const struct gc1084_mode_info *mode)
{
	int ret;

	/*
	 * compression mode 3 timing
	 *
	 * Data is transmitted with programmable width (VFIFO_HSIZE).
	 * No padding done. Last line may have less data. Varying
	 * number of lines per frame, depending on amount of data.
	 */
	ret = gc1084_mod_reg(sensor, GC1084_REG_JPG_MODE_SELECT, 0x7, 0x3);
	if (ret < 0)
		return ret;

	ret = gc1084_write_reg16(sensor, GC1084_REG_VFIFO_HSIZE, mode->hact);
	if (ret < 0)
		return ret;

	return gc1084_write_reg16(sensor, GC1084_REG_VFIFO_VSIZE, mode->vact);
}

/* download gc1084 settings to sensor through i2c */
static int gc1084_set_timings(struct gc1084_dev *sensor,
			      const struct gc1084_mode_info *mode)
{
	int ret;

	if (sensor->fmt.code == MEDIA_BUS_FMT_JPEG_1X8) {
		ret = gc1084_set_jpeg_timings(sensor, mode);
		if (ret < 0)
			return ret;
	}

	ret = gc1084_write_reg16(sensor, GC1084_REG_TIMING_DVPHO, mode->hact);
	if (ret < 0)
		return ret;

	ret = gc1084_write_reg16(sensor, GC1084_REG_TIMING_DVPVO, mode->vact);
	if (ret < 0)
		return ret;

	ret = gc1084_write_reg16(sensor, GC1084_REG_TIMING_HTS, mode->htot);
	if (ret < 0)
		return ret;

	return gc1084_write_reg16(sensor, GC1084_REG_TIMING_VTS, mode->vtot);
}

static int gc1084_load_regs(struct gc1084_dev *sensor,
			    const struct gc1084_mode_info *mode)
{
	const struct reg_value *regs = mode->reg_data;
	unsigned int i;
	u32 delay_ms;
	u16 reg_addr;
	u8 mask, val;
	int ret = 0;

	for (i = 0; i < mode->reg_data_size; ++i, ++regs) {
		delay_ms = regs->delay_ms;
		reg_addr = regs->reg_addr;
		val = regs->val;
		mask = regs->mask;

		/* remain in power down mode for DVP */
		if (regs->reg_addr == GC1084_REG_SYS_CTRL0 &&
		    val == GC1084_REG_SYS_CTRL0_SW_PWUP &&
		    sensor->ep.bus_type != V4L2_MBUS_CSI2_DPHY)
			continue;

		if (mask)
			ret = gc1084_mod_reg(sensor, reg_addr, mask, val);
		else
			ret = gc1084_write_reg(sensor, reg_addr, val);
		if (ret)
			break;

		if (delay_ms)
			usleep_range(1000 * delay_ms, 1000 * delay_ms + 100);
	}

	return gc1084_set_timings(sensor, mode);
}

static int gc1084_set_autoexposure(struct gc1084_dev *sensor, bool on)
{
	return gc1084_mod_reg(sensor, GC1084_REG_AEC_PK_MANUAL,
			      BIT(0), on ? 0 : BIT(0));
}

/* read exposure, in number of line periods */
static int gc1084_get_exposure(struct gc1084_dev *sensor)
{
	int exp, ret;
	u8 temp;

	ret = gc1084_read_reg(sensor, GC1084_REG_AEC_PK_EXPOSURE_HI, &temp);
	if (ret)
		return ret;
	exp = ((int)temp & 0x0f) << 16;
	ret = gc1084_read_reg(sensor, GC1084_REG_AEC_PK_EXPOSURE_MED, &temp);
	if (ret)
		return ret;
	exp |= ((int)temp << 8);
	ret = gc1084_read_reg(sensor, GC1084_REG_AEC_PK_EXPOSURE_LO, &temp);
	if (ret)
		return ret;
	exp |= (int)temp;

	return exp >> 4;
}

/* write exposure, given number of line periods */
static int gc1084_set_exposure(struct gc1084_dev *sensor, u32 exposure)
{
	int ret;

	exposure <<= 4;

	ret = gc1084_write_reg(sensor,
			       GC1084_REG_AEC_PK_EXPOSURE_LO,
			       exposure & 0xff);
	if (ret)
		return ret;
	ret = gc1084_write_reg(sensor,
			       GC1084_REG_AEC_PK_EXPOSURE_MED,
			       (exposure >> 8) & 0xff);
	if (ret)
		return ret;
	return gc1084_write_reg(sensor,
				GC1084_REG_AEC_PK_EXPOSURE_HI,
				(exposure >> 16) & 0x0f);
}

static int gc1084_get_gain(struct gc1084_dev *sensor)
{
	u16 gain;
	int ret;

	ret = gc1084_read_reg16(sensor, GC1084_REG_AEC_PK_REAL_GAIN, &gain);
	if (ret)
		return ret;

	return gain & 0x3ff;
}

static int gc1084_set_gain(struct gc1084_dev *sensor, int gain)
{
	return gc1084_write_reg16(sensor, GC1084_REG_AEC_PK_REAL_GAIN,
				  (u16)gain & 0x3ff);
}

static int gc1084_set_autogain(struct gc1084_dev *sensor, bool on)
{
	return gc1084_mod_reg(sensor, GC1084_REG_AEC_PK_MANUAL,
			      BIT(1), on ? 0 : BIT(1));
}

static int gc1084_set_stream_dvp(struct gc1084_dev *sensor, bool on)
{
	return gc1084_write_reg(sensor, GC1084_REG_SYS_CTRL0, on ?
				GC1084_REG_SYS_CTRL0_SW_PWUP :
				GC1084_REG_SYS_CTRL0_SW_PWDN);
}

static int gc1084_set_stream_mipi(struct gc1084_dev *sensor, bool on)
{
	const struct gc1084_mode_info *mode;
	u8 line_sync;
	int ret;

	mode = sensor->current_mode;
	line_sync = (mode->id == GC1084_MODE_XGA_1024_768 ||
		     mode->id == GC1084_MODE_QSXGA_2592_1944) ? 0 : 1;
	ret = gc1084_write_reg(sensor, GC1084_REG_MIPI_CTRL00,
			       0x24 | (line_sync << 4));
	if (ret)
		return ret;

	/*
	 * Enable/disable the MIPI interface
	 *
	 * 0x300e = on ? 0x45 : 0x40
	 *
	 * FIXME: the sensor manual (version 2.03) reports
	 * [7:5] = 000  : 1 data lane mode
	 * [7:5] = 001  : 2 data lanes mode
	 * But this settings do not work, while the following ones
	 * have been validated for 2 data lanes mode.
	 *
	 * [7:5] = 010	: 2 data lanes mode
	 * [4] = 0	: Power up MIPI HS Tx
	 * [3] = 0	: Power up MIPI LS Rx
	 * [2] = 1/0	: MIPI interface enable/disable
	 * [1:0] = 01/00: FIXME: 'debug'
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_IO_MIPI_CTRL00,
			       on ? 0x45 : 0x40);
	if (ret)
		return ret;

	ret = gc1084_write_reg(sensor, GC1084_REG_FRAME_CTRL01,
				on ? 0x00 : 0x0f);
	if (ret)
		return ret;

	ret = gc1084_write_reg(sensor, GC1084_REG_SYS_CTRL0,
				on ? 0x02 : 0x42);
	if (ret)
		return ret;

	msleep(100);
	return ret;
}

static int gc1084_get_sysclk(struct gc1084_dev *sensor)
{
	 /* calculate sysclk */
	u32 xvclk = sensor->xclk_freq / 10000;
	u32 multiplier, prediv, VCO, sysdiv, pll_rdiv;
	u32 sclk_rdiv_map[] = {1, 2, 4, 8};
	u32 bit_div2x = 1, sclk_rdiv, sysclk;
	u8 temp1, temp2;
	int ret;

	ret = gc1084_read_reg(sensor, GC1084_REG_SC_PLL_CTRL0, &temp1);
	if (ret)
		return ret;
	temp2 = temp1 & 0x0f;
	if (temp2 == 8 || temp2 == 10)
		bit_div2x = temp2 / 2;

	ret = gc1084_read_reg(sensor, GC1084_REG_SC_PLL_CTRL1, &temp1);
	if (ret)
		return ret;
	sysdiv = temp1 >> 4;
	if (sysdiv == 0)
		sysdiv = 16;

	ret = gc1084_read_reg(sensor, GC1084_REG_SC_PLL_CTRL2, &temp1);
	if (ret)
		return ret;
	multiplier = temp1;

	ret = gc1084_read_reg(sensor, GC1084_REG_SC_PLL_CTRL3, &temp1);
	if (ret)
		return ret;
	prediv = temp1 & 0x0f;
	pll_rdiv = ((temp1 >> 4) & 0x01) + 1;

	ret = gc1084_read_reg(sensor, GC1084_REG_SYS_ROOT_DIVIDER, &temp1);
	if (ret)
		return ret;
	temp2 = temp1 & 0x03;
	sclk_rdiv = sclk_rdiv_map[temp2];

	if (!prediv || !sysdiv || !pll_rdiv || !bit_div2x)
		return -EINVAL;

	VCO = xvclk * multiplier / prediv;

	sysclk = VCO / sysdiv / pll_rdiv * 2 / bit_div2x / sclk_rdiv;

	return sysclk;
}

static int gc1084_set_night_mode(struct gc1084_dev *sensor)
{
	 /* read HTS from register settings */
	u8 mode;
	int ret;

	ret = gc1084_read_reg(sensor, GC1084_REG_AEC_CTRL00, &mode);
	if (ret)
		return ret;
	mode &= 0xfb;
	return gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL00, mode);
}

static int gc1084_get_hts(struct gc1084_dev *sensor)
{
	/* read HTS from register settings */
	u16 hts;
	int ret;

	ret = gc1084_read_reg16(sensor, GC1084_REG_TIMING_HTS, &hts);
	if (ret)
		return ret;
	return hts;
}

static int gc1084_get_vts(struct gc1084_dev *sensor)
{
	u16 vts;
	int ret;

	ret = gc1084_read_reg16(sensor, GC1084_REG_TIMING_VTS, &vts);
	if (ret)
		return ret;
	return vts;
}

static int gc1084_set_vts(struct gc1084_dev *sensor, int vts)
{
	return gc1084_write_reg16(sensor, GC1084_REG_TIMING_VTS, vts);
}

static int gc1084_get_light_freq(struct gc1084_dev *sensor)
{
	/* get banding filter value */
	int ret, light_freq = 0;
	u8 temp, temp1;

	ret = gc1084_read_reg(sensor, GC1084_REG_HZ5060_CTRL01, &temp);
	if (ret)
		return ret;

	if (temp & 0x80) {
		/* manual */
		ret = gc1084_read_reg(sensor, GC1084_REG_HZ5060_CTRL00,
				      &temp1);
		if (ret)
			return ret;
		if (temp1 & 0x04) {
			/* 50Hz */
			light_freq = 50;
		} else {
			/* 60Hz */
			light_freq = 60;
		}
	} else {
		/* auto */
		ret = gc1084_read_reg(sensor, GC1084_REG_SIGMADELTA_CTRL0C,
				      &temp1);
		if (ret)
			return ret;

		if (temp1 & 0x01) {
			/* 50Hz */
			light_freq = 50;
		} else {
			/* 60Hz */
		}
	}

	return light_freq;
}

static int gc1084_set_bandingfilter(struct gc1084_dev *sensor)
{
	u32 band_step60, max_band60, band_step50, max_band50, prev_vts;
	int ret;

	/* read preview PCLK */
	ret = gc1084_get_sysclk(sensor);
	if (ret < 0)
		return ret;
	if (ret == 0)
		return -EINVAL;
	sensor->prev_sysclk = ret;
	/* read preview HTS */
	ret = gc1084_get_hts(sensor);
	if (ret < 0)
		return ret;
	if (ret == 0)
		return -EINVAL;
	sensor->prev_hts = ret;

	/* read preview VTS */
	ret = gc1084_get_vts(sensor);
	if (ret < 0)
		return ret;
	prev_vts = ret;

	/* calculate banding filter */
	/* 60Hz */
	band_step60 = sensor->prev_sysclk * 100 / sensor->prev_hts * 100 / 120;
	ret = gc1084_write_reg16(sensor, GC1084_REG_AEC_B60_STEP, band_step60);
	if (ret)
		return ret;
	if (!band_step60)
		return -EINVAL;
	max_band60 = (int)((prev_vts - 4) / band_step60);
	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL0D, max_band60);
	if (ret)
		return ret;

	/* 50Hz */
	band_step50 = sensor->prev_sysclk * 100 / sensor->prev_hts;
	ret = gc1084_write_reg16(sensor, GC1084_REG_AEC_B50_STEP, band_step50);
	if (ret)
		return ret;
	if (!band_step50)
		return -EINVAL;
	max_band50 = (int)((prev_vts - 4) / band_step50);
	return gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL0E, max_band50);
}

static int gc1084_set_ae_target(struct gc1084_dev *sensor, int target)
{
	/* stable in high */
	u32 fast_high, fast_low;
	int ret;

	sensor->ae_low = target * 23 / 25;	/* 0.92 */
	sensor->ae_high = target * 27 / 25;	/* 1.08 */

	fast_high = sensor->ae_high << 1;
	if (fast_high > 255)
		fast_high = 255;

	fast_low = sensor->ae_low >> 1;

	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL0F, sensor->ae_high);
	if (ret)
		return ret;
	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL10, sensor->ae_low);
	if (ret)
		return ret;
	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL1B, sensor->ae_high);
	if (ret)
		return ret;
	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL1E, sensor->ae_low);
	if (ret)
		return ret;
	ret = gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL11, fast_high);
	if (ret)
		return ret;
	return gc1084_write_reg(sensor, GC1084_REG_AEC_CTRL1F, fast_low);
}

static int gc1084_get_binning(struct gc1084_dev *sensor)
{
	u8 temp;
	int ret;

	ret = gc1084_read_reg(sensor, GC1084_REG_TIMING_TC_REG21, &temp);
	if (ret)
		return ret;

	return temp & BIT(0);
}

static int gc1084_set_binning(struct gc1084_dev *sensor, bool enable)
{
	int ret;

	/*
	 * TIMING TC REG21:
	 * - [0]:	Horizontal binning enable
	 */
	ret = gc1084_mod_reg(sensor, GC1084_REG_TIMING_TC_REG21,
			     BIT(0), enable ? BIT(0) : 0);
	if (ret)
		return ret;
	/*
	 * TIMING TC REG20:
	 * - [0]:	Undocumented, but hardcoded init sequences
	 *		are always setting REG21/REG20 bit 0 to same value...
	 */
	return gc1084_mod_reg(sensor, GC1084_REG_TIMING_TC_REG20,
			      BIT(0), enable ? BIT(0) : 0);
}

static int gc1084_set_virtual_channel(struct gc1084_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	u8 temp, channel = virtual_channel;
	int ret;

	if (channel > 3) {
		dev_err(&client->dev,
			"%s: wrong virtual_channel parameter, expected (0..3), got %d\n",
			__func__, channel);
		return -EINVAL;
	}

	ret = gc1084_read_reg(sensor, GC1084_REG_DEBUG_MODE, &temp);
	if (ret)
		return ret;
	temp &= ~(3 << 6);
	temp |= (channel << 6);
	return gc1084_write_reg(sensor, GC1084_REG_DEBUG_MODE, temp);
}

static const struct gc1084_mode_info *
gc1084_find_mode(struct gc1084_dev *sensor, enum gc1084_frame_rate fr,
		 int width, int height, bool nearest)
{
	const struct gc1084_mode_info *mode;

	mode = v4l2_find_nearest_size(gc1084_mode_data,
				      ARRAY_SIZE(gc1084_mode_data),
				      hact, vact,
				      width, height);

	if (!mode ||
	    (!nearest && (mode->hact != width || mode->vact != height))) {
		return NULL;
	}

	return mode;
}

static u64 gc1084_calc_pixel_rate(struct gc1084_dev *sensor)
{
	u64 rate;

	rate = sensor->current_mode->vtot * sensor->current_mode->htot;
	rate *= gc1084_framerates[sensor->current_fr];

	return rate;
}

/*
 * sensor changes between scaling and subsampling, go through
 * exposure calculation
 */
static int gc1084_set_mode_exposure_calc(struct gc1084_dev *sensor,
					 const struct gc1084_mode_info *mode)
{
	u32 prev_shutter, prev_gain16;
	u32 cap_shutter, cap_gain16;
	u32 cap_sysclk, cap_hts, cap_vts;
	u32 light_freq, cap_bandfilt, cap_maxband;
	u32 cap_gain16_shutter;
	u8 average;
	int ret;

	if (!mode->reg_data)
		return -EINVAL;

	/* read preview shutter */
	ret = gc1084_get_exposure(sensor);
	if (ret < 0)
		return ret;
	prev_shutter = ret;
	ret = gc1084_get_binning(sensor);
	if (ret < 0)
		return ret;
	if (ret && mode->id != GC1084_MODE_720P_1280_720 &&
	    mode->id != GC1084_MODE_1080P_1920_1080)
		prev_shutter *= 2;

	/* read preview gain */
	ret = gc1084_get_gain(sensor);
	if (ret < 0)
		return ret;
	prev_gain16 = ret;

	/* get average */
	ret = gc1084_read_reg(sensor, GC1084_REG_AVG_READOUT, &average);
	if (ret)
		return ret;

	/* turn off night mode for capture */
	ret = gc1084_set_night_mode(sensor);
	if (ret < 0)
		return ret;

	/* Write capture setting */
	ret = gc1084_load_regs(sensor, mode);
	if (ret < 0)
		return ret;

	/* read capture VTS */
	ret = gc1084_get_vts(sensor);
	if (ret < 0)
		return ret;
	cap_vts = ret;
	ret = gc1084_get_hts(sensor);
	if (ret < 0)
		return ret;
	if (ret == 0)
		return -EINVAL;
	cap_hts = ret;

	ret = gc1084_get_sysclk(sensor);
	if (ret < 0)
		return ret;
	if (ret == 0)
		return -EINVAL;
	cap_sysclk = ret;

	/* calculate capture banding filter */
	ret = gc1084_get_light_freq(sensor);
	if (ret < 0)
		return ret;
	light_freq = ret;

	if (light_freq == 60) {
		/* 60Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_hts * 100 / 120;
	} else {
		/* 50Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_hts;
	}

	if (!sensor->prev_sysclk) {
		ret = gc1084_get_sysclk(sensor);
		if (ret < 0)
			return ret;
		if (ret == 0)
			return -EINVAL;
		sensor->prev_sysclk = ret;
	}

	if (!cap_bandfilt)
		return -EINVAL;

	cap_maxband = (int)((cap_vts - 4) / cap_bandfilt);

	/* calculate capture shutter/gain16 */
	if (average > sensor->ae_low && average < sensor->ae_high) {
		/* in stable range */
		cap_gain16_shutter =
			prev_gain16 * prev_shutter *
			cap_sysclk / sensor->prev_sysclk *
			sensor->prev_hts / cap_hts *
			sensor->ae_target / average;
	} else {
		cap_gain16_shutter =
			prev_gain16 * prev_shutter *
			cap_sysclk / sensor->prev_sysclk *
			sensor->prev_hts / cap_hts;
	}

	/* gain to shutter */
	if (cap_gain16_shutter < (cap_bandfilt * 16)) {
		/* shutter < 1/100 */
		cap_shutter = cap_gain16_shutter / 16;
		if (cap_shutter < 1)
			cap_shutter = 1;

		cap_gain16 = cap_gain16_shutter / cap_shutter;
		if (cap_gain16 < 16)
			cap_gain16 = 16;
	} else {
		if (cap_gain16_shutter > (cap_bandfilt * cap_maxband * 16)) {
			/* exposure reach max */
			cap_shutter = cap_bandfilt * cap_maxband;
			if (!cap_shutter)
				return -EINVAL;

			cap_gain16 = cap_gain16_shutter / cap_shutter;
		} else {
			/* 1/100 < (cap_shutter = n/100) =< max */
			cap_shutter =
				((int)(cap_gain16_shutter / 16 / cap_bandfilt))
				* cap_bandfilt;
			if (!cap_shutter)
				return -EINVAL;

			cap_gain16 = cap_gain16_shutter / cap_shutter;
		}
	}

	/* set capture gain */
	ret = gc1084_set_gain(sensor, cap_gain16);
	if (ret)
		return ret;

	/* write capture shutter */
	if (cap_shutter > (cap_vts - 4)) {
		cap_vts = cap_shutter + 4;
		ret = gc1084_set_vts(sensor, cap_vts);
		if (ret < 0)
			return ret;
	}

	/* set exposure */
	return gc1084_set_exposure(sensor, cap_shutter);
}

/*
 * if sensor changes inside scaling or subsampling
 * change mode directly
 */
static int gc1084_set_mode_direct(struct gc1084_dev *sensor,
				  const struct gc1084_mode_info *mode)
{
	if (!mode->reg_data)
		return -EINVAL;

	/* Write capture setting */
	return gc1084_load_regs(sensor, mode);
}

static int gc1084_set_mode(struct gc1084_dev *sensor)
{
	const struct gc1084_mode_info *mode = sensor->current_mode;
	const struct gc1084_mode_info *orig_mode = sensor->last_mode;
	enum gc1084_downsize_mode dn_mode, orig_dn_mode;
	bool auto_gain = sensor->ctrls.auto_gain->val == 1;
	bool auto_exp =  sensor->ctrls.auto_exp->val == V4L2_EXPOSURE_AUTO;
	unsigned long rate;
	int ret;

	dn_mode = mode->dn_mode;
	orig_dn_mode = orig_mode->dn_mode;

	/* auto gain and exposure must be turned off when changing modes */
	if (auto_gain) {
		ret = gc1084_set_autogain(sensor, false);
		if (ret)
			return ret;
	}

	if (auto_exp) {
		ret = gc1084_set_autoexposure(sensor, false);
		if (ret)
			goto restore_auto_gain;
	}

	/*
	 * All the formats we support have 16 bits per pixel, seems to require
	 * the same rate than YUV, so we can just use 16 bpp all the time.
	 */
	rate = gc1084_calc_pixel_rate(sensor) * 16;
	if (sensor->ep.bus_type == V4L2_MBUS_CSI2_DPHY) {
		rate = rate / sensor->ep.bus.mipi_csi2.num_data_lanes;
		ret = gc1084_set_mipi_pclk(sensor, rate);
	} else {
		rate = rate / sensor->ep.bus.parallel.bus_width;
		ret = gc1084_set_dvp_pclk(sensor, rate);
	}

	if (ret < 0)
		return 0;

	if ((dn_mode == SUBSAMPLING && orig_dn_mode == SCALING) ||
	    (dn_mode == SCALING && orig_dn_mode == SUBSAMPLING)) {
		/*
		 * change between subsampling and scaling
		 * go through exposure calculation
		 */
		ret = gc1084_set_mode_exposure_calc(sensor, mode);
	} else {
		/*
		 * change inside subsampling or scaling
		 * download firmware directly
		 */
		ret = gc1084_set_mode_direct(sensor, mode);
	}
	if (ret < 0)
		goto restore_auto_exp_gain;

	/* restore auto gain and exposure */
	if (auto_gain)
		gc1084_set_autogain(sensor, true);
	if (auto_exp)
		gc1084_set_autoexposure(sensor, true);

	ret = gc1084_set_binning(sensor, dn_mode != SCALING);
	if (ret < 0)
		return ret;
	ret = gc1084_set_ae_target(sensor, sensor->ae_target);
	if (ret < 0)
		return ret;
	ret = gc1084_get_light_freq(sensor);
	if (ret < 0)
		return ret;
	ret = gc1084_set_bandingfilter(sensor);
	if (ret < 0)
		return ret;
	ret = gc1084_set_virtual_channel(sensor);
	if (ret < 0)
		return ret;

	sensor->pending_mode_change = false;
	sensor->last_mode = mode;

	return 0;

restore_auto_exp_gain:
	if (auto_exp)
		gc1084_set_autoexposure(sensor, true);
restore_auto_gain:
	if (auto_gain)
		gc1084_set_autogain(sensor, true);

	return ret;
}

static int gc1084_set_framefmt(struct gc1084_dev *sensor,
			       struct v4l2_mbus_framefmt *format);

/* restore the last set video mode after chip power-on */
static int gc1084_restore_mode(struct gc1084_dev *sensor)
{
	int ret;

	/* first load the initial register values */
	ret = gc1084_load_regs(sensor, &gc1084_mode_init_data);
	if (ret < 0)
		return ret;
	sensor->last_mode = &gc1084_mode_init_data;

	ret = gc1084_mod_reg(sensor, GC1084_REG_SYS_ROOT_DIVIDER, 0x3f,
			     (ilog2(GC1084_SCLK2X_ROOT_DIV) << 2) |
			     ilog2(GC1084_SCLK_ROOT_DIV));
	if (ret)
		return ret;

	/* now restore the last capture mode */
	ret = gc1084_set_mode(sensor);
	if (ret < 0)
		return ret;

	return gc1084_set_framefmt(sensor, &sensor->fmt);
}

static void gc1084_power(struct gc1084_dev *sensor, bool enable)
{
	gpiod_set_value_cansleep(sensor->pwdn_gpio, enable ? 0 : 1);
}

static void gc1084_reset(struct gc1084_dev *sensor)
{
	if (!sensor->reset_gpio)
		return;

	gpiod_set_value_cansleep(sensor->reset_gpio, 0);

	/* camera power cycle */
	gc1084_power(sensor, false);
	usleep_range(5000, 10000);
	gc1084_power(sensor, true);
	usleep_range(5000, 10000);

	gpiod_set_value_cansleep(sensor->reset_gpio, 1);
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	usleep_range(20000, 25000);
}

static int gc1084_set_power_on(struct gc1084_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret;

	ret = clk_prepare_enable(sensor->xclk);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable clock\n",
			__func__);
		return ret;
	}

	ret = regulator_bulk_enable(GC1084_NUM_SUPPLIES,
				    sensor->supplies);
	if (ret) {
		dev_err(&client->dev, "%s: failed to enable regulators\n",
			__func__);
		goto xclk_off;
	}

	gc1084_reset(sensor);
	gc1084_power(sensor, true);

	ret = gc1084_init_slave_id(sensor);
	if (ret)
		goto power_off;

	return 0;

power_off:
	gc1084_power(sensor, false);
	regulator_bulk_disable(GC1084_NUM_SUPPLIES, sensor->supplies);
xclk_off:
	clk_disable_unprepare(sensor->xclk);
	return ret;
}

static void gc1084_set_power_off(struct gc1084_dev *sensor)
{
	gc1084_power(sensor, false);
	regulator_bulk_disable(GC1084_NUM_SUPPLIES, sensor->supplies);
	clk_disable_unprepare(sensor->xclk);
	sensor->streaming = false;
}

static int gc1084_set_power_mipi(struct gc1084_dev *sensor, bool on)
{
	int ret;

	if (!on) {
		/* Reset MIPI bus settings to their default values. */
		gc1084_write_reg(sensor, GC1084_REG_IO_MIPI_CTRL00, 0x40);
		gc1084_write_reg(sensor, GC1084_REG_MIPI_CTRL00, 0x04);
		gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT00, 0x00);
		return 0;
	}

	/*
	 * Power up MIPI HS Tx and LS Rx; 2 data lanes mode
	 *
	 * 0x300e = 0x40
	 * [7:5] = 010	: 2 data lanes mode (see FIXME note in
	 *		  "gc1084_set_stream_mipi()")
	 * [4] = 0	: Power up MIPI HS Tx
	 * [3] = 0	: Power up MIPI LS Rx
	 * [2] = 0	: MIPI interface disabled
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_IO_MIPI_CTRL00, 0x45);
	if (ret)
		return ret;

	/*
	 * Gate clock and set LP11 in 'no packets mode' (idle)
	 *
	 * 0x4800 = 0x24
	 * [5] = 1	: Gate clock when 'no packets'
	 * [4] = 1	: Line sync enable
	 * [2] = 1	: MIPI bus in LP11 when 'no packets'
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_MIPI_CTRL00, 0x34);
	if (ret)
		return ret;

	/*
	 * Set data lanes and clock in LP11 when 'sleeping'
	 *
	 * 0x3019 = 0x70
	 * [6] = 1	: MIPI data lane 2 in LP11 when 'sleeping'
	 * [5] = 1	: MIPI data lane 1 in LP11 when 'sleeping'
	 * [4] = 1	: MIPI clock lane in LP11 when 'sleeping'
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT00, 0x70);
	if (ret)
		return ret;

	/* Give lanes some time to coax into LP11 state. */
	usleep_range(500, 1000);

	return 0;
}

static int gc1084_set_power_dvp(struct gc1084_dev *sensor, bool on)
{
	unsigned int flags = sensor->ep.bus.parallel.flags;
	bool bt656 = sensor->ep.bus_type == V4L2_MBUS_BT656;
	u8 polarities = 0;
	int ret;

	if (!on) {
		/* Reset settings to their default values. */
		gc1084_write_reg(sensor, GC1084_REG_CCIR656_CTRL00, 0x00);
		gc1084_write_reg(sensor, GC1084_REG_IO_MIPI_CTRL00, 0x00);
		gc1084_write_reg(sensor, GC1084_REG_POLARITY_CTRL00, 0x20);
		gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT_ENABLE01, 0x00);
		gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT_ENABLE02, 0x00);
		return 0;
	}

	/*
	 * Note about parallel port configuration.
	 *
	 * When configured in parallel mode, the GC1084 will
	 * output 10 bits data on DVP data lines [9:0].
	 * If only 8 bits data are wanted, the 8 bits data lines
	 * of the camera interface must be physically connected
	 * on the DVP data lines [9:2].
	 *
	 * Control lines polarity can be configured through
	 * devicetree endpoint control lines properties.
	 * If no endpoint control lines properties are set,
	 * polarity will be as below:
	 * - VSYNC:	active high
	 * - HREF:	active low
	 * - PCLK:	active low
	 *
	 * VSYNC & HREF are not configured if BT656 bus mode is selected
	 */

	/*
	 * BT656 embedded synchronization configuration
	 *
	 * CCIR656 CTRL00
	 * - [7]:	SYNC code selection (0: auto generate sync code,
	 *		1: sync code from regs 0x4732-0x4735)
	 * - [6]:	f value in CCIR656 SYNC code when fixed f value
	 * - [5]:	Fixed f value
	 * - [4:3]:	Blank toggle data options (00: data=1'h040/1'h200,
	 *		01: data from regs 0x4736-0x4738, 10: always keep 0)
	 * - [1]:	Clip data disable
	 * - [0]:	CCIR656 mode enable
	 *
	 * Default CCIR656 SAV/EAV mode with default codes
	 * SAV=0xff000080 & EAV=0xff00009d is enabled here with settings:
	 * - CCIR656 mode enable
	 * - auto generation of sync codes
	 * - blank toggle data 1'h040/1'h200
	 * - clip reserved data (0x00 & 0xff changed to 0x01 & 0xfe)
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_CCIR656_CTRL00,
			       bt656 ? 0x01 : 0x00);
	if (ret)
		return ret;

	/*
	 * configure parallel port control lines polarity
	 *
	 * POLARITY CTRL0
	 * - [5]:	PCLK polarity (0: active low, 1: active high)
	 * - [1]:	HREF polarity (0: active low, 1: active high)
	 * - [0]:	VSYNC polarity (mismatch here between
	 *		datasheet and hardware, 0 is active high
	 *		and 1 is active low...)
	 */
	if (!bt656) {
		if (flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH)
			polarities |= BIT(1);
		if (flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)
			polarities |= BIT(0);
	}
	if (flags & V4L2_MBUS_PCLK_SAMPLE_RISING)
		polarities |= BIT(5);

	ret = gc1084_write_reg(sensor, GC1084_REG_POLARITY_CTRL00, polarities);
	if (ret)
		return ret;

	/*
	 * powerdown MIPI TX/RX PHY & enable DVP
	 *
	 * MIPI CONTROL 00
	 * [4] = 1	: Power down MIPI HS Tx
	 * [3] = 1	: Power down MIPI LS Rx
	 * [2] = 0	: DVP enable (MIPI disable)
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_IO_MIPI_CTRL00, 0x58);
	if (ret)
		return ret;

	/*
	 * enable VSYNC/HREF/PCLK DVP control lines
	 * & D[9:6] DVP data lines
	 *
	 * PAD OUTPUT ENABLE 01
	 * - 6:		VSYNC output enable
	 * - 5:		HREF output enable
	 * - 4:		PCLK output enable
	 * - [3:0]:	D[9:6] output enable
	 */
	ret = gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT_ENABLE01,
			       bt656 ? 0x1f : 0x7f);
	if (ret)
		return ret;

	/*
	 * enable D[5:0] DVP data lines
	 *
	 * PAD OUTPUT ENABLE 02
	 * - [7:2]:	D[5:0] output enable
	 */
	return gc1084_write_reg(sensor, GC1084_REG_PAD_OUTPUT_ENABLE02, 0xfc);
}

static int gc1084_set_power(struct gc1084_dev *sensor, bool on)
{
	int ret = 0;

	if (on) {
		ret = gc1084_set_power_on(sensor);
		if (ret)
			return ret;

		ret = gc1084_restore_mode(sensor);
		if (ret)
			goto power_off;
	}

	if (sensor->ep.bus_type == V4L2_MBUS_CSI2_DPHY)
		ret = gc1084_set_power_mipi(sensor, on);
	else
		ret = gc1084_set_power_dvp(sensor, on);
	if (ret)
		goto power_off;

	if (!on)
		gc1084_set_power_off(sensor);

	return 0;

power_off:
	gc1084_set_power_off(sensor);
	return ret;
}

/* --------------- Subdev Operations --------------- */

static int gc1084_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	int ret = 0;

	mutex_lock(&sensor->lock);

	/*
	 * If the power count is modified from 0 to != 0 or from != 0 to 0,
	 * update the power state.
	 */
	if (sensor->power_count == !on) {
		ret = gc1084_set_power(sensor, !!on);
		if (ret)
			goto out;
	}

	/* Update the power count. */
	sensor->power_count += on ? 1 : -1;
	WARN_ON(sensor->power_count < 0);
out:
	mutex_unlock(&sensor->lock);

	if (on && !ret && sensor->power_count == 1) {
		/* restore controls */
		ret = v4l2_ctrl_handler_setup(&sensor->ctrls.handler);
	}

	return ret;
}

static int gc1084_try_frame_interval(struct gc1084_dev *sensor,
				     struct v4l2_fract *fi,
				     u32 width, u32 height)
{
	const struct gc1084_mode_info *mode;
	enum gc1084_frame_rate rate = GC1084_08_FPS;
	int minfps, maxfps, best_fps, fps;
	int i;

	minfps = gc1084_framerates[GC1084_08_FPS];
	maxfps = gc1084_framerates[GC1084_30_FPS];

	if (fi->numerator == 0) {
		fi->denominator = maxfps;
		fi->numerator = 1;
		rate = GC1084_30_FPS;
		goto find_mode;
	}

	fps = clamp_val(DIV_ROUND_CLOSEST(fi->denominator, fi->numerator),
			minfps, maxfps);

	best_fps = minfps;
	for (i = 0; i < ARRAY_SIZE(gc1084_framerates); i++) {
		int curr_fps = gc1084_framerates[i];

		if (abs(curr_fps - fps) < abs(best_fps - fps)) {
			best_fps = curr_fps;
			rate = i;
		}
	}

	fi->numerator = 1;
	fi->denominator = best_fps;

find_mode:
	mode = gc1084_find_mode(sensor, rate, width, height, false);
	return mode ? rate : -EINVAL;
}

static int gc1084_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	struct v4l2_mbus_framefmt *fmt;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		fmt = v4l2_subdev_get_try_format(&sensor->sd, sd_state,
						 format->pad);
	else
		fmt = &sensor->fmt;

	fmt->reserved[1] = (sensor->current_fr == GC1084_30_FPS) ? 30 : 15;
	format->format = *fmt;

	mutex_unlock(&sensor->lock);
	return 0;
}

static int gc1084_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   enum gc1084_frame_rate fr,
				   const struct gc1084_mode_info **new_mode)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	const struct gc1084_mode_info *mode;
	int i;

	mode = gc1084_find_mode(sensor, fr, fmt->width, fmt->height, true);
	if (!mode)
		return -EINVAL;
	fmt->width = mode->hact;
	fmt->height = mode->vact;
	memset(fmt->reserved, 0, sizeof(fmt->reserved));

	if (new_mode)
		*new_mode = mode;

	for (i = 0; i < ARRAY_SIZE(gc1084_formats); i++)
		if (gc1084_formats[i].code == fmt->code)
			break;
	if (i >= ARRAY_SIZE(gc1084_formats))
		i = 0;

	fmt->code = gc1084_formats[i].code;
	fmt->colorspace = gc1084_formats[i].colorspace;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);

	return 0;
}

static int gc1084_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	const struct gc1084_mode_info *new_mode;
	struct v4l2_mbus_framefmt *mbus_fmt = &format->format;
	int ret;

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	if (sensor->streaming) {
		ret = -EBUSY;
		goto out;
	}

	ret = gc1084_try_fmt_internal(sd, mbus_fmt,
				      sensor->current_fr, &new_mode);
	if (ret)
		goto out;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, sd_state, 0) = *mbus_fmt;
		goto out;
	}

	if (new_mode != sensor->current_mode) {
		sensor->current_mode = new_mode;
		sensor->pending_mode_change = true;
	}
	if (mbus_fmt->code != sensor->fmt.code)
		sensor->pending_fmt_change = true;

	/* update format even if code is unchanged, resolution might change */
	sensor->fmt = *mbus_fmt;

	__v4l2_ctrl_s_ctrl_int64(sensor->ctrls.pixel_rate,
				 gc1084_calc_pixel_rate(sensor));

	if (sensor->pending_mode_change || sensor->pending_fmt_change)
		sensor->fmt = *mbus_fmt;
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static int gc1084_set_framefmt(struct gc1084_dev *sensor,
			       struct v4l2_mbus_framefmt *format)
{
	int ret = 0;
	bool is_jpeg = false;
	u8 fmt, mux;

	switch (format->code) {
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_UYVY8_2X8:
		/* YUV422, UYVY */
		fmt = 0x3f;
		mux = GC1084_FMT_MUX_YUV422;
		break;
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YUYV8_2X8:
		/* YUV422, YUYV */
		fmt = 0x30;
		mux = GC1084_FMT_MUX_YUV422;
		break;
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		/* RGB565 {g[2:0],b[4:0]},{r[4:0],g[5:3]} */
		fmt = 0x6F;
		mux = GC1084_FMT_MUX_RGB;
		break;
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
		/* RGB565 {r[4:0],g[5:3]},{g[2:0],b[4:0]} */
		fmt = 0x61;
		mux = GC1084_FMT_MUX_RGB;
		break;
	case MEDIA_BUS_FMT_JPEG_1X8:
		/* YUV422, YUYV */
		fmt = 0x30;
		mux = GC1084_FMT_MUX_YUV422;
		is_jpeg = true;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
		/* Raw, BGBG... / GRGR... */
		fmt = 0x00;
		mux = GC1084_FMT_MUX_RAW_DPC;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
		/* Raw bayer, GBGB... / RGRG... */
		fmt = 0x01;
		mux = GC1084_FMT_MUX_RAW_DPC;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
		/* Raw bayer, GRGR... / BGBG... */
		fmt = 0x02;
		mux = GC1084_FMT_MUX_RAW_DPC;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		/* Raw bayer, RGRG... / GBGB... */
		fmt = 0x03;
		mux = GC1084_FMT_MUX_RAW_DPC;
		break;
	default:
		return -EINVAL;
	}

	/* FORMAT CONTROL00: YUV and RGB formatting */
	ret = gc1084_write_reg(sensor, GC1084_REG_FORMAT_CONTROL00, fmt);
	if (ret)
		return ret;

	/* FORMAT MUX CONTROL: ISP YUV or RGB */
	ret = gc1084_write_reg(sensor, GC1084_REG_ISP_FORMAT_MUX_CTRL, mux);
	if (ret)
		return ret;

	/*
	 * TIMING TC REG21:
	 * - [5]:	JPEG enable
	 */
	ret = gc1084_mod_reg(sensor, GC1084_REG_TIMING_TC_REG21,
			     BIT(5), is_jpeg ? BIT(5) : 0);
	if (ret)
		return ret;

	/*
	 * SYSTEM RESET02:
	 * - [4]:	Reset JFIFO
	 * - [3]:	Reset SFIFO
	 * - [2]:	Reset JPEG
	 */
	ret = gc1084_mod_reg(sensor, GC1084_REG_SYS_RESET02,
			     BIT(4) | BIT(3) | BIT(2),
			     is_jpeg ? 0 : (BIT(4) | BIT(3) | BIT(2)));
	if (ret)
		return ret;

	/*
	 * CLOCK ENABLE02:
	 * - [5]:	Enable JPEG 2x clock
	 * - [3]:	Enable JPEG clock
	 */
	return gc1084_mod_reg(sensor, GC1084_REG_SYS_CLOCK_ENABLE02,
			      BIT(5) | BIT(3),
			      is_jpeg ? (BIT(5) | BIT(3)) : 0);
}

/*
 * Sensor Controls.
 */

static int gc1084_set_ctrl_hue(struct gc1084_dev *sensor, int value)
{
	int ret;

	if (value) {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0,
				     BIT(0), BIT(0));
		if (ret)
			return ret;
		ret = gc1084_write_reg16(sensor, GC1084_REG_SDE_CTRL1, value);
	} else {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0, BIT(0), 0);
	}

	return ret;
}

static int gc1084_set_ctrl_contrast(struct gc1084_dev *sensor, int value)
{
	int ret;

	if (value) {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0,
				     BIT(2), BIT(2));
		if (ret)
			return ret;
		ret = gc1084_write_reg(sensor, GC1084_REG_SDE_CTRL5,
				       value & 0xff);
	} else {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0, BIT(2), 0);
	}

	return ret;
}

static int gc1084_set_ctrl_saturation(struct gc1084_dev *sensor, int value)
{
	int ret;

	if (value) {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0,
				     BIT(1), BIT(1));
		if (ret)
			return ret;
		ret = gc1084_write_reg(sensor, GC1084_REG_SDE_CTRL3,
				       value & 0xff);
		if (ret)
			return ret;
		ret = gc1084_write_reg(sensor, GC1084_REG_SDE_CTRL4,
				       value & 0xff);
	} else {
		ret = gc1084_mod_reg(sensor, GC1084_REG_SDE_CTRL0, BIT(1), 0);
	}

	return ret;
}

static int gc1084_set_ctrl_white_balance(struct gc1084_dev *sensor, int awb)
{
	int ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_AWB_MANUAL_CTRL,
			     BIT(0), awb ? 0 : 1);
	if (ret)
		return ret;

	if (!awb) {
		u16 red = (u16)sensor->ctrls.red_balance->val;
		u16 blue = (u16)sensor->ctrls.blue_balance->val;

		ret = gc1084_write_reg16(sensor, GC1084_REG_AWB_R_GAIN, red);
		if (ret)
			return ret;
		ret = gc1084_write_reg16(sensor, GC1084_REG_AWB_B_GAIN, blue);
	}

	return ret;
}

static int gc1084_set_ctrl_exposure(struct gc1084_dev *sensor,
				    enum v4l2_exposure_auto_type auto_exposure)
{
	struct gc1084_ctrls *ctrls = &sensor->ctrls;
	bool auto_exp = (auto_exposure == V4L2_EXPOSURE_AUTO);
	int ret = 0;

	if (ctrls->auto_exp->is_new) {
		ret = gc1084_set_autoexposure(sensor, auto_exp);
		if (ret)
			return ret;
	}

	if (!auto_exp && ctrls->exposure->is_new) {
		u16 max_exp;

		ret = gc1084_read_reg16(sensor, GC1084_REG_AEC_PK_VTS,
					&max_exp);
		if (ret)
			return ret;
		ret = gc1084_get_vts(sensor);
		if (ret < 0)
			return ret;
		max_exp += ret;
		ret = 0;

		if (ctrls->exposure->val < max_exp)
			ret = gc1084_set_exposure(sensor, ctrls->exposure->val);
	}

	return ret;
}

static int gc1084_set_ctrl_gain(struct gc1084_dev *sensor, bool auto_gain)
{
	struct gc1084_ctrls *ctrls = &sensor->ctrls;
	int ret = 0;

	if (ctrls->auto_gain->is_new) {
		ret = gc1084_set_autogain(sensor, auto_gain);
		if (ret)
			return ret;
	}

	if (!auto_gain && ctrls->gain->is_new)
		ret = gc1084_set_gain(sensor, ctrls->gain->val);

	return ret;
}

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Color bars",
	"Color bars w/ rolling bar",
	"Color squares",
	"Color squares w/ rolling bar",
};

#define GC1084_TEST_ENABLE		BIT(7)
#define GC1084_TEST_ROLLING		BIT(6)	/* rolling horizontal bar */
#define GC1084_TEST_TRANSPARENT		BIT(5)
#define GC1084_TEST_SQUARE_BW		BIT(4)	/* black & white squares */
#define GC1084_TEST_BAR_STANDARD	(0 << 2)
#define GC1084_TEST_BAR_VERT_CHANGE_1	(1 << 2)
#define GC1084_TEST_BAR_HOR_CHANGE	(2 << 2)
#define GC1084_TEST_BAR_VERT_CHANGE_2	(3 << 2)
#define GC1084_TEST_BAR			(0 << 0)
#define GC1084_TEST_RANDOM		(1 << 0)
#define GC1084_TEST_SQUARE		(2 << 0)
#define GC1084_TEST_BLACK		(3 << 0)

static const u8 test_pattern_val[] = {
	0,
	GC1084_TEST_ENABLE | GC1084_TEST_BAR_VERT_CHANGE_1 |
		GC1084_TEST_BAR,
	GC1084_TEST_ENABLE | GC1084_TEST_ROLLING |
		GC1084_TEST_BAR_VERT_CHANGE_1 | GC1084_TEST_BAR,
	GC1084_TEST_ENABLE | GC1084_TEST_SQUARE,
	GC1084_TEST_ENABLE | GC1084_TEST_ROLLING | GC1084_TEST_SQUARE,
};

static int gc1084_set_ctrl_test_pattern(struct gc1084_dev *sensor, int value)
{
	return gc1084_write_reg(sensor, GC1084_REG_PRE_ISP_TEST_SET1,
				test_pattern_val[value]);
}

static int gc1084_set_ctrl_light_freq(struct gc1084_dev *sensor, int value)
{
	int ret;

	ret = gc1084_mod_reg(sensor, GC1084_REG_HZ5060_CTRL01, BIT(7),
			     (value == V4L2_CID_POWER_LINE_FREQUENCY_AUTO) ?
			     0 : BIT(7));
	if (ret)
		return ret;

	return gc1084_mod_reg(sensor, GC1084_REG_HZ5060_CTRL00, BIT(2),
			      (value == V4L2_CID_POWER_LINE_FREQUENCY_50HZ) ?
			      BIT(2) : 0);
}

static int gc1084_set_ctrl_hflip(struct gc1084_dev *sensor, int value)
{
	/*
	 * If sensor is mounted upside down, mirror logic is inversed.
	 *
	 * Sensor is a BSI (Back Side Illuminated) one,
	 * so image captured is physically mirrored.
	 * This is why mirror logic is inversed in
	 * order to cancel this mirror effect.
	 */

	/*
	 * TIMING TC REG21:
	 * - [2]:	ISP mirror
	 * - [1]:	Sensor mirror
	 */
	return gc1084_mod_reg(sensor, GC1084_REG_TIMING_TC_REG21,
			      BIT(2) | BIT(1),
			      (!(value ^ sensor->upside_down)) ?
			      (BIT(2) | BIT(1)) : 0);
}

static int gc1084_set_ctrl_vflip(struct gc1084_dev *sensor, int value)
{
	/* If sensor is mounted upside down, flip logic is inversed */

	/*
	 * TIMING TC REG20:
	 * - [2]:	ISP vflip
	 * - [1]:	Sensor vflip
	 */
	return gc1084_mod_reg(sensor, GC1084_REG_TIMING_TC_REG20,
			      BIT(2) | BIT(1),
			      (value ^ sensor->upside_down) ?
			      (BIT(2) | BIT(1)) : 0);
}

static int gc1084_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	int val;

	/* v4l2_ctrl_lock() locks our own mutex */

	switch (ctrl->id) {
	case V4L2_CID_AUTOGAIN:
		val = gc1084_get_gain(sensor);
		if (val < 0)
			return val;
		sensor->ctrls.gain->val = val;
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		val = gc1084_get_exposure(sensor);
		if (val < 0)
			return val;
		sensor->ctrls.exposure->val = val;
		break;
	}

	return 0;
}

static int gc1084_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	int ret;

	/* v4l2_ctrl_lock() locks our own mutex */

	/*
	 * If the device is not powered up by the host driver do
	 * not apply any controls to H/W at this time. Instead
	 * the controls will be restored right after power-up.
	 */
	if (sensor->power_count == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_AUTOGAIN:
		ret = gc1084_set_ctrl_gain(sensor, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ret = gc1084_set_ctrl_exposure(sensor, ctrl->val);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		ret = gc1084_set_ctrl_white_balance(sensor, ctrl->val);
		break;
	case V4L2_CID_HUE:
		ret = gc1084_set_ctrl_hue(sensor, ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		ret = gc1084_set_ctrl_contrast(sensor, ctrl->val);
		break;
	case V4L2_CID_SATURATION:
		ret = gc1084_set_ctrl_saturation(sensor, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = gc1084_set_ctrl_test_pattern(sensor, ctrl->val);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ret = gc1084_set_ctrl_light_freq(sensor, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = gc1084_set_ctrl_hflip(sensor, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ret = gc1084_set_ctrl_vflip(sensor, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops gc1084_ctrl_ops = {
	.g_volatile_ctrl = gc1084_g_volatile_ctrl,
	.s_ctrl = gc1084_s_ctrl,
};

static int gc1084_init_controls(struct gc1084_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &gc1084_ctrl_ops;
	struct gc1084_ctrls *ctrls = &sensor->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret;

	v4l2_ctrl_handler_init(hdl, 32);

	/* we can use our own mutex for the ctrl lock */
	hdl->lock = &sensor->lock;

	/* Clock related controls */
	ctrls->pixel_rate = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_PIXEL_RATE,
					      0, INT_MAX, 1,
					      gc1084_calc_pixel_rate(sensor));

	/* Auto/manual white balance */
	ctrls->auto_wb = v4l2_ctrl_new_std(hdl, ops,
					   V4L2_CID_AUTO_WHITE_BALANCE,
					   0, 1, 1, 1);
	ctrls->blue_balance = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_BLUE_BALANCE,
						0, 4095, 1, 0);
	ctrls->red_balance = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_RED_BALANCE,
					       0, 4095, 1, 0);
	/* Auto/manual exposure */
	ctrls->auto_exp = v4l2_ctrl_new_std_menu(hdl, ops,
						 V4L2_CID_EXPOSURE_AUTO,
						 V4L2_EXPOSURE_MANUAL, 0,
						 V4L2_EXPOSURE_AUTO);
	ctrls->exposure = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_EXPOSURE,
					    0, 65535, 1, 0);
	/* Auto/manual gain */
	ctrls->auto_gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_AUTOGAIN,
					     0, 1, 1, 1);
	ctrls->gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_GAIN,
					0, 1023, 1, 0);

	ctrls->saturation = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_SATURATION,
					      0, 255, 1, 64);
	ctrls->hue = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HUE,
				       0, 359, 1, 0);
	ctrls->contrast = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_CONTRAST,
					    0, 255, 1, 0);
	ctrls->test_pattern =
		v4l2_ctrl_new_std_menu_items(hdl, ops, V4L2_CID_TEST_PATTERN,
					     ARRAY_SIZE(test_pattern_menu) - 1,
					     0, 0, test_pattern_menu);
	ctrls->hflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HFLIP,
					 0, 1, 1, 0);
	ctrls->vflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_VFLIP,
					 0, 1, 1, 0);

	ctrls->light_freq =
		v4l2_ctrl_new_std_menu(hdl, ops,
				       V4L2_CID_POWER_LINE_FREQUENCY,
				       V4L2_CID_POWER_LINE_FREQUENCY_AUTO, 0,
				       V4L2_CID_POWER_LINE_FREQUENCY_50HZ);

	if (hdl->error) {
		ret = hdl->error;
		goto free_ctrls;
	}

	ctrls->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	ctrls->gain->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->exposure->flags |= V4L2_CTRL_FLAG_VOLATILE;

	v4l2_ctrl_auto_cluster(3, &ctrls->auto_wb, 0, false);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_gain, 0, true);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_exp, 1, true);

	sensor->sd.ctrl_handler = hdl;
	return 0;

free_ctrls:
	v4l2_ctrl_handler_free(hdl);
	return ret;
}

static int gc1084_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->pad != 0)
		return -EINVAL;
	if (fse->index >= GC1084_NUM_MODES)
		return -EINVAL;

	fse->min_width =
		gc1084_mode_data[fse->index].hact;
	fse->max_width = fse->min_width;
	fse->min_height =
		gc1084_mode_data[fse->index].vact;
	fse->max_height = fse->min_height;

	return 0;
}

static int gc1084_enum_frame_interval(
	struct v4l2_subdev *sd,
	struct v4l2_subdev_state *sd_state,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	int i, j, count;

	if (fie->pad != 0)
		return -EINVAL;
	if (fie->index >= GC1084_NUM_FRAMERATES)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 || fie->code == 0) {
		pr_warn("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < GC1084_NUM_FRAMERATES; i++) {
		for (j = 0; j < GC1084_NUM_MODES; j++) {
			if (fie->width  == gc1084_mode_data[j].hact &&
			    fie->height == gc1084_mode_data[j].vact &&
			    !gc1084_check_valid_mode(sensor, &gc1084_mode_data[j], i))
				count++;

			if (fie->index == (count - 1)) {
				fie->interval.denominator = gc1084_framerates[i];
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int gc1084_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);

	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int gc1084_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	const struct gc1084_mode_info *mode;
	int frame_rate, ret = 0;

	if (fi->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	if (sensor->streaming) {
		ret = -EBUSY;
		goto out;
	}

	mode = sensor->current_mode;

	frame_rate = gc1084_try_frame_interval(sensor, &fi->interval,
					       mode->hact, mode->vact);
	if (frame_rate < 0) {
		/* Always return a valid frame interval value */
		fi->interval = sensor->frame_interval;
		goto out;
	}

	mode = gc1084_find_mode(sensor, frame_rate, mode->hact,
				mode->vact, true);
	if (!mode) {
		ret = -EINVAL;
		goto out;
	}

	if (mode != sensor->current_mode ||
	    frame_rate != sensor->current_fr) {
		sensor->current_fr = frame_rate;
		sensor->frame_interval = fi->interval;
		sensor->current_mode = mode;
		sensor->pending_mode_change = true;

		__v4l2_ctrl_s_ctrl_int64(sensor->ctrls.pixel_rate,
					 gc1084_calc_pixel_rate(sensor));
	}
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static int gc1084_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad != 0)
		return -EINVAL;
	if (code->index >= ARRAY_SIZE(gc1084_formats))
		return -EINVAL;

	code->code = gc1084_formats[code->index].code;
	return 0;
}

static int gc1084_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc1084_dev *sensor = to_gc1084_dev(sd);
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;

	printk("gc1084_s_stream\n");

	mutex_lock(&sensor->lock);

	if (sensor->streaming == !enable) {
		ret = gc1084_check_valid_mode(sensor,
					      sensor->current_mode,
					      sensor->current_fr);
		if (ret) {
			dev_err(&client->dev, "Not support WxH@fps=%dx%d@%d\n",
				sensor->current_mode->hact,
				sensor->current_mode->vact,
				gc1084_framerates[sensor->current_fr]);
			goto out;
		}

		if (enable && sensor->pending_mode_change) {
			ret = gc1084_set_mode(sensor);
			if (ret)
				goto out;
		}

		if (enable && sensor->pending_fmt_change) {
			ret = gc1084_set_framefmt(sensor, &sensor->fmt);
			if (ret)
				goto out;
			sensor->pending_fmt_change = false;
		}

		if (sensor->ep.bus_type == V4L2_MBUS_CSI2_DPHY)
			ret = gc1084_set_stream_mipi(sensor, enable);
		else
			ret = gc1084_set_stream_dvp(sensor, enable);

		if (!ret)
			sensor->streaming = enable;
	}
out:
	mutex_unlock(&sensor->lock);
	return ret;
}

static const struct v4l2_subdev_core_ops gc1084_core_ops = {
	.s_power = gc1084_s_power,
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops gc1084_video_ops = {
	.g_frame_interval = gc1084_g_frame_interval,
	.s_frame_interval = gc1084_s_frame_interval,
	.s_stream = gc1084_s_stream,
};

static const struct v4l2_subdev_pad_ops gc1084_pad_ops = {
	.enum_mbus_code = gc1084_enum_mbus_code,
	.get_fmt = gc1084_get_fmt,
	.set_fmt = gc1084_set_fmt,
	.enum_frame_size = gc1084_enum_frame_size,
	.enum_frame_interval = gc1084_enum_frame_interval,
};

static const struct v4l2_subdev_ops gc1084_subdev_ops = {
	.core = &gc1084_core_ops,
	.video = &gc1084_video_ops,
	.pad = &gc1084_pad_ops,
};

static int gc1084_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations gc1084_sd_media_ops = {
	.link_setup = gc1084_link_setup,
};

static int gc1084_get_regulators(struct gc1084_dev *sensor)
{
	int i;

	for (i = 0; i < GC1084_NUM_SUPPLIES; i++)
		sensor->supplies[i].supply = gc1084_supply_name[i];

	return devm_regulator_bulk_get(&sensor->i2c_client->dev,
				       GC1084_NUM_SUPPLIES,
				       sensor->supplies);
}

static int gc1084_check_chip_id(struct gc1084_dev *sensor)
{
	struct i2c_client *client = sensor->i2c_client;
	int ret = 0;
	u8 id_h = 0, id_l = 0;
	u16 id = 0;

	ret = gc1084_set_power_on(sensor);
	if (ret)
		return ret;

	ret = gc1084_read_reg(sensor, GC1084_REG_CHIP_ID_H, &id_h);
	ret |= gc1084_read_reg(sensor, GC1084_REG_CHIP_ID_L, &id_l);
	if (ret) {
		return ret;
	}

	id = id_h << 8 | id_l;
	if (id != GC1084_CHIP_ID) {
		return -ENODEV;
	}

	printk("Detected GC1084 sensor\n");

	return ret;
}

static int gc1084_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct fwnode_handle *endpoint;
	struct gc1084_dev *sensor;
	struct v4l2_mbus_framefmt *fmt;
	u32 rotation;
	int ret;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;

	/*
	 * default init sequence initialize sensor to
	 * YUV422 UYVY VGA@30fps
	 */
	fmt = &sensor->fmt;
	fmt->code = MEDIA_BUS_FMT_UYVY8_2X8;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
	fmt->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(fmt->colorspace);
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(fmt->colorspace);
	fmt->width = 640;
	fmt->height = 480;
	fmt->field = V4L2_FIELD_NONE;
	sensor->frame_interval.numerator = 1;
	sensor->frame_interval.denominator = gc1084_framerates[GC1084_30_FPS];
	sensor->current_fr = GC1084_30_FPS;
	sensor->current_mode =
		&gc1084_mode_data[GC1084_MODE_VGA_640_480];
	sensor->last_mode = sensor->current_mode;

	sensor->ae_target = 52;

	/* optional indication of physical rotation of sensor */
	ret = fwnode_property_read_u32(dev_fwnode(&client->dev), "rotation",
				       &rotation);
	if (!ret) {
		switch (rotation) {
		case 180:
			sensor->upside_down = true;
			fallthrough;
		case 0:
			break;
		default:
			dev_warn(dev, "%u degrees rotation is not supported, ignoring...\n",
				 rotation);
		}
	}

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(&client->dev),
						  NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(endpoint, &sensor->ep);
	fwnode_handle_put(endpoint);
	if (ret) {
		dev_err(dev, "Could not parse endpoint\n");
		return ret;
	}

	if (sensor->ep.bus_type != V4L2_MBUS_PARALLEL &&
	    sensor->ep.bus_type != V4L2_MBUS_CSI2_DPHY &&
	    sensor->ep.bus_type != V4L2_MBUS_BT656) {
		dev_err(dev, "Unsupported bus type %d\n", sensor->ep.bus_type);
		return -EINVAL;
	}

	/* get system clock (xclk) */
	sensor->xclk = devm_clk_get(dev, "xclk");
	if (IS_ERR(sensor->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(sensor->xclk);
	}

	sensor->xclk_freq = clk_get_rate(sensor->xclk);
	if (sensor->xclk_freq < GC1084_XCLK_MIN ||
	    sensor->xclk_freq > GC1084_XCLK_MAX) {
		dev_err(dev, "xclk frequency out of range: %d Hz\n",
			sensor->xclk_freq);
		return -EINVAL;
	}

	/* request optional power down pin */
	sensor->pwdn_gpio = devm_gpiod_get_optional(dev, "powerdown",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(sensor->pwdn_gpio))
		return PTR_ERR(sensor->pwdn_gpio);

	/* request optional reset pin */
	sensor->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(sensor->reset_gpio))
		return PTR_ERR(sensor->reset_gpio);

	v4l2_i2c_subdev_init(&sensor->sd, client, &gc1084_subdev_ops);

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.ops = &gc1084_sd_media_ops;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		return ret;

	ret = gc1084_get_regulators(sensor);
	if (ret)
		return ret;

	mutex_init(&sensor->lock);

	ret = gc1084_check_chip_id(sensor);
	if (ret)
		goto entity_cleanup;

	ret = gc1084_init_controls(sensor);
	if (ret)
		goto entity_cleanup;

	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
		goto free_ctrls;

	return 0;

free_ctrls:
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);
entity_cleanup:
	media_entity_cleanup(&sensor->sd.entity);
	mutex_destroy(&sensor->lock);
	return ret;
}

static int gc1084_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc1084_dev *sensor = to_gc1084_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);
	mutex_destroy(&sensor->lock);

	return 0;
}

static const struct i2c_device_id gc1084_id[] = {
	{"gc1084", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, gc1084_id);

static const struct of_device_id gc1084_dt_ids[] = {
	{ .compatible = "galaxycore,gc1084" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gc1084_dt_ids);

static struct i2c_driver gc1084_i2c_driver = {
	.driver = {
		.name  = "gc1084",
		.of_match_table	= gc1084_dt_ids,
	},
	.id_table = gc1084_id,
	.probe_new = gc1084_probe,
	.remove   = gc1084_remove,
};

module_i2c_driver(gc1084_i2c_driver);

MODULE_DESCRIPTION("GC1084 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");
