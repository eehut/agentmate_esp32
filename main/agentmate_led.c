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
        effect.mode = AM_LED_DOUBLE_FLASH;
        effect.color = strcmp(state, "permission_required") == 0 ? (rgb_led_color_t){255, 95, 122} : (rgb_led_color_t){255, 176, 46};
    } else if (strcmp(state, "working") == 0) {
        effect.mode = AM_LED_CHASE;
        effect.color = (rgb_led_color_t){54, 226, 154};
    } else if (strcmp(state, "thinking") == 0) {
        effect.mode = AM_LED_THINKING;
        effect.color = (rgb_led_color_t){139, 153, 255};
    } else if (strcmp(state, "stale") == 0) {
        effect.mode = AM_LED_STALE;
        effect.color = (rgb_led_color_t){163, 175, 211};
    } else if (am_state_result(state)) {
        effect.mode = AM_LED_RESULT;
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

void am_led_frame(struct am_led_state *state, const struct am_led_effect *effect, int64_t now,
                  am_led_random_fn random, void *context, rgb_led_color_t colors[16])
{
    if (!state->initialized || !same_effect(&state->effect, effect)) {
        memset(state, 0, sizeof(*state));
        state->initialized = true;
        state->effect = *effect;
        state->started_us = now;
        if (effect->mode == AM_LED_CHASE) {
            state->direction = random(context) % 2 ? 1 : -1;
            state->steps_left = 8 + random(context) % 17;
            state->next_step_us = now + 150000;
        }
    }
    memset(colors, 0, 16 * sizeof(*colors));
    int64_t elapsed_ms = (now - state->started_us) / 1000;
    if (effect->mode == AM_LED_CHASE) {
        while (now >= state->next_step_us) {
            state->head = (state->head + state->direction + 16) % 16;
            state->next_step_us += 150000;
            if (--state->steps_left == 0) {
                state->direction = -state->direction;
                state->steps_left = 8 + random(context) % 17;
            }
        }
        static const int s_offsets[] = {0, -1, -2, -5, -6};
        for (int i = 0; i < 5; ++i) {
            colors[(state->head + state->direction * s_offsets[i] + 32) % 16] = effect->color;
        }
        return;
    }
    for (int i = 0; i < 16; ++i) {
        bool lit = false;
        if (effect->mode == AM_LED_DISCONNECTED || effect->mode == AM_LED_CONNECTING) {
            int step_ms = effect->mode == AM_LED_DISCONNECTED ? 400 : 100;
            int position = (i - (int)(elapsed_ms / step_ms % 16) + 16) % 16;
            lit = (position >= 2 && position < 8) || position >= 10;
        } else if (effect->mode == AM_LED_DOUBLE_FLASH) {
            int phase = elapsed_ms % 2000;
            lit = phase < 200 || (phase >= 400 && phase < 600);
        } else if (effect->mode == AM_LED_THINKING) {
            int position = (i - (int)(elapsed_ms / 300 % 16) + 16) % 16;
            lit = position < 2 || (position >= 8 && position < 10);
        } else if (effect->mode == AM_LED_STALE) {
            int phase = elapsed_ms % 2000;
            lit = (phase < 500 && i < 2) || (phase >= 1000 && phase < 1500 && i >= 8 && i < 10);
        } else if (effect->mode == AM_LED_RESULT) {
            lit = elapsed_ms % 1600 < 250;
        }
        if (lit) {
            colors[i] = effect->color;
        }
    }
}
