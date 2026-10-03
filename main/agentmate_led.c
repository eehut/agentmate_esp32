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

#include "agentmate_led.h"

#include <string.h>

struct am_led_effect am_led_select(const am_model_t *model, const struct am_ui_selection *selection)
{
    struct am_led_effect effect = {.generation = model->remote_generation};
    if (model->link != AM_LINK_ONLINE) {
        bool connecting = model->link == AM_LINK_HANDSHAKE || model->link == AM_LINK_SYNCING;
        effect.mode = connecting ? AM_LED_CONNECTING : AM_LED_DISCONNECTED;
        effect.color = connecting ? (rgb_led_color_t){255, 159, 28} : (rgb_led_color_t){255, 59, 48};
        return effect;
    }
    if (!selection->count) {
        return effect;
    }
    const am_session_t *session = &model->sessions[selection->slots[selection->current]];
    memcpy(effect.key, session->key, sizeof(effect.key));
    const char *state = session->state;
    if (strcmp(state, "permission_required") == 0 || strcmp(state, "waiting_for_input") == 0) {
        effect.mode = strcmp(state, "permission_required") == 0 ? AM_LED_PERMISSION : AM_LED_INPUT;
        effect.color = strcmp(state, "permission_required") == 0 ? (rgb_led_color_t){255, 95, 122} : (rgb_led_color_t){255, 176, 46};
    } else if (strcmp(state, "working") == 0) {
        effect.mode = AM_LED_WORKING;
        effect.color = (rgb_led_color_t){54, 226, 154};
    } else if (strcmp(state, "thinking") == 0) {
        effect.mode = AM_LED_THINKING;
        effect.color = (rgb_led_color_t){139, 153, 255};
    } else if (strcmp(state, "stale") == 0) {
        effect.mode = AM_LED_STALE;
        effect.color = (rgb_led_color_t){163, 175, 211};
    } else if (am_state_result(state)) {
        effect.mode = strcmp(state, "task_success") == 0 ? AM_LED_SUCCESS :
                      strcmp(state, "task_failed") == 0 ? AM_LED_FAILED : AM_LED_CANCELED;
        effect.color = strcmp(state, "task_success") == 0 ? (rgb_led_color_t){54, 226, 154} :
                       strcmp(state, "task_failed") == 0 ? (rgb_led_color_t){255, 59, 48} : (rgb_led_color_t){74, 168, 255};
    }
    return effect;
}

static bool same_effect(const struct am_led_effect *left, const struct am_led_effect *right)
{
    return left->mode == right->mode && left->generation == right->generation && strcmp(left->key, right->key) == 0 &&
           left->color.r == right->color.r && left->color.g == right->color.g && left->color.b == right->color.b;
}

static rgb_led_color_t scale_color(rgb_led_color_t color, unsigned percent)
{
    return (rgb_led_color_t){(color.r * percent + 50) / 100, (color.g * percent + 50) / 100, (color.b * percent + 50) / 100};
}

static void fixed_arcs(rgb_led_color_t colors[16], rgb_led_color_t color, int length, unsigned first, unsigned second)
{
    for (int i = 0; i < length; ++i) {
        colors[i] = scale_color(color, first);
        colors[i + 8] = scale_color(color, second);
    }
}

static void moving_arc(rgb_led_color_t colors[16], rgb_led_color_t color, int head, const uint8_t *levels, int length)
{
    for (int i = 0; i < length; ++i) {
        colors[(head - i + 16) % 16] = scale_color(color, levels[i]);
    }
}

void am_led_frame(struct am_led_state *state, const struct am_led_effect *effect, int64_t now, rgb_led_color_t colors[16])
{
    if (!state->initialized || !same_effect(&state->effect, effect)) {
        memset(state, 0, sizeof(*state));
        state->initialized = true;
        state->effect = *effect;
        state->started_us = now;
    }
    memset(colors, 0, 16 * sizeof(*colors));
    int64_t elapsed_ms = now >= state->started_us ? (now - state->started_us) / 1000 : 0;
    switch (effect->mode) {
    case AM_LED_CONNECTING: {
        static const uint8_t s_levels[] = {100, 75, 40, 15};
        int head = (int)(elapsed_ms / 100 % 16);
        moving_arc(colors, effect->color, head, s_levels, 4);
        moving_arc(colors, effect->color, (head + 8) % 16, s_levels, 4);
        break;
    }
    case AM_LED_WORKING: {
        static const uint8_t s_levels[] = {100, 100, 75, 50, 30, 15};
        moving_arc(colors, effect->color, (int)(elapsed_ms / 150 % 16), s_levels, 6);
        break;
    }
    case AM_LED_THINKING: {
        static const uint8_t s_levels[] = {80, 40, 15};
        int head = (int)(elapsed_ms / 250 % 16);
        moving_arc(colors, effect->color, head, s_levels, 3);
        moving_arc(colors, effect->color, (head + 8) % 16, s_levels, 3);
        break;
    }
    case AM_LED_DISCONNECTED:
        if (elapsed_ms % 4000 < 500) {
            fixed_arcs(colors, effect->color, 3, 60, 60);
        }
        break;
    case AM_LED_INPUT:
        if (elapsed_ms % 2000 < 600) {
            fixed_arcs(colors, effect->color, 4, 70, 70);
        }
        break;
    case AM_LED_CANCELED:
        if (elapsed_ms % 3000 < 300) {
            fixed_arcs(colors, effect->color, 4, 50, 50);
        }
        break;
    case AM_LED_STALE: {
        bool alternate = elapsed_ms % 3000 >= 1500;
        fixed_arcs(colors, effect->color, 3, alternate ? 15 : 60, alternate ? 60 : 15);
        break;
    }
    case AM_LED_PERMISSION:
    case AM_LED_FAILED:
    case AM_LED_SUCCESS: {
        int count = 0;
        if (effect->mode == AM_LED_PERMISSION) {
            int phase = (int)(elapsed_ms % 2000);
            count = phase < 150 || (phase >= 300 && phase < 450) ? 16 : 0;
        } else if (effect->mode == AM_LED_FAILED) {
            count = elapsed_ms % 2400 < 200 ? 16 : 0;
        } else {
            int phase = (int)(elapsed_ms % 2400);
            count = phase < 450 ? (phase / 150 + 1) * 4 : phase < 1200 ? 16 : 0;
        }
        for (int i = 0; i < count; ++i) {
            colors[i] = scale_color(effect->color, 55);
        }
        break;
    }
    case AM_LED_OFF:
        break;
    }
}
