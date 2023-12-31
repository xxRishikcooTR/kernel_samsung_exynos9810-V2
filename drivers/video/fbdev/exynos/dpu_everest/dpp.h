/* linux/drivers/video/fbdev/exynos/dpu/dpp.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * header file for Samsung EXYNOS SoC DPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DPP_H__
#define __SAMSUNG_DPP_H__

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/exynos_iovmm.h>
#include <soc/samsung/bts.h>

#include "decon.h"
#include "./cal_9810/regs-dpp.h"

extern int dpp_log_level;

#define DPP_MODULE_NAME		"exynos-dpp"
#define MAX_DPP_CNT		7
#define MAX_FMT_CNT		64
#define DEFAULT_FMT_CNT		9

/* about 1msec @ ACLK=630MHz */
#define INIT_RCV_NUM		630000

#define SRC_WIDTH_MIN		16
#define SRC_WIDTH_MAX		65534
#define SRC_HEIGHT_MIN		16
#define SRC_HEIGHT_MAX		8190

#define check_align(width, height, align_w, align_h)\
	(IS_ALIGNED(width, align_w) && IS_ALIGNED(height, align_h))

#define is_normal(config) (DPP_ROT_NORMAL)
#define is_rotation(config) (config->dpp_parm.rot > DPP_ROT_180)
#define is_yuv(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) \
			&& (config->format < DECON_PIXEL_FORMAT_MAX))
#define is_yuv422(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) \
			&& (config->format <= DECON_PIXEL_FORMAT_YVU422_3P))
#define is_yuv420(config) ((config->format >= DECON_PIXEL_FORMAT_NV12) \
			&& (config->format <= DECON_PIXEL_FORMAT_YVU420M))
#define is_vgr(dpp) ((dpp->id == IDMA_VGF0) || (dpp->id == IDMA_VGF1))

#define dpp_err(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dpp_warn(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dpp_info(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define dpp_dbg(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

/* TODO: This will be removed */
struct dpp_size_constraints {
	u32		src_mul_w;
	u32		src_mul_h;
	u32		src_w_min;
	u32		src_w_max;
	u32		src_h_min;
	u32		src_h_max;
	u32		img_mul_w;
	u32		img_mul_h;
	u32		img_w_min;
	u32		img_w_max;
	u32		img_h_min;
	u32		img_h_max;
	u32		blk_w_min;
	u32		blk_w_max;
	u32		blk_h_min;
	u32		blk_h_max;
	u32		blk_mul_w;
	u32		blk_mul_h;
	u32		src_mul_x;
	u32		src_mul_y;
	u32		sca_w_min;
	u32		sca_w_max;
	u32		sca_h_min;
	u32		sca_h_max;
	u32		sca_mul_w;
	u32		sca_mul_h;
	u32		dst_mul_w;
	u32		dst_mul_h;
	u32		dst_w_min;
	u32		dst_w_max;
	u32		dst_h_min;
	u32		dst_h_max;
	u32		dst_mul_x;
	u32		dst_mul_y;
};

struct dpp_img_format {
	u32		vgr;
	u32		normal;
	u32		rot;
	u32		scale;
	u32		format;
	u32		afbc_en;
	u32		yuv;
	u32		yuv422;
	u32		yuv420;
	u32		wb;
};

enum dpp_cfg_err {
	DPP_CFG_ERR_SCL_POS		= (1 << 4),
	DPP_CFG_ERR_SCALE_RATIO	= (1 << 3),
	DPP_CFG_ERR_ODD_SIZE		= (1 << 2),
	DPP_CFG_ERR_MAX_SIZE		= (1 << 1),
	DPP_CFG_ERR_MIN_SIZE		= (1 << 0),
};

enum dpp_csc_defs {
	/* csc_type */
	DPP_CSC_BT_601 = 0,
	DPP_CSC_BT_709 = 1,
	/* csc_range */
	DPP_CSC_NARROW = 0,
	DPP_CSC_WIDE = 1,
	/* csc_mode */
	CSC_COEF_HARDWIRED = 0,
	CSC_COEF_CUSTOMIZED = 1,
	/* csc_id used in csc_3x3_t[] : increase by even value */
	DPP_CSC_ID_BT_2020 = 0,
	DPP_CSC_ID_DCI_P3 = 2,
};

