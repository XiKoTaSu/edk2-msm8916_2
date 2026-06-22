#ifndef _ADV7533_PANEL_H_
#define _ADV7533_PANEL_H_

#include "msm_panel.h"
#include "mipi_dsi.h"

static struct mdss_dsi_phy_ctrl adv7533_phy_db = {
    .regulator_mode = DSI_PHY_REGULATOR_LDO_MODE,
    .is_pll_20nm = 0,
    .timing = {
        0x1b, 0x14, 0x06, 0x07, 0x02, 0x02, 0x04, 0xa0,
        0x1b, 0x14, 0x06, 0x00
    },
    .regulator = {
        0x03, 0x0a, 0x04, 0x00, 0x20
    },
    .strength = {
        0xff, 0x00
    },
    .bistCtrl = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    },
    .laneCfg = {
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }
};

static struct msm_fb_panel_data adv7533_panel = {
    .panel_info = {
        .xres = 1920,
        .yres = 1080,
        .type = MIPI_VIDEO_PANEL,
        .wait_cycle = 0,
        .bpp = 24,
        .clk_rate = 800000000,
        .mipi = {
            .mode = DSI_VIDEO_MODE,
            .pulse_mode_hsa_he = 1,
            .hfp_power_stop = 0,
            .hbp_power_stop = 0,
            .hsa_power_stop = 0,
            .eof_bllp_power_stop = 0,
            .bllp_power_stop = 0,
            .traffic_mode = DSI_NON_BURST_SYNCH_PULSE,
            .dst_format = DSI_VIDEO_DST_FORMAT_RGB888,
            .vc = 0,
            .rgb_swap = DSI_RGB_SWAP_RGB,
            .data_lane0 = 1,
            .data_lane1 = 1,
            .data_lane2 = 1,
            .data_lane3 = 1,
            .tx_eot_append = 1,
            .stream = 0,
            .mdp_trigger = 0,
            .dma_trigger = 0,
            .force_clk_lane_hs = 1,
            .mdss_dsi_phy_db = &adv7533_phy_db,
        },
        .lcdc = {
            .h_back_porch = 148,
            .h_front_porch = 88,
            .h_pulse_width = 44,
            .v_back_porch = 36,
            .v_front_porch = 4,
            .v_pulse_width = 5,
            .border_clr = 0,
            .underflow_clr = 0xff,
            .hsync_skew = 0,
        },
    },
    .fb = {
        .width = 1920,
        .height = 1080,
        .stride = 1920,
        .bpp = 24,
        .format = FB_FORMAT_RGB888,
        .base = (void *)0x8E000000,
    },
    .mdp_rev = MDP_REV_50,
};

#endif
