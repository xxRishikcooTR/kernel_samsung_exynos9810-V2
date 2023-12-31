/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_enc_param.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_enc_param.h"

#include "mfc_reg.h"

/* Definition */
#define CBR_FIX_MAX			10
#define CBR_I_LIMIT_MAX			5
#define BPG_EXTENSION_TAG_SIZE		5

void s5p_mfc_set_slice_mode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	/* multi-slice control */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES)
		MFC_WRITEL((enc->slice_mode + 0x4), S5P_FIMV_E_MSLICE_MODE);
	else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW)
		MFC_WRITEL((enc->slice_mode - 0x2), S5P_FIMV_E_MSLICE_MODE);
	else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_FIXED_BYTES)
		MFC_WRITEL((enc->slice_mode + 0x3), S5P_FIMV_E_MSLICE_MODE);
	else
		MFC_WRITEL(enc->slice_mode, S5P_FIMV_E_MSLICE_MODE);

	/* multi-slice MB number or bit size */
	if ((enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) ||
			(enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW)) {
		MFC_WRITEL(enc->slice_size.mb, S5P_FIMV_E_MSLICE_SIZE_MB);
	} else if ((enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) ||
			(enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_FIXED_BYTES)){
		MFC_WRITEL(enc->slice_size.bits, S5P_FIMV_E_MSLICE_SIZE_BITS);
	} else {
		MFC_WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_MB);
		MFC_WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_BITS);
	}
}

void s5p_mfc_set_enc_ts_delta(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0;
	int ts_delta;

	ts_delta = mfc_enc_get_ts_delta(ctx);

	reg = MFC_READL(S5P_FIMV_E_TIME_STAMP_DELTA);
	reg &= ~(0xFFFF);
	reg |= (ts_delta & 0xFFFF);
	MFC_WRITEL(reg, S5P_FIMV_E_TIME_STAMP_DELTA);
	if (ctx->ts_last_interval)
		mfc_debug(3, "[DFR] fps %d -> %ld, delta: %d, reg: %#x\n",
				p->rc_framerate, USEC_PER_SEC / ctx->ts_last_interval,
				ts_delta, reg);
	else
		mfc_debug(3, "[DFR] fps %d -> 0, delta: %d, reg: %#x\n",
				p->rc_framerate, ts_delta, reg);
}

static void mfc_set_gop_size(struct s5p_mfc_ctx *ctx, int ctrl_mode)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0;

	if (ctrl_mode) {
		p->i_frm_ctrl_mode = 1;
		p->i_frm_ctrl = p->gop_size * (p->num_b_frame + 1);
		if (p->i_frm_ctrl >= 0x3FFFFFFF) {
			mfc_info_ctx("I frame interval is bigger than max: %d\n",
					p->i_frm_ctrl);
			p->i_frm_ctrl = 0x3FFFFFFF;
		}
	} else {
		p->i_frm_ctrl_mode = 0;
		p->i_frm_ctrl = p->gop_size;
	}

	mfc_debug(2, "I frame interval: %d, (P: %d, B: %d), ctrl mode: %d\n",
			p->i_frm_ctrl, p->gop_size,
			p->num_b_frame, p->i_frm_ctrl_mode);

	/* pictype : IDR period, number of B */
	reg = MFC_READL(S5P_FIMV_E_GOP_CONFIG);
	reg &= ~(0xFFFF);
	reg |= p->i_frm_ctrl & 0xFFFF;
	reg &= ~(0x1 << 19);
	reg |= p->i_frm_ctrl_mode << 19;
	reg &= ~(0x3 << 16);
	/* if B frame is used, the performance falls by half */
	reg |= (p->num_b_frame << 16);
	MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	reg = MFC_READL(S5P_FIMV_E_GOP_CONFIG2);
	reg &= ~(0x3FFF);
	reg |= (p->i_frm_ctrl >> 16) & 0x3FFF;
	MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG2);
}

