/*
#####################################################################################
#
# Copyright (c) 2023-2026 Liu Chuansen <liuchuansen@outlook.com>. All Rights Reserved.
#
# This software is the personal work of Liu Chuansen. You shall not copy, use,
# distribute, or create derivative works of this software without the author's
# prior written permission.
#
# UNAUTHORIZED COPYING, USE, OR DISTRIBUTION OF THIS SOFTWARE, OR ANY PORTION OR
# DERIVATIVE THEREOF, IS STRICTLY PROHIBITED. IF YOU HAVE RECEIVED THIS SOFTWARE IN
# ERROR, PLEASE NOTIFY THE AUTHOR IMMEDIATELY AND DELETE IT FROM YOUR SYSTEM.
#
#####################################################################################
#
# Description: AgentMate bounded scene renderer
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-12 1.5      Codex         Expose smooth curved headers and a common black background.
# 2026-10-11 1.4      Codex         Expose Session split panels and background selection.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-06 1.3      Codex         Add remaining-quota progress scene operations.
# 2026-10-03 1.2      Codex         Add bounded animated status border elements.
# 2026-10-03 1.1      Codex         Expose ink bounds for complete directory font selection.
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#ifndef AGENTMATE_UI_RENDER_H
#define AGENTMATE_UI_RENDER_H

#include "agentmate_app.h"
#include "agentmate_ui_assets.h"
#include "agentmate_status.h"

#define AM_UI_BG TFT_COLOR_BLACK
#define AM_UI_OPS 48

enum am_op_kind { AM_OP_TEXT, AM_OP_RECT, AM_OP_DISC, AM_OP_MASK, AM_OP_BITMAP, AM_OP_STATUS_FRAME, AM_OP_PROGRESS, AM_OP_PANEL, AM_OP_CAP };
struct am_ui_op {
    enum am_op_kind kind;
    int16_t x, y, width, height;
    uint16_t color;
    uint16_t track_color;
    uint16_t remaining_bp;
    uint8_t radius;
    uint8_t split_y;
    uint8_t border_width;
    uint8_t animation_frame;
    enum am_status_kind status_kind;
    const struct am_font *font;
    const struct am_image *image;
    const uint16_t *bitmap;
    char text[96];
};
struct am_ui_scene {
    struct am_ui_op ops[AM_UI_OPS];
    int count;
    uint8_t body_alpha;
    bool session_background;
};

extern uint16_t g_am_ui_pixels[240 * 32];

int am_text_width(const struct am_font *font, const char *text);
int am_text_ink_width(const struct am_font *font, const char *text);
void am_scene_text(struct am_ui_scene *scene, int x, int y, int width, const struct am_font *font, uint16_t color,
                   const char *text, int alignment);
void am_scene_rect(struct am_ui_scene *scene, int x, int y, int width, int height, uint16_t color, int radius);
void am_scene_panel(struct am_ui_scene *scene, int x, int y, int width, int height, int split_y, uint16_t upper, uint16_t lower);
void am_scene_cap(struct am_ui_scene *scene, int height, int radius_x, int radius_y, uint16_t color, int border_width);
void am_scene_disc(struct am_ui_scene *scene, int x, int y, int radius, uint16_t color);
void am_scene_progress(struct am_ui_scene *scene, int x, int y, int width, int height, uint16_t color, uint16_t track_color,
                   unsigned remaining_bp);
void am_scene_mask(struct am_ui_scene *scene, int x, int y, const struct am_image *image, uint16_t color);
void am_scene_bitmap(struct am_ui_scene *scene, int x, int y, int width, int height, const uint16_t *bitmap);
void am_scene_status(struct am_ui_scene *scene, const struct am_status_visual *visual);
/* A NULL previous scene requests an unconditional repaint without retaining another scene. */
esp_err_t am_scene_render(am_app_t *app, struct am_ui_scene *previous, const struct am_ui_scene *next, bool clear);

#endif
