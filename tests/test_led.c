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
# Description: Host regression checks for selected-session LED animation
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

#include <assert.h>
#include <string.h>

struct random_sequence {
    unsigned count;
};

static uint32_t next_random(void *context)
{
    struct random_sequence *sequence = context;
    uint32_t values[] = {1, 0, 16, 8};
    return values[sequence->count++ % 4];
}

static unsigned lit_mask(const rgb_led_color_t colors[16])
{
    unsigned mask = 0;
    for (int i = 0; i < 16; ++i) {
        if (colors[i].r || colors[i].g || colors[i].b) {
            mask |= 1U << i;
        }
    }
    return mask;
}

static int lit_count(const rgb_led_color_t colors[16])
{
    unsigned mask = lit_mask(colors);
    int count = 0;
    while (mask) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

void test_led_patterns(void)
{
    am_model_t model = {.link = AM_LINK_WAIT_HOST};
    struct am_ui_selection selected = {0};
    struct am_led_effect effect = am_led_select(&model, &selected);
    struct am_led_state state = {0};
    struct random_sequence random = {0};
    rgb_led_color_t colors[16];
    am_led_frame(&state, &effect, 0, next_random, &random, colors);
    assert(effect.mode == AM_LED_DISCONNECTED && lit_mask(colors) == 0xfcfc);
    am_led_frame(&state, &effect, 399999, next_random, &random, colors); assert(lit_mask(colors) == 0xfcfc);
    am_led_frame(&state, &effect, 400000, next_random, &random, colors); assert(lit_mask(colors) == 0xf9f9);
    assert(lit_count(colors) == 12 && colors[0].r == 255 && random.count == 0);
    model.link = AM_LINK_SYNCING;
    effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 1000000, next_random, &random, colors);
    assert(effect.mode == AM_LED_CONNECTING && colors[2].g == 159);
    am_led_frame(&state, &effect, 1100000, next_random, &random, colors); assert(lit_mask(colors) == 0xf9f9);
    model.link = AM_LINK_ONLINE;
    effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 1200000, next_random, &random, colors); assert(lit_count(colors) == 0);
    selected.count = 2; selected.slots[0] = 0; selected.slots[1] = 1;
    strcpy(model.sessions[0].key, "s1"); strcpy(model.sessions[1].key, "s2");
    strcpy(model.sessions[0].state, "permission_required"); strcpy(model.sessions[1].state, "waiting_for_input");
    effect = am_led_select(&model, &selected);
    assert(effect.mode == AM_LED_DOUBLE_FLASH && effect.color.b == 122);
    am_led_frame(&state, &effect, 2000000, next_random, &random, colors); assert(lit_count(colors) == 16);
    am_led_frame(&state, &effect, 2200000, next_random, &random, colors); assert(lit_count(colors) == 0);
    am_led_frame(&state, &effect, 2400000, next_random, &random, colors); assert(lit_count(colors) == 16);
    am_led_frame(&state, &effect, 2600000, next_random, &random, colors); assert(lit_count(colors) == 0);
    am_led_frame(&state, &effect, 4000000, next_random, &random, colors); assert(lit_count(colors) == 16);
    selected.current = 1; effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 4100000, next_random, &random, colors);
    assert(colors[0].g == 176 && colors[0].b == 46 && state.started_us == 4100000);
    strcpy(model.sessions[1].state, "working"); effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 5000000, next_random, &random, colors);
    assert(lit_count(colors) == 5 && state.direction == 1 && state.steps_left == 8 && random.count == 2);
    unsigned initial_mask = lit_mask(colors);
    model.sessions[1].rev++;
    model.sessions[1].task_seconds++;
    strcpy(model.sessions[1].project_name, "updated_directory");
    effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 5100000, next_random, &random, colors);
    assert(state.started_us == 5000000 && random.count == 2 && lit_mask(colors) == initial_mask);
    am_led_frame(&state, &effect, 5149999, next_random, &random, colors); assert(lit_mask(colors) == initial_mask && random.count == 2);
    am_led_frame(&state, &effect, 5150000, next_random, &random, colors); assert(state.head == 1 && lit_count(colors) == 5);
    am_led_frame(&state, &effect, 6200000, next_random, &random, colors);
    assert(state.head == 8 && state.direction == -1 && state.steps_left == 24 && random.count == 3);
    for (int i = 1; i <= 24; ++i) {
        am_led_frame(&state, &effect, 6200000 + i * 150000LL, next_random, &random, colors);
        assert(lit_count(colors) == 5);
    }
    assert(state.head == 0 && state.direction == 1 && state.steps_left == 16 && random.count == 4);
    effect.generation++; am_led_frame(&state, &effect, 10000000, next_random, &random, colors);
    assert(state.started_us == 10000000 && random.count == 6);
    strcpy(model.sessions[1].state, "thinking"); effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 11000000, next_random, &random, colors); assert(lit_mask(colors) == 0x0303);
    am_led_frame(&state, &effect, 11300000, next_random, &random, colors); assert(lit_mask(colors) == 0x0606);
    assert(lit_count(colors) == 4 && effect.color.b == 255);
    strcpy(model.sessions[1].state, "stale"); effect = am_led_select(&model, &selected);
    am_led_frame(&state, &effect, 12000000, next_random, &random, colors); assert(lit_mask(colors) == 0x0003);
    am_led_frame(&state, &effect, 12500000, next_random, &random, colors); assert(lit_count(colors) == 0);
    am_led_frame(&state, &effect, 13000000, next_random, &random, colors); assert(lit_mask(colors) == 0x0300);
    am_led_frame(&state, &effect, 13500000, next_random, &random, colors); assert(lit_count(colors) == 0);
    const char *results[] = {"task_success", "task_failed", "task_canceled"};
    const rgb_led_color_t expected[] = {{54, 226, 154}, {255, 59, 48}, {74, 168, 255}};
    for (int i = 0; i < 3; ++i) {
        strcpy(model.sessions[1].state, results[i]); effect = am_led_select(&model, &selected);
        am_led_frame(&state, &effect, 14000000 + i * 2000000, next_random, &random, colors);
        assert(lit_count(colors) == 16 && memcmp(&colors[0], &expected[i], sizeof(colors[0])) == 0);
        am_led_frame(&state, &effect, 14250000 + i * 2000000, next_random, &random, colors); assert(lit_count(colors) == 0);
        am_led_frame(&state, &effect, 15600000 + i * 2000000, next_random, &random, colors); assert(lit_count(colors) == 16);
    }
}