enum dpp_state {
	DPP_STATE_ON,
	DPP_STATE_OFF,
};

enum dpp_reg_area {
	REG_AREA_DPP = 0,
	REG_AREA_DMA,
	REG_AREA_DMA_COM,
};

enum dpp_attr {
	DPP_ATTR_AFBC		= 0,
	DPP_ATTR_BLOCK		= 1,
	DPP_ATTR_FLIP		= 2,
	DPP_ATTR_ROT		= 3,
	DPP_ATTR_CSC		= 4,
	DPP_ATTR_SCALE		= 5,
	DPP_ATTR_HDR		= 6,
	DPP_ATTR_HDR10		= 7,

	DPP_ATTR_IDMA		= 16,
	DPP_ATTR_ODMA		= 17,
	DPP_ATTR_DPP		= 18,
};

struct dpp_resources {
	struct clk *gate;
	void __iomem *regs;
	void __iomem *dma_regs;
	void __iomem *dma_com_regs;
	int irq;
	int dma_irq;
};

struct dpp_debug {
	struct timer_list op_timer;
	u32 done_count;
	u32 recovery_cnt;
};

struct dpp_size_range {
	u32 min;
	u32 max;
	u32 align;
};

struct dpp_restriction {
	struct dpp_size_range src_f_w;
	struct dpp_size_range src_f_h;
	struct dpp_size_range src_w;
	struct dpp_size_range src_h;
	u32 src_x_align;
	u32 src_y_align;

	struct dpp_size_range dst_f_w;
	struct dpp_size_range dst_f_h;
	struct dpp_size_range dst_w;
	struct dpp_size_range dst_h;
	u32 dst_x_align;
	u32 dst_y_align;

	struct dpp_size_range blk_w;
	struct dpp_size_range blk_h;
	u32 blk_x_align;
	u32 blk_y_align;

	u32 src_h_rot_max; /* limit of source img height in case of rotation */

	u32 format[MAX_FMT_CNT]; /* supported format list for each DPP channel */
	int format_cnt;

	u32 scale_down;
	u32 scale_up;

	u32 reserved[6];
};

struct dpp_ch_restriction {
	int id;
	unsigned long attr;

	struct dpp_restriction restriction;
	u32 reserved[4];
};

struct dpp_restrictions_info {
	u32 ver; /* version of dpp_restrictions_info structure */
	struct dpp_ch_restriction dpp_ch[MAX_DPP_CNT];
	int dpp_cnt;
	u32 reserved[4];
};

struct dpp_device {
	int id;
	unsigned long attr;
	enum dpp_state state;
	struct device *dev;
	struct v4l2_subdev sd;
	struct dpp_resources res;
	struct dpp_debug d;
	wait_queue_head_t framedone_wq;
	struct decon_win_config *config;
	spinlock_t slock;
	spinlock_t dma_slock;
	struct mutex lock;
	bool hold_rpm_on_boot;
	struct dpp_restriction restriction;
};

struct dpp_params_info {
	struct decon_frame src;
	struct decon_frame dst;
	struct decon_win_rect block;
	u32 rot;

	enum dpp_hdr_standard hdr;
	u32 min_luminance;
	u32 max_luminance;
	bool is_4p;
	u32 y_2b_strd;
	u32 c_2b_strd;

	bool is_comp;
	bool is_scale;
	bool is_block;
	enum decon_pixel_format format;
	dma_addr_t addr[MAX_PLANE_ADDR_CNT];
	enum dpp_csc_eq eq_mode;
	int h_ratio;
	int v_ratio;
};

extern struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static inline struct dpp_device *get_dpp_drvdata(u32 id)
{
	if (id >= MAX_DPP_CNT)
		return NULL;
	else
		return dpp_drvdata[id];
}

