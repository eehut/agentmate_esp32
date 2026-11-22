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
# Description: AgentMate continuous LED arcs and deterministic state animation
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-03 1.0      Codex         Created selected-session LED animations.
# 2026-10-03 1.1      Codex         Redesigned patterns for discrete DIY light points.
#
#####################################################################################
*/

#ifndef AGENTMATE_LED_H
#define AGENTMATE_LED_H

#include "ui_state.h"

enum am_led_mode {
    AM_LED_OFF,
    AM_LED_DISCONNECTED,
    AM_LED_CONNECTING,
    AM_LED_PERMISSION,
    AM_LED_WORKING,
    AM_LED_THINKING,
    AM_LED_STALE,
    AM_LED_SUCCESS,
    AM_LED_INPUT,
    AM_LED_FAILED,
    AM_LED_CANCELED,
};

struct am_led_effect {
    enum am_led_mode mode;
    rgb_led_color_t color;
    uint32_t generation;
    char key[9];
};

struct am_led_state {
    bool initialized;
    struct am_led_effect effect;
    int64_t started_us;
};

struct am_led_effect am_led_select(const am_model_t *model, const struct am_ui_selection *selection);
void am_led_frame(struct am_led_state *state, const struct am_led_effect *effect, int64_t now, rgb_led_color_t colors[16]);

#endif