static void mfc_set_default_params(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	/* Default setting for quality */
	/* Common Registers */
	MFC_WRITEL(0x0, S5P_FIMV_E_ENC_OPTIONS);
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	MFC_WRITEL(0x100, S5P_FIMV_E_MV_HOR_RANGE);
	MFC_WRITEL(0x100, S5P_FIMV_E_MV_VER_RANGE);
	MFC_WRITEL(0x0, S5P_FIMV_E_IR_SIZE);
	MFC_WRITEL(0x0, S5P_FIMV_E_AIR_THRESHOLD);
	MFC_WRITEL(0x1E, S5P_FIMV_E_GOP_CONFIG); /* I frame period: 30 */
	MFC_WRITEL(0x0, S5P_FIMV_E_GOP_CONFIG2);
	MFC_WRITEL(0x0, S5P_FIMV_E_MSLICE_MODE);

	/* Hierarchical Coding */
	MFC_WRITEL(0x8, S5P_FIMV_E_NUM_T_LAYER);

	/* Rate Control */
	MFC_WRITEL(0x4000, S5P_FIMV_E_RC_CONFIG);
	MFC_WRITEL(0x0, S5P_FIMV_E_RC_QP_BOUND);
	MFC_WRITEL(0x0, S5P_FIMV_E_RC_QP_BOUND_PB);
	MFC_WRITEL(0x12, S5P_FIMV_E_RC_MODE);
	MFC_WRITEL(0x1E0001, S5P_FIMV_E_RC_FRAME_RATE); /* framerate: 30 fps */
	MFC_WRITEL(0xF4240, S5P_FIMV_E_RC_BIT_RATE); /* bitrate: 1000000 */
	MFC_WRITEL(0x3FD00, S5P_FIMV_E_RC_ROI_CTRL);
	MFC_WRITEL(0x0, S5P_FIMV_E_VBV_BUFFER_SIZE);
	MFC_WRITEL(0x0, S5P_FIMV_E_VBV_INIT_DELAY);
	MFC_WRITEL(0x0, S5P_FIMV_E_BIT_COUNT_ENABLE);
	MFC_WRITEL(0x2710, S5P_FIMV_E_MAX_BIT_COUNT); /* max bit count: 10000 */
	MFC_WRITEL(0x3E8, S5P_FIMV_E_MIN_BIT_COUNT); /* min bit count: 1000 */
	/*
	 * If the high quality mode is used, the performance falls by half
	 * If the high quality mode is used, NAL-Q is not supported
	 */
	MFC_WRITEL(0x0, S5P_FIMV_E_HIGH_QUALITY_MODE);
	MFC_WRITEL(0x0, S5P_FIMV_E_WEIGHT_FOR_WEIGHTED_PREDICTION);

	/* HEVC */
	MFC_WRITEL(0x8050F215, S5P_FIMV_E_HEVC_OPTIONS);
	MFC_WRITEL(0x0, S5P_FIMV_E_HEVC_REFRESH_PERIOD);
	MFC_WRITEL(0x0, S5P_FIMV_E_HEVC_CHROMA_QP_OFFSET);
	MFC_WRITEL(0x0, S5P_FIMV_E_HEVC_LF_BETA_OFFSET_DIV2);
	MFC_WRITEL(0x0, S5P_FIMV_E_HEVC_LF_TC_OFFSET_DIV2);
	MFC_WRITEL(0x0, S5P_FIMV_E_SAO_WEIGHT0);
	MFC_WRITEL(0x0, S5P_FIMV_E_SAO_WEIGHT1);

	/* H.264 */
	MFC_WRITEL(0x3011, S5P_FIMV_E_H264_OPTIONS);
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_OPTIONS_2);
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_LF_ALPHA_OFFSET);
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_LF_BETA_OFFSET);
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_REFRESH_PERIOD);
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_CHROMA_QP_OFFSET);

	/* VP8 */
	MFC_WRITEL(0x0, S5P_FIMV_E_VP8_OPTION);
	MFC_WRITEL(0x0, S5P_FIMV_E_VP8_GOLDEN_FRAME_OPTION);

	/* VP9 */
	MFC_WRITEL(0x2D, S5P_FIMV_E_VP9_OPTION);
	MFC_WRITEL(0xA00, S5P_FIMV_E_VP9_FILTER_OPTION);
	MFC_WRITEL(0x3C, S5P_FIMV_E_VP9_GOLDEN_FRAME_OPTION);

	/* BPG */
	MFC_WRITEL(0x961, S5P_FIMV_E_BPG_OPTIONS);
	MFC_WRITEL(0x6FA00, S5P_FIMV_E_BPG_EXT_CTB_QP_CTRL);
	MFC_WRITEL(0x0, S5P_FIMV_E_BPG_CHROMA_QP_OFFSET);

	/* MVC */
	MFC_WRITEL(0x1D, S5P_FIMV_E_MVC_FRAME_QP_VIEW1); /* QP: 29 */
	MFC_WRITEL(0xF4240, S5P_FIMV_E_MVC_RC_BIT_RATE_VIEW1); /* bitrate: 1000000 */
	MFC_WRITEL(0x33003300, S5P_FIMV_E_MVC_RC_QBOUND_VIEW1); /* max I, P QP: 51 */
	MFC_WRITEL(0x2, S5P_FIMV_E_MVC_RC_MODE_VIEW1);
	MFC_WRITEL(0x1, S5P_FIMV_E_MVC_INTER_VIEW_PREDICTION_ON);

	/* Additional initialization: NAL start only */
	MFC_WRITEL(0x0, S5P_FIMV_E_FRAME_INSERTION);
	MFC_WRITEL(0x0, S5P_FIMV_E_ROI_BUFFER_ADDR);
	MFC_WRITEL(0x0, S5P_FIMV_E_PARAM_CHANGE);
	MFC_WRITEL(0x0, S5P_FIMV_E_PICTURE_TAG);
	MFC_WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_ADDR);
	MFC_WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_SIZE);
}

