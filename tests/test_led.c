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
# 2026-10-03 1.1      Codex         Verify discrete light arcs, timing, intensity and lifecycle.
#
#####################################################################################
*/

#include "led.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *s_states[] = {"permission_required", "working", "thinking", "stale", "task_success",
                                 "waiting_for_input", "task_failed", "task_canceled"};

static struct am_led_effect selected_effect(enum am_led_mode mode)
{
    am_model_t model = {.link = AM_LINK_ONLINE};
    struct am_ui_selection selection = {.count = 1};
    strcpy(model.sessions[0].key, "s1");
    if (mode == AM_LED_OFF) {
        selection.count = 0;
    } else if (mode == AM_LED_DISCONNECTED) {
        model.link = AM_LINK_WAIT_HOST;
    } else if (mode == AM_LED_CONNECTING) {
        model.link = AM_LINK_SYNCING;
    } else {
        strcpy(model.sessions[0].state, s_states[mode - AM_LED_PERMISSION]);
    }
    struct am_led_effect effect = am_led_select(&model, &selection);
    assert(effect.mode == mode);
    return effect;
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

static unsigned channel_sum(const rgb_led_color_t colors[16])
{
    unsigned sum = 0;
    for (int i = 0; i < 16; ++i) {
        sum += colors[i].r + colors[i].g + colors[i].b;
    }
    return sum;
}

static void check_timing(enum am_led_mode mode, const int *times, const unsigned *masks, size_t length)
{
    struct am_led_effect effect = selected_effect(mode);
    struct am_led_state state = {0};
    rgb_led_color_t colors[16];
    for (size_t i = 0; i < length; ++i) {
        am_led_frame(&state, &effect, times[i] * 1000LL, colors);
        assert(lit_mask(colors) == masks[i]);
    }
}

void test_led_patterns(void)
{
    const int disconnected[] = {0, 499, 500, 3999, 4000};
    const unsigned disconnected_masks[] = {0x0707, 0x0707, 0, 0, 0x0707};
    check_timing(AM_LED_DISCONNECTED, disconnected, disconnected_masks, 5);
    const int permission[] = {0, 149, 150, 299, 300, 449, 450, 1999, 2000};
    const unsigned permission_masks[] = {0xffff, 0xffff, 0, 0, 0xffff, 0xffff, 0, 0, 0xffff};
    check_timing(AM_LED_PERMISSION, permission, permission_masks, 9);
    const int input[] = {0, 599, 600, 1999, 2000};
    const unsigned input_masks[] = {0x0f0f, 0x0f0f, 0, 0, 0x0f0f};
    check_timing(AM_LED_INPUT, input, input_masks, 5);
    const int success[] = {0, 149, 150, 299, 300, 449, 450, 1199, 1200, 2399, 2400};
    const unsigned success_masks[] = {0x000f, 0x000f, 0x00ff, 0x00ff, 0x0fff, 0x0fff, 0xffff, 0xffff, 0, 0, 0x000f};
    check_timing(AM_LED_SUCCESS, success, success_masks, 11);
    const int failed[] = {0, 199, 200, 2399, 2400};
    const unsigned failed_masks[] = {0xffff, 0xffff, 0, 0, 0xffff};
    check_timing(AM_LED_FAILED, failed, failed_masks, 5);
    const int canceled[] = {0, 299, 300, 2999, 3000};
    const unsigned canceled_masks[] = {0x0f0f, 0x0f0f, 0, 0, 0x0f0f};
    check_timing(AM_LED_CANCELED, canceled, canceled_masks, 5);
    const enum am_led_mode modes[] = {AM_LED_CONNECTING, AM_LED_WORKING, AM_LED_THINKING};
    const unsigned masks[] = {0xe1e1, 0xf801, 0xc1c1};
    const unsigned sums[] = {2032, 1607, 1476};
    const int steps[] = {100, 150, 250};
    for (int m = 0; m < 3; ++m) {
        struct am_led_state state = {0};
        struct am_led_effect effect = selected_effect(modes[m]);
        rgb_led_color_t colors[16], first[16];
        am_led_frame(&state, &effect, 0, first);
        assert(lit_mask(first) == masks[m] && channel_sum(first) == sums[m]);
        for (int step = 0; step <= 16; ++step) {
            am_led_frame(&state, &effect, step * steps[m] * 1000LL, colors);
            for (int i = 0; i < 16; ++i) {
                assert(memcmp(&colors[(i + step) % 16], &first[i], sizeof(colors[i])) == 0);
            }
            assert(channel_sum(colors) == sums[m]);
            am_led_frame(&state, &effect, (step + 1) * steps[m] * 1000LL - 1, colors);
            assert(channel_sum(colors) == sums[m]);
            for (int i = 0; i < 16; ++i) {
                assert(memcmp(&colors[(i + step) % 16], &first[i], sizeof(colors[i])) == 0);
            }
        }
        // A long scheduling gap computes the same phase directly, without replaying steps.
        am_led_frame(&state, &effect, 16LL * steps[m] * 1000 * 1000000000, colors);
        assert(memcmp(first, colors, sizeof(first)) == 0);
    }
    struct am_led_state state = {0};
    struct am_led_effect effect = selected_effect(AM_LED_WORKING);
    rgb_led_color_t colors[16];
    am_led_frame(&state, &effect, 0, colors);
    assert(colors[0].r == 54 && colors[0].g == 226 && colors[0].b == 154);
    assert(colors[14].r == 41 && colors[14].g == 170 && colors[14].b == 116);
    assert(colors[11].r == 8 && colors[11].g == 34 && colors[11].b == 23);
    am_led_frame(&state, &effect, 150000, colors);
    assert(state.started_us == 0 && lit_mask(colors) == 0xf003);
    effect.generation++;
    am_led_frame(&state, &effect, 200000, colors);
    assert(state.started_us == 200000 && lit_mask(colors) == 0xf801);
    strcpy(effect.key, "s2");
    am_led_frame(&state, &effect, 350000, colors);
    assert(state.started_us == 350000);
    effect.color.r++;
    am_led_frame(&state, &effect, 500000, colors);
    assert(state.started_us == 500000);
    effect = selected_effect(AM_LED_STALE);
    am_led_frame(&state, &effect, 600000, colors);
    rgb_led_color_t first[16];
    memcpy(first, colors, sizeof(first));
    assert(lit_mask(colors) == 0x0707 && channel_sum(colors) == 1236 && colors[0].r == 98 && colors[8].r == 24);
    am_led_frame(&state, &effect, 2099999, colors);
    assert(memcmp(first, colors, sizeof(first)) == 0);
    am_led_frame(&state, &effect, 2100000, colors);
    assert(lit_mask(colors) == 0x0707 && channel_sum(colors) == 1236);
    assert(colors[0].r == 24 && colors[8].r == 98);
    am_led_frame(&state, &effect, 3600000, colors);
    assert(memcmp(first, colors, sizeof(first)) == 0);
    effect = selected_effect(AM_LED_OFF);
    am_led_frame(&state, &effect, 4000000, colors);
    assert(channel_sum(colors) == 0);
    // Selection follows the displayed slot; ordinary telemetry does not restart an effect.
    am_model_t model = {.link = AM_LINK_ONLINE};
    struct am_ui_selection selection = {.count = 2, .slots = {0, 1}, .current = 1};
    strcpy(model.sessions[0].state, "permission_required");
    strcpy(model.sessions[1].state, "working");
    strcpy(model.sessions[1].key, "chosen");
    effect = am_led_select(&model, &selection);
    am_led_frame(&state, &effect, 5000000, colors);
    model.sessions[1].rev++;
    model.sessions[1].task_seconds++;
    strcpy(model.sessions[1].project_name, "updated");
    effect = am_led_select(&model, &selection);
    am_led_frame(&state, &effect, 5150000, colors);
    assert(effect.mode == AM_LED_WORKING && state.started_us == 5000000 && lit_mask(colors) == 0xf003);
    model.link = AM_LINK_DEGRADED;
    effect = am_led_select(&model, &selection);
    assert(effect.mode == AM_LED_DISCONNECTED && effect.color.r == 255);
    model.link = AM_LINK_HANDSHAKE;
    effect = am_led_select(&model, &selection);
    assert(effect.mode == AM_LED_CONNECTING && effect.color.g == 159);
}

void write_led_preview(const char *directory)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/led_frames.csv", directory);
    FILE *file = fopen(path, "w");
    assert(file);
    fprintf(file, "mode,ms");
    for (int i = 0; i < 16; ++i) {
        fprintf(file, ",r%d,g%d,b%d", i, i, i);
    }
    fputc('\n', file);
    for (int mode = AM_LED_OFF; mode <= AM_LED_CANCELED; ++mode) {
        struct am_led_effect effect = selected_effect(mode);
        struct am_led_state state = {0};
        rgb_led_color_t colors[16];
        for (int ms = 0; ms < 24000; ms += 50) {
            am_led_frame(&state, &effect, ms * 1000LL, colors);
            fprintf(file, "%d,%d", mode, ms);
            for (int i = 0; i < 16; ++i) {
                fprintf(file, ",%u,%u,%u", colors[i].r, colors[i].g, colors[i].b);
            }
            fputc('\n', file);
        }
    }
    assert(fclose(file) == 0);
}
