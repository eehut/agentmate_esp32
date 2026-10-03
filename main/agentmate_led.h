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
# Description: AgentMate binary LED patterns and injectable chase animation
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-03 1.0      Codex         Created selected-session LED animations.
#
#####################################################################################
*/

#ifndef AGENTMATE_LED_H
#define AGENTMATE_LED_H

#include "agentmate_ui_state.h"

enum am_led_mode {
    AM_LED_OFF,
    AM_LED_DISCONNECTED,
    AM_LED_CONNECTING,
    AM_LED_DOUBLE_FLASH,
    AM_LED_CHASE,
    AM_LED_THINKING,
    AM_LED_STALE,
    AM_LED_RESULT,
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
    int64_t next_step_us;
    int head;
    int direction;
    unsigned steps_left;
};

typedef uint32_t (*am_led_random_fn)(void *context);

struct am_led_effect am_led_select(const am_model_t *model, const struct am_ui_selection *selection);
void am_led_frame(struct am_led_state *state, const struct am_led_effect *effect, int64_t now,
                  am_led_random_fn random, void *context, rgb_led_color_t colors[16]);

#endif