static void mfc_set_enc_params(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0;

	mfc_debug_enter();

	mfc_set_default_params(ctx);

	/* width */
	MFC_WRITEL(ctx->img_width, S5P_FIMV_E_FRAME_WIDTH); /* 16 align */
	/* height */
	MFC_WRITEL(ctx->img_height, S5P_FIMV_E_FRAME_HEIGHT); /* 16 align */
	/** cropped width */
	MFC_WRITEL(ctx->img_width, S5P_FIMV_E_CROPPED_FRAME_WIDTH);
	/** cropped height */
	MFC_WRITEL(ctx->img_height, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	/** cropped offset */
	MFC_WRITEL(0x0, S5P_FIMV_E_FRAME_CROP_OFFSET);

	/* multi-slice control */
	/* multi-slice MB number or bit size */
	enc->slice_mode = p->slice_mode;

	if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
		enc->slice_size.mb = p->slice_mb;
	} else if ((p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) ||
			(p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_FIXED_BYTES)){
		enc->slice_size.bits = p->slice_bit;
	} else if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW) {
		enc->slice_size.mb = p->slice_mb_row * ((ctx->img_width + 15) / 16);
	} else {
		enc->slice_size.mb = 0;
		enc->slice_size.bits = 0;
	}

	s5p_mfc_set_slice_mode(ctx);

	/* cyclic intra refresh */
	MFC_WRITEL(p->intra_refresh_mb, S5P_FIMV_E_IR_SIZE);

	reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
	/* frame skip mode */
	reg |= (p->frame_skip_mode & S5P_FIMV_E_ENC_OPT_FRAME_SKIP_EN_MASK);
	/* seq header ctrl */
	reg |= ((p->seq_hdr_mode & S5P_FIMV_E_ENC_OPT_SEQ_HEADER_CONTROL_MASK)
			<< S5P_FIMV_E_ENC_OPT_SEQ_HEADER_CONTROL_SHIFT);
	/* enable seq header generation */
	reg &= ~(0x1 << S5P_FIMV_E_ENC_OPT_DISABLE_SEQ_HEADER_SHIFT);
	/* disable seq header generation */
	if (ctx->otf_handle) {
		reg |= (0x1 << S5P_FIMV_E_ENC_OPT_DISABLE_SEQ_HEADER_SHIFT);
		mfc_debug(2, "OTF: SEQ_HEADER_GENERATION is disabled\n");
	}
	/* cyclic intra refresh */
	if (p->intra_refresh_mb)
		reg |= (0x1 << S5P_FIMV_E_ENC_OPT_IR_MODE_SHIFT);
	/* 'NON_REFERENCE_STORE_ENABLE' for debugging */
	reg &= ~(0x1 << S5P_FIMV_E_ENC_OPT_NON_REFERENCE_EN_SHIFT);
	/* Disable parallel processing if nal_q_parallel_disable was set */
	if (nal_q_parallel_disable)
		reg |= 0x1 << S5P_FIMV_E_ENC_OPT_PARALLEL_DISABLE_SHIFT;
	MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	s5p_mfc_set_pixel_format(dev, ctx->src_fmt->fourcc);

	/* padding control & value */
	MFC_WRITEL(0x0, S5P_FIMV_E_PADDING_CTRL);
	if (p->pad) {
		reg = 0;
		/** enable */
		reg |= (1 << 31);
		/** cr value */
		reg &= ~(0xFF << 16);
		reg |= (p->pad_cr << 16);
		/** cb value */
		reg &= ~(0xFF << 8);
		reg |= (p->pad_cb << 8);
		/** y value */
		reg &= ~(0xFF);
		reg |= (p->pad_luma);
		MFC_WRITEL(reg, S5P_FIMV_E_PADDING_CTRL);
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/* macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= ((p->rc_mb & 0x1) << 8);
	/* frame-level rate control */
	reg &= ~(0x1 << 9);
	reg |= ((p->rc_frame & 0x1) << 9);
	/* drop control */
	reg &= ~(0x1 << 10);
	reg |= ((p->drop_control & 0x1) << 10);
	if (MFC_FEATURE_SUPPORT(dev, dev->pdata->enc_ts_delta)) {
		reg &= ~(0x1 << 20);
		reg |= (0x1 << 20);
	}
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/*
	 * frame rate
	 * delta is timestamp diff
	 * ex) 30fps: 33, 60fps: 16
	 */
	p->rc_frame_delta = p->rc_framerate_res / p->rc_framerate;
	reg = MFC_READL(S5P_FIMV_E_RC_FRAME_RATE);
	reg &= ~(0xFFFF << 16);
	reg |= (p->rc_framerate_res << 16);
	reg &= ~(0xFFFF);
	reg |= (p->rc_frame_delta & 0xFFFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);

	/* bit rate */
	if (p->rc_bitrate)
		MFC_WRITEL(p->rc_bitrate, S5P_FIMV_E_RC_BIT_RATE);

	if (p->rc_frame) {
		reg = MFC_READL(S5P_FIMV_E_RC_MODE);
		reg &= ~(0x3);

		if (p->rc_reaction_coeff <= CBR_I_LIMIT_MAX) {
			reg |= S5P_FIMV_E_RC_CBR_I_LIMIT;
			/*
			 * Ratio of intra for max frame size
			 * is controled when only CBR_I_LIMIT mode.
			 * And CBR_I_LIMIT mode is valid for H.264, HEVC codec
			 */
			if (p->ratio_intra) {
				reg &= ~(0xFF << 8);
				reg |= ((p->ratio_intra & 0xff) << 8);
			}
		} else if (p->rc_reaction_coeff <= CBR_FIX_MAX) {
			reg |= S5P_FIMV_E_RC_CBR_FIX;
		} else {
			reg |= S5P_FIMV_E_RC_VBR;
		}

		if (p->rc_mb) {
			reg &= ~(0x3 << 4);
			reg |= ((p->rc_pvc & 0x3) << 4);
		}

		MFC_WRITEL(reg, S5P_FIMV_E_RC_MODE);
	}

	/* extended encoder ctrl */
	/** vbv buffer size */
	if (p->frame_skip_mode == V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT) {
		reg = MFC_READL(S5P_FIMV_E_VBV_BUFFER_SIZE);
		reg &= ~(0xFF);
		reg |= p->vbv_buf_size & 0xFF;
		MFC_WRITEL(reg, S5P_FIMV_E_VBV_BUFFER_SIZE);
	}

	mfc_debug_leave();
}