static inline u32 dpp_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	return readl(dpp->res.regs + reg_id);
}

static inline u32 dpp_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dpp_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void dpp_write(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	writel(val, dpp->res.regs + reg_id);
}

static inline void dpp_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 old = dpp_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.regs + reg_id);
}

/* DPU_DMA Common part */
static inline u32 dma_com_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(0);
	return readl(dpp->res.dma_com_regs + reg_id);
}

static inline u32 dma_com_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dma_com_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void dma_com_write(u32 id, u32 reg_id, u32 val)
{
	/* get reliable address when probing IDMA_G0 */
	struct dpp_device *dpp = get_dpp_drvdata(0);
	writel(val, dpp->res.dma_com_regs + reg_id);
}

static inline void dma_com_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(0);
	u32 old = dma_com_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.dma_com_regs + reg_id);
}

/* DPU_DMA */
static inline u32 dma_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	return readl(dpp->res.dma_regs + reg_id);
}

static inline u32 dma_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dma_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void dma_write(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	writel(val, dpp->res.dma_regs + reg_id);
}

static inline void dma_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 old = dma_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.dma_regs + reg_id);
}

static inline void dpp_select_format(struct dpp_device *dpp,
			struct dpp_img_format *vi, struct dpp_params_info *p)
{
	struct decon_win_config *config = dpp->config;

	vi->vgr = is_vgr(dpp);
	vi->normal = is_normal(dpp);
	vi->rot = p->rot;
	vi->scale = p->is_scale;
	vi->format = p->format;
	vi->afbc_en = p->is_comp;
	vi->yuv = is_yuv(config);
	vi->yuv422 = is_yuv422(config);
	vi->yuv420 = is_yuv420(config);
	vi->wb = test_bit(DPP_ATTR_ODMA, &dpp->attr);
}

void dpp_dump(struct dpp_device *dpp);
void dpp_release_rpm_hold(u32 id);

/* DPU DMA low-level APIs exposed to DPP driver */
u32 dma_reg_get_irq_status(u32 id, unsigned long attr);
void dma_reg_clear_irq(u32 id, u32 irq, unsigned long attr);

/* BIST mode */
void dma_reg_set_test_pattern(u32 id, u32 pat_id, u32 pat_dat[4]);
void dma_reg_set_ch_map(u32 id, u32 dpp_id, u32 to_pat);
void dma_reg_set_test_en(u32 id,u32 en);

struct dpp_restriction;

/* DPP low-level APIs exposed to DPP driver */
void dpp_reg_irq_enable(u32 id);
void dpp_reg_init(u32 id, unsigned long attr);
int dpp_reg_deinit(u32 id, bool reset, unsigned long attr);
void dpp_reg_configure_params(u32 id, struct dpp_params_info *p, unsigned long attr);
u32 dpp_reg_get_irq_status(u32 id);
void dpp_reg_clear_irq(u32 id, u32 irq);
void dpp_constraints_params(struct dpp_size_constraints *vc,
					struct dpp_img_format *vi, struct dpp_restriction *res);
int dpp_reg_wait_idle_status(int id, unsigned long timeout, unsigned long attr);
void dma_reg_set_recovery_num(u32 id, u32 rcv_num);

/* DPU DMA DEBUG */
void dma_reg_set_debug(u32 id);
void dma_reg_set_common_debug(u32 id);


#define DPP_WIN_CONFIG			_IOW('P', 0, struct decon_win_config)
#define DPP_STOP			_IOW('P', 1, unsigned long)
#define DPP_DUMP			_IOW('P', 2, u32)
#define DPP_WB_WAIT_FOR_FRAMEDONE	_IOR('P', 3, u32)
#define DPP_WAIT_IDLE			_IOR('P', 4, unsigned long)
#define DPP_SET_RECOVERY_NUM		_IOR('P', 5, unsigned long)
#define DPP_GET_RECOVERY_CNT		_IOR('P', 6, unsigned long)
#define DPP_GET_RESTRICTION		_IOR('P', 8, unsigned long)

#endif /* __SAMSUNG_DPP_H__ */