static void mfc_set_temporal_svc_h264(struct s5p_mfc_ctx *ctx, struct s5p_mfc_h264_enc_params *p_264)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0, reg2 = 0;
	int i;

	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS_2);
	/* pic_order_cnt_type = 0 for backward compatibilities */
	reg &= ~(0x3);
	/* Enable LTR */
	reg &= ~(0x1 << 2);
	if ((p_264->enable_ltr & 0x1) || (p_264->num_of_ltr > 0))
		reg |= (0x1 << 2);
	/* Number of LTR */
	reg &= ~(0x3 << 7);
	if (p_264->num_of_ltr > 2)
		reg |= (((p_264->num_of_ltr - 2) & 0x3) << 7);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS_2);

	/* Temporal SVC - qp type, layer number */
	reg = MFC_READL(S5P_FIMV_E_NUM_T_LAYER);
	reg &= ~(0x1 << 3);
	reg |= (p_264->hier_qp_type & 0x1) << 3;
	reg &= ~(0x7);
	reg |= p_264->num_hier_layer & 0x7;
	reg &= ~(0x7 << 4);
	if (p_264->hier_ref_type) {
		reg |= 0x1 << 7;
		reg |= (p->num_hier_max_layer & 0x7) << 4;
	} else {
		reg |= 0x7 << 4;
	}
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
	mfc_debug(3, "Temporal SVC: hier_qp_enable %d, enable_ltr %d, "
		"num_hier_layer %d, max_layer %d, hier_ref_type %d, NUM_T_LAYER 0x%x\n",
		p_264->hier_qp_enable, p_264->enable_ltr, p_264->num_hier_layer,
		p->num_hier_max_layer, p_264->hier_ref_type, reg);
	/* QP & Bitrate for each layer */
	for (i = 0; i < 7; i++) {
		MFC_WRITEL(p_264->hier_qp_layer[i],
				S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		MFC_WRITEL(p_264->hier_bit_layer[i],
				S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		mfc_debug(3, "Temporal SVC: layer[%d] QP: %#x, bitrate: %d\n",
					i, p_264->hier_qp_layer[i],
					p_264->hier_bit_layer[i]);
	}
	if (p_264->set_priority) {
		reg = 0;
		reg2 = 0;
		for (i = 0; i < (p_264->num_hier_layer & 0x7); i++) {
			if (i <= 4)
				reg |= ((p_264->base_priority & 0x3F) + i) << (6 * i);
			else
				reg2 |= ((p_264->base_priority & 0x3F) + i) << (6 * (i - 5));
		}
		MFC_WRITEL(reg, S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
		MFC_WRITEL(reg2, S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
		mfc_debug(3, "Temporal SVC: priority EXTENSION0: %#x, EXTENSION1: %#x\n",
							reg, reg2);
	}
}

static void mfc_set_fmo_slice_map_h264(struct s5p_mfc_ctx *ctx, struct s5p_mfc_h264_enc_params *p_264)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int i;

	if (p_264->fmo_enable) {
		switch (p_264->fmo_slice_map_type) {
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			for (i = 0; i < (p_264->fmo_slice_num_grp & 0xF); i++)
				MFC_WRITEL(p_264->fmo_run_length[i] - 1,
				S5P_FIMV_E_H264_FMO_RUN_LENGTH_MINUS1_0 + i*4);
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_SCATTERED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_RASTER_SCAN:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN:
			if (p_264->fmo_slice_num_grp > 2)
				p_264->fmo_slice_num_grp = 2;
			MFC_WRITEL(p_264->fmo_sg_dir & 0x1,
				S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_DIR);
			/* the valid range is 0 ~ number of macroblocks -1 */
			MFC_WRITEL(p_264->fmo_sg_rate, S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_RATE_MINUS1);
			break;
		default:
			mfc_err_ctx("Unsupported map type for FMO: %d\n",
					p_264->fmo_slice_map_type);
			p_264->fmo_slice_map_type = 0;
			p_264->fmo_slice_num_grp = 1;
			break;
		}

		MFC_WRITEL(p_264->fmo_slice_map_type, S5P_FIMV_E_H264_FMO_SLICE_GRP_MAP_TYPE);
		MFC_WRITEL(p_264->fmo_slice_num_grp - 1, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	} else {
		MFC_WRITEL(0, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	}
}

void s5p_mfc_set_enc_params_h264(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_h264_enc_params *p_264 = &p->codec.h264;
	unsigned int reg = 0;

	mfc_debug_enter();

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	if (p_264->num_hier_layer & 0x7) {
		/* set gop_size without i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 0);
	} else {
		/* set gop_size with i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 1);
	}

	/* UHD encoding case */
	if(IS_UHD_RES(ctx)) {
		if (p_264->level < 51) {
			mfc_info_ctx("Set Level 5.1 for UHD\n");
			p_264->level = 51;
		}
		if (p_264->profile != 0x2) {
			mfc_info_ctx("Set High profile for UHD\n");
			p_264->profile = 0x2;
		}
	}

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_264->level << 8);
	/** profile - 0 ~ 3 */
	reg &= ~(0x3F);
	reg |= p_264->profile;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	/* entropy coding mode */
	reg &= ~(0x1);
	reg |= (p_264->entropy_mode & 0x1);
	/* loop filter ctrl */
	reg &= ~(0x3 << 1);
	reg |= ((p_264->loop_filter_mode & 0x3) << 1);
	/* interlace */
	reg &= ~(0x1 << 3);
	reg |= ((p_264->interlace & 0x1) << 3);
	/* intra picture period for H.264 open GOP */
	reg &= ~(0x1 << 4);
	reg |= ((p_264->open_gop & 0x1) << 4);
	/* extended encoder ctrl */
	reg &= ~(0x1 << 5);
	reg |= ((p_264->ar_vui & 0x1) << 5);
	/* ASO enable */
	reg &= ~(0x1 << 6);
	reg |= ((p_264->aso_enable & 0x1) << 6);
	/* if num_refs_for_p is 2, the performance falls by half */
	reg &= ~(0x1 << 7);
	reg |= (((p->num_refs_for_p - 1) & 0x1) << 7);
	/* Temporal SVC - hier qp enable */
	reg &= ~(0x1 << 8);
	reg |= ((p_264->hier_qp_enable & 0x1) << 8);
	/* Weighted Prediction enable */
	reg &= ~(0x3 << 9);
	reg |= ((p->weighted_enable & 0x1) << 9);
	/* 8x8 transform enable */
	reg &= ~(0x1 << 12);
	reg &= ~(0x1 << 13);
	reg |= ((p_264->_8x8_transform & 0x1) << 12);
	reg |= ((p_264->_8x8_transform & 0x1) << 13);
	/* 'CONSTRAINED_INTRA_PRED_ENABLE' is disable */
	reg &= ~(0x1 << 14);
	/*
	 * CONSTRAINT_SET0_FLAG: all constraints specified in
	 * Baseline Profile
	 */
	reg |= (0x1 << 26);
	/* sps pps control */
	reg &= ~(0x1 << 29);
	reg |= ((p_264->prepend_sps_pps_to_idr & 0x1) << 29);
	/* enable sps pps control in OTF scenario */
	if (ctx->otf_handle) {
		reg |= (0x1 << 29);
		mfc_debug(2, "OTF: SPS_PPS_CONTROL enabled\n");
	}
	/* VUI parameter disable */
	reg &= ~(0x1 << 30);
	reg |= ((p_264->vui_enable & 0x1) << 30);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/** height */
	if (p_264->interlace) {
		MFC_WRITEL(ctx->img_height >> 1, S5P_FIMV_E_FRAME_HEIGHT); /* 32 align */
		/** cropped height */
		MFC_WRITEL(ctx->img_height >> 1, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	}

	/* loopfilter alpha offset */
	reg = MFC_READL(S5P_FIMV_E_H264_LF_ALPHA_OFFSET);
	reg &= ~(0x1F);
	reg |= (p_264->loop_filter_alpha & 0x1F);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_LF_ALPHA_OFFSET);

	/* loopfilter beta offset */
	reg = MFC_READL(S5P_FIMV_E_H264_LF_BETA_OFFSET);
	reg &= ~(0x1F);
	reg |= (p_264->loop_filter_beta & 0x1F);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_LF_BETA_OFFSET);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_264->rc_frame_qp & 0xFF);
	if (!p->rc_frame && !p->rc_mb && p->dynamic_qp)
		reg |= (0x1 << 11);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_264->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= p_264->rc_min_qp & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max B frame QP */
	reg &= ~(0xFF << 24);
	reg |= ((p_264->rc_max_qp_b & 0xFF) << 24);
	/** min B frame QP */
	reg &= ~(0xFF << 16);
	reg |= ((p_264->rc_min_qp_b & 0xFF) << 16);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_264->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_264->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 16);
	reg |= ((p_264->rc_b_frame_qp & 0xFF) << 16);
	reg &= ~(0xFF << 8);
	reg |= ((p_264->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_264->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	MFC_WRITEL(0x0, S5P_FIMV_E_ASPECT_RATIO);
	MFC_WRITEL(0x0, S5P_FIMV_E_EXTENDED_SAR);
	if (p_264->ar_vui) {
		/* aspect ration IDC */
		reg = 0;
		reg &= ~(0xff);
		reg |= p_264->ar_vui_idc;
		MFC_WRITEL(reg, S5P_FIMV_E_ASPECT_RATIO);
		if (p_264->ar_vui_idc == 0xFF) {
			/* sample  AR info. */
			reg = 0;
			reg &= ~(0xffffffff);
			reg |= p_264->ext_sar_width << 16;
			reg |= p_264->ext_sar_height;
			MFC_WRITEL(reg, S5P_FIMV_E_EXTENDED_SAR);
		}
	}
	/* intra picture period for H.264 open GOP, value */
	if (p_264->open_gop) {
		reg = MFC_READL(S5P_FIMV_E_H264_REFRESH_PERIOD);
		reg &= ~(0xFFFF);
		reg |= (p_264->open_gop_size & 0xFFFF);
		MFC_WRITEL(reg, S5P_FIMV_E_H264_REFRESH_PERIOD);
	}

	/* Temporal SVC */
	mfc_set_temporal_svc_h264(ctx, p_264);

	/* set frame pack sei generation */
	if (p_264->sei_gen_enable) {
		/* frame packing enable */
		reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
		reg |= (1 << 25);
		MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

		/* set current frame0 flag & arrangement type */
		reg = 0;
		/** current frame0 flag */
		reg |= ((p_264->sei_fp_curr_frame_0 & 0x1) << 2);
		/** arrangement type */
		reg |= (p_264->sei_fp_arrangement_type - 3) & 0x3;
		MFC_WRITEL(reg, S5P_FIMV_E_H264_FRAME_PACKING_SEI_INFO);
	}

	if (FW_HAS_ENC_COLOR_ASPECT(dev) && p->check_color_range) {
		reg = MFC_READL(S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		/* VIDEO_SIGNAL_TYPE_FLAG */
		reg |= 0x1 << 31;
		/* COLOR_RANGE */
		reg &= ~(0x1 << 25);
		reg |= p->color_range << 25;
		if ((p->colour_primaries != 0) && (p->transfer_characteristics != 0) &&
				(p->matrix_coefficients != 3)) {
			/* COLOUR_DESCRIPTION_PRESENT_FLAG */
			reg |= 0x1 << 24;
			/* COLOUR_PRIMARIES */
			reg &= ~(0xFF << 16);
			reg |= p->colour_primaries << 16;
			/* TRANSFER_CHARACTERISTICS */
			reg &= ~(0xFF << 8);
			reg |= p->transfer_characteristics << 8;
			/* MATRIX_COEFFICIENTS */
			reg &= ~(0xFF);
			reg |= p->matrix_coefficients;
		} else {
			reg &= ~(0x1 << 24);
		}
		MFC_WRITEL(reg, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		mfc_debug(2, "[HDR] H264 ENC Color aspect: range(%s), pri(%d), trans(%d), mat(%d)\n",
				p->color_range ? "Full" : "Limited", p->colour_primaries,
				p->transfer_characteristics, p->matrix_coefficients);
	} else {
		MFC_WRITEL(0, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
	}

	mfc_set_fmo_slice_map_h264(ctx, p_264);

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_mpeg4(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	/* set gop_size with I_FRM_CTRL mode */
	mfc_set_gop_size(ctx, 1);

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_mpeg4->level << 8);
	/** profile - 0 ~ 1 */
	reg &= ~(0x3F);
	reg |= p_mpeg4->profile;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* quarter_pixel */
	/* MFC_WRITEL(p_mpeg4->quarter_pixel, S5P_FIMV_ENC_MPEG4_QUART_PXL); */

	/* qp */
	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 16);
	reg |= ((p_mpeg4->rc_b_frame_qp & 0xFF) << 16);
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_min_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max B frame QP */
	reg &= ~(0xFF << 24);
	reg |= ((p_mpeg4->rc_max_qp_b & 0xFF) << 24);
	/** min B frame QP */
	reg &= ~(0xFF << 16);
	reg |= ((p_mpeg4->rc_min_qp_b & 0xFF) << 16);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_mpeg4->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	/* initialize for '0' only setting*/
	MFC_WRITEL(0x0, S5P_FIMV_E_MPEG4_OPTIONS); /* SEQ_start only */
	MFC_WRITEL(0x0, S5P_FIMV_E_MPEG4_HEC_PERIOD); /* SEQ_start only */

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_h263(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	/* For H.263 only 8 bit is used and maximum value can be 0xFF */
	p->rc_framerate_res = 100;
	mfc_set_enc_params(ctx);

	/* set gop_size with I_FRM_CTRL mode */
	mfc_set_gop_size(ctx, 1);

	/* profile & level: supports only baseline profile Level 70 */

	/* qp */
	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= (p_mpeg4->rc_min_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_mpeg4->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_mpeg4->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_vp8(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_vp8_enc_params *p_vp8 = &p->codec.vp8;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	if (p_vp8->num_hier_layer & 0x3) {
		/* set gop_size without i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 0);
	} else {
		/* set gop_size with i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 1);
	}

	/* profile*/
	reg = 0;
	reg |= (p_vp8->vp8_version) ;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	reg = MFC_READL(S5P_FIMV_E_VP8_OPTION);
	/* if num_refs_for_p is 2, the performance falls by half */
	reg &= ~(0x1);
	reg |= (p->num_refs_for_p - 1) & 0x1;
	/* vp8 partition is possible as below value: 1/2/4/8 */
	if (p_vp8->vp8_numberofpartitions & 0x1) {
		if (p_vp8->vp8_numberofpartitions > 1)
			mfc_err_ctx("partition should be even num (%d)\n",
					p_vp8->vp8_numberofpartitions);
		p_vp8->vp8_numberofpartitions = (p_vp8->vp8_numberofpartitions & ~0x1);
	}
	reg &= ~(0xF << 3);
	reg |= (p_vp8->vp8_numberofpartitions & 0xF) << 3;
	reg &= ~(0x1 << 10);
	reg |= (p_vp8->intra_4x4mode_disable & 0x1) << 10;
	/* Temporal SVC - hier qp enable */
	reg &= ~(0x1 << 11);
	reg |= (p_vp8->hier_qp_enable & 0x1) << 11;
	/* Disable IVF header */
	reg &= ~(0x1 << 12);
	reg |= ((p->ivf_header_disable & 0x1) << 12);
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_OPTION);

	reg = MFC_READL(S5P_FIMV_E_VP8_GOLDEN_FRAME_OPTION);
	reg &= ~(0x1);
	reg |= (p_vp8->vp8_goldenframesel & 0x1);
	reg &= ~(0xFFFF << 1);
	reg |= (p_vp8->vp8_gfrefreshperiod & 0xFFFF) << 1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_GOLDEN_FRAME_OPTION);

	/* Temporal SVC - layer number */
	reg = MFC_READL(S5P_FIMV_E_NUM_T_LAYER);
	reg &= ~(0x7);
	reg |= p_vp8->num_hier_layer & 0x3;
	reg &= ~(0x7 << 4);
	reg |= 0x3 << 4;
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
	mfc_debug(3, "Temporal SVC: hier_qp_enable %d, num_hier_layer %d, NUM_T_LAYER 0x%x\n",
			p_vp8->hier_qp_enable, p_vp8->num_hier_layer, reg);

	/* QP & Bitrate for each layer */
	for (i = 0; i < 3; i++) {
		MFC_WRITEL(p_vp8->hier_qp_layer[i],
				S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		MFC_WRITEL(p_vp8->hier_bit_layer[i],
				S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		mfc_debug(3, "Temporal SVC: layer[%d] QP: %#x, bitrate: %#x\n",
					i, p_vp8->hier_qp_layer[i],
					p_vp8->hier_bit_layer[i]);
	}

	reg = 0;
	reg |= (p_vp8->vp8_filtersharpness & 0x7);
	reg |= (p_vp8->vp8_filterlevel & 0x3f) << 8;
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_FILTER_OPTION);

	/* qp */
	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 8);
	reg |= ((p_vp8->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_vp8->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_vp8->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_vp8->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= (p_vp8->rc_min_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_vp8->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_vp8->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_vp9(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_vp9_enc_params *p_vp9 = &p->codec.vp9;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	if (p_vp9->num_hier_layer & 0x3) {
		/* set gop_size without i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 0);
	} else {
		/* set gop_size with i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 1);
	}

	/* profile*/
	reg = 0;
	reg |= (p_vp9->vp9_version) ;
	/* bit depth minus8 */
	if (ctx->is_10bit) {
		reg &= ~(0x3F << 17);
		reg |= (0x2 << 17);
		reg |= (0x2 << 20);
		/* fixed profile */
		reg |= 0x2;
	}
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	reg = MFC_READL(S5P_FIMV_E_VP9_OPTION);
	/* if num_refs_for_p is 2, the performance falls by half */
	reg &= ~(0x1);
	reg |= (p->num_refs_for_p - 1) & 0x1;
	reg &= ~(0x1 << 1);
	reg |= (p_vp9->intra_pu_split_disable & 0x1) << 1;
	reg &= ~(0x1 << 3);
	reg |= (p_vp9->max_partition_depth & 0x1) << 3;
	/* Temporal SVC - hier qp enable */
	reg &= ~(0x1 << 11);
	reg |= ((p_vp9->hier_qp_enable & 0x1) << 11);
	/* Disable IVF header */
	reg &= ~(0x1 << 12);
	reg |= ((p->ivf_header_disable & 0x1) << 12);
	MFC_WRITEL(reg, S5P_FIMV_E_VP9_OPTION);

	reg = MFC_READL(S5P_FIMV_E_VP9_GOLDEN_FRAME_OPTION);
	reg &= ~(0x1);
	reg |= (p_vp9->vp9_goldenframesel & 0x1);
	reg &= ~(0xFFFF << 1);
	reg |= (p_vp9->vp9_gfrefreshperiod & 0xFFFF) << 1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP9_GOLDEN_FRAME_OPTION);

	/* Temporal SVC - layer number */
	reg = MFC_READL(S5P_FIMV_E_NUM_T_LAYER);
	reg &= ~(0x7);
	reg |= p_vp9->num_hier_layer & 0x3;
	reg &= ~(0x7 << 4);
	reg |= 0x3 << 4;
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
	mfc_debug(3, "Temporal SVC: hier_qp_enable %d, num_hier_layer %d, NUM_T_LAYER 0x%x\n",
			p_vp9->hier_qp_enable, p_vp9->num_hier_layer, reg);

	/* QP & Bitrate for each layer */
	for (i = 0; i < 3; i++) {
		MFC_WRITEL(p_vp9->hier_qp_layer[i],
				S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		MFC_WRITEL(p_vp9->hier_bit_layer[i],
				S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		mfc_debug(3, "Temporal SVC: layer[%d] QP: %#x, bitrate: %#x\n",
					i, p_vp9->hier_qp_layer[i],
					p_vp9->hier_bit_layer[i]);
	}

	/* qp */
	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 8);
	reg |= ((p_vp9->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_vp9->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_vp9->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_vp9->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= (p_vp9->rc_min_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_vp9->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_vp9->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	if (FW_HAS_ENC_COLOR_ASPECT(dev) && p->check_color_range) {
		reg = MFC_READL(S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		/* VIDEO_SIGNAL_TYPE_FLAG */
		reg |= 0x1 << 31;
		/* COLOR_SPACE: VP9 uses colour_primaries interface for color space */
		reg &= ~(0x1F << 26);
		reg |= p->colour_primaries << 26;
		/* COLOR_RANGE */
		reg &= ~(0x1 << 25);
		reg |= p->color_range << 25;
		MFC_WRITEL(reg, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		mfc_debug(2, "[HDR] VP9 ENC Color aspect: range(%s), space(%d)\n",
				p->color_range ? "Full" : "Limited", p->colour_primaries);
	} else {
		MFC_WRITEL(0, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
	}

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_hevc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_hevc_enc_params *p_hevc = &p->codec.hevc;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	if (p_hevc->num_hier_layer & 0x7) {
		/* set gop_size without i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 0);
	} else {
		/* set gop_size with i_frm_ctrl mode */
		mfc_set_gop_size(ctx, 1);
	}

	/* UHD encoding case */
	if ((ctx->img_width == 3840) && (ctx->img_height == 2160)) {
		p_hevc->level = 51;
		p_hevc->tier_flag = 0;
	/* this tier_flag can be changed */
	}

	/* tier_flag & level & profile */
	reg = 0;
	/* profile */
	reg |= p_hevc->profile & 0x3;
	/* level */
	reg &= ~(0xFF << 8);
	reg |= (p_hevc->level << 8);
	/* tier_flag - 0 ~ 1 */
	reg |= (p_hevc->tier_flag << 16);
	/* bit depth minus8 */
	if (ctx->is_10bit) {
		reg &= ~(0x3F << 17);
		reg |= (0x2 << 17);
		reg |= (0x2 << 20);
		/* fixed profile */
		if (ctx->is_422format)
			reg |= 0x2;
		else
			reg |= 0x3;
	}
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* max partition depth */
	reg = MFC_READL(S5P_FIMV_E_HEVC_OPTIONS);
	reg &= ~(0x3);
	reg |= (p_hevc->max_partition_depth & 0x1);
	/* if num_refs_for_p is 2, the performance falls by half */
	reg &= ~(0x1 << 2);
	reg |= (p->num_refs_for_p - 1) << 2;
	reg &= ~(0x3 << 3);
	reg |= (p_hevc->refreshtype & 0x3) << 3;
	reg &= ~(0x1 << 5);
	reg |= (p_hevc->const_intra_period_enable & 0x1) << 5;
	reg &= ~(0x1 << 6);
	reg |= (p_hevc->lossless_cu_enable & 0x1) << 6;
	reg &= ~(0x1 << 7);
	reg |= (p_hevc->wavefront_enable & 0x1) << 7;
	reg &= ~(0x1 << 8);
	reg |= (p_hevc->loopfilter_disable & 0x1) << 8;
	reg &= ~(0x1 << 9);
	reg |= (p_hevc->loopfilter_across & 0x1) << 9;
	reg &= ~(0x1 << 10);
	reg |= (p_hevc->enable_ltr & 0x1) << 10;
	reg &= ~(0x1 << 11);
	reg |= (p_hevc->hier_qp_enable & 0x1) << 11;
	reg &= ~(0x1 << 13);
	reg |= (p_hevc->general_pb_enable & 0x1) << 13;
	reg &= ~(0x1 << 14);
	reg |= (p_hevc->temporal_id_enable & 0x1) << 14;
	reg &= ~(0x1 << 15);
	reg |= (p_hevc->strong_intra_smooth & 0x1) << 15;
	reg &= ~(0x1 << 16);
	reg |= (p_hevc->intra_pu_split_disable & 0x1) << 16;
	reg &= ~(0x1 << 17);
	reg |= (p_hevc->tmv_prediction_disable & 0x1) << 17;
	reg &= ~(0x7 << 18);
	reg |= (p_hevc->max_num_merge_mv & 0x7) << 18;
	reg &= ~(0x1 << 23);
	reg |= (p_hevc->encoding_nostartcode_enable & 0x1) << 23;
	reg &= ~(0x1 << 26);
	reg |= (p_hevc->prepend_sps_pps_to_idr & 0x1) << 26;
	/* enable sps pps control in OTF scenario */
	if (ctx->otf_handle) {
		reg |= (0x1 << 26);
		mfc_debug(2, "OTF: SPS_PPS_CONTROL enabled\n");
	}
	/* Weighted Prediction enable */
	reg &= ~(0x1 << 28);
	reg |= ((p->weighted_enable & 0x1) << 28);
	/* 30bit is 32x32 transform. If it is enabled, the performance falls by half */
	reg &= ~(0x1 << 30);
	MFC_WRITEL(reg, S5P_FIMV_E_HEVC_OPTIONS);
	/* refresh period */
	reg = MFC_READL(S5P_FIMV_E_HEVC_REFRESH_PERIOD);
	reg &= ~(0xFFFF);
	reg |= (p_hevc->refreshperiod & 0xFFFF);
	MFC_WRITEL(reg, S5P_FIMV_E_HEVC_REFRESH_PERIOD);
	/* loop filter setting */
	if (!p_hevc->loopfilter_disable) {
		MFC_WRITEL(p_hevc->lf_beta_offset_div2, S5P_FIMV_E_HEVC_LF_BETA_OFFSET_DIV2);
		MFC_WRITEL(p_hevc->lf_tc_offset_div2, S5P_FIMV_E_HEVC_LF_TC_OFFSET_DIV2);
	}
	/* long term reference */
	if (p_hevc->enable_ltr) {
		reg = 0;
		reg |= (p_hevc->store_ref & 0x3);
		reg &= ~(0x3 << 2);
		reg |= (p_hevc->user_ref & 0x3) << 2;
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_NAL_CONTROL);
	}

	/* Temporal SVC - qp type, layer number */
	reg = MFC_READL(S5P_FIMV_E_NUM_T_LAYER);
	reg &= ~(0x1 << 3);
	reg |= (p_hevc->hier_qp_type & 0x1) << 3;
	reg &= ~(0x7);
	reg |= p_hevc->num_hier_layer & 0x7;
	reg &= ~(0x7 << 4);
	if (p_hevc->hier_ref_type) {
		reg |= 0x1 << 7;
		reg |= (p->num_hier_max_layer & 0x7) << 4;
	} else {
		reg |= 0x7 << 4;
	}
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
	mfc_debug(2, "Temporal SVC: hier_qp_enable %d, enable_ltr %d, "
		"num_hier_layer %d, max_layer %d, hier_ref_type %d, NUM_T_LAYER 0x%x\n",
		p_hevc->hier_qp_enable, p_hevc->enable_ltr, p_hevc->num_hier_layer,
		p->num_hier_max_layer, p_hevc->hier_ref_type, reg);

	/* QP & Bitrate for each layer */
	for (i = 0; i < 7; i++) {
		MFC_WRITEL(p_hevc->hier_qp_layer[i],
			S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		MFC_WRITEL(p_hevc->hier_bit_layer[i],
			S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		mfc_debug(3, "Temporal SVC: layer[%d] QP: %#x, bitrate: %#x\n",
					i, p_hevc->hier_qp_layer[i],
					p_hevc->hier_bit_layer[i]);
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= (p_hevc->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP for I frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND);
	/** max I frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_hevc->rc_max_qp & 0xFF) << 8);
	/** min I frame QP */
	reg &= ~(0xFF);
	reg |= (p_hevc->rc_min_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* max & min value of QP for P/B frame */
	reg = MFC_READL(S5P_FIMV_E_RC_QP_BOUND_PB);
	/** max B frame QP */
	reg &= ~(0xFF << 24);
	reg |= ((p_hevc->rc_max_qp_b & 0xFF) << 24);
	/** min B frame QP */
	reg &= ~(0xFF << 16);
	reg |= ((p_hevc->rc_min_qp_b & 0xFF) << 16);
	/** max P frame QP */
	reg &= ~(0xFF << 8);
	reg |= ((p_hevc->rc_max_qp_p & 0xFF) << 8);
	/** min P frame QP */
	reg &= ~(0xFF);
	reg |= p_hevc->rc_min_qp_p & 0xFF;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND_PB);

	reg = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
	reg &= ~(0xFF << 24);
	reg |= ((p->config_qp & 0xFF) << 24);
	reg &= ~(0xFF << 16);
	reg |= ((p_hevc->rc_b_frame_qp & 0xFF) << 16);
	reg &= ~(0xFF << 8);
	reg |= ((p_hevc->rc_p_frame_qp & 0xFF) << 8);
	reg &= ~(0xFF);
	reg |= (p_hevc->rc_frame_qp & 0xFF);
	MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);

	/* ROI enable: it must set on SEQ_START only for HEVC encoder */
	reg = MFC_READL(S5P_FIMV_E_RC_ROI_CTRL);
	reg &= ~(0x1);
	reg |= (p->roi_enable);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_ROI_CTRL);
	mfc_debug(3, "ROI: HEVC ROI enable\n");

	if (FW_HAS_ENC_COLOR_ASPECT(dev) && p->check_color_range) {
		reg = MFC_READL(S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		/* VIDEO_SIGNAL_TYPE_FLAG */
		reg |= 0x1 << 31;
		/* COLOR_RANGE */
		reg &= ~(0x1 << 25);
		reg |= p->color_range << 25;
		if ((p->colour_primaries != 0) && (p->transfer_characteristics != 0) &&
				(p->matrix_coefficients != 3)) {
			/* COLOUR_DESCRIPTION_PRESENT_FLAG */
			reg |= 0x1 << 24;
			/* COLOUR_PRIMARIES */
			reg &= ~(0xFF << 16);
			reg |= p->colour_primaries << 16;
			/* TRANSFER_CHARACTERISTICS */
			reg &= ~(0xFF << 8);
			reg |= p->transfer_characteristics << 8;
			/* MATRIX_COEFFICIENTS */
			reg &= ~(0xFF);
			reg |= p->matrix_coefficients;
		} else {
			reg &= ~(0x1 << 24);
		}
		MFC_WRITEL(reg, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
		mfc_debug(2, "[HDR] HEVC ENC Color aspect: range(%s), pri(%d), trans(%d), mat(%d)\n",
				p->color_range ? "Full" : "Limited", p->colour_primaries,
				p->transfer_characteristics, p->matrix_coefficients);
	} else {
		MFC_WRITEL(0, S5P_FIMV_E_VIDEO_SIGNAL_TYPE);
	}

	if (FW_HAS_ENC_STATIC_INFO(dev) && p->static_info_enable && ctx->is_10bit) {
		reg = MFC_READL(S5P_FIMV_E_HEVC_OPTIONS_2);
		/* HDR_STATIC_INFO_ENABLE */
		reg |= p->static_info_enable;
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_OPTIONS_2);
		/* MAX_PIC_AVERAGE_LIGHT & MAX_CONTENT_LIGHT */
		reg = p->max_pic_average_light;
		reg |= (p->max_content_light << 16);
		MFC_WRITEL(reg, S5P_FIMV_E_CONTENT_LIGHT_LEVEL_INFO_SEI);
		/* MAX_DISPLAY_LUMINANCE */
		MFC_WRITEL(p->max_display_luminance, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_0);
		/* MIN DISPLAY LUMINANCE */
		MFC_WRITEL(p->min_display_luminance, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_1);
		/* WHITE_POINT */
		MFC_WRITEL(p->white_point, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_2);
		/* DISPLAY PRIMARIES_0 */
		MFC_WRITEL(p->display_primaries_0, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_3);
		/* DISPLAY PRIMARIES_1 */
		MFC_WRITEL(p->display_primaries_1, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_4);
		/* DISPLAY PRIMARIES_2 */
		MFC_WRITEL(p->display_primaries_2, S5P_FIMV_E_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_5);

		mfc_debug(2, "[HDR] HEVC ENC static info: enable(%d), max_pic(0x%x), max_content(0x%x)\n",
				p->static_info_enable, p->max_pic_average_light, p->max_content_light);
		mfc_debug(2, "[HDR] max_disp(0x%x), min_disp(0x%x), white_point(0x%x)\n",
				p->max_display_luminance, p->min_display_luminance, p->white_point);
		mfc_debug(2, "[HDR] disp_pri_0(0x%x), disp_pri_1(0x%x), disp_pri_2(0x%x)\n",
				p->display_primaries_0, p->display_primaries_1, p->display_primaries_2);
	} else {
		reg = MFC_READL(S5P_FIMV_E_HEVC_OPTIONS_2);
		/* HDR_STATIC_INFO_ENABLE */
		reg &=  ~(0x1);
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_OPTIONS_2);
	}

	mfc_debug_leave();
}

void s5p_mfc_set_enc_params_bpg(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_bpg_enc_params *p_bpg = &p->codec.bpg;
	unsigned int reg = 0;

	p->rc_framerate_res = FRAME_RATE_RESOLUTION;
	mfc_set_enc_params(ctx);

	/* extension tag */
	reg = p_bpg->thumb_size + p_bpg->exif_size;
	MFC_WRITEL(reg, S5P_FIMV_E_BPG_EXTENSION_DATA_SIZE);
	mfc_debug(3, "main image extension size %d (thumbnail: %d, exif: %d)\n",
			reg, p_bpg->thumb_size, p_bpg->exif_size);

	/* profile & level */
	reg = 0;
	/** profile */
	reg &= ~(0xF);
	/* bit depth minus8 */
	if (ctx->is_10bit) {
		reg &= ~(0x3F << 17);
		reg |= (0x2 << 17);
		reg |= (0x2 << 20);
		/* fixed profile */
		if (ctx->is_422format)
			reg |= 0x1;
	}
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);
}
