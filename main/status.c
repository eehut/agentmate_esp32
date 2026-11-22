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
# Description: AgentMate status animation timing and rounded border geometry
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-03 1.0      Codex         Created continuously animated status borders.
#
#####################################################################################
*/

#include "status.h"

#include <math.h>
#include <string.h>

/* One-time geometry cache: distance and perimeter position, not a color framebuffer. */
static uint16_t s_geometry[AM_STATUS_WIDTH * AM_STATUS_HEIGHT];
static bool s_geometry_ready;

static enum am_status_kind status_kind(const char *state)
{
    if (strcmp(state, "thinking") == 0) {
        return AM_STATUS_THINKING;
    }
    if (strcmp(state, "waiting_for_input") == 0) {
        return AM_STATUS_INPUT;
    }
    if (strcmp(state, "permission_required") == 0) {
        return AM_STATUS_PERMISSION;
    }
    if (strcmp(state, "stale") == 0) {
        return AM_STATUS_STALE;
    }
    if (strcmp(state, "task_success") == 0) {
        return AM_STATUS_SUCCESS;
    }
    if (strcmp(state, "task_failed") == 0) {
        return AM_STATUS_FAILED;
    }
    if (strcmp(state, "task_canceled") == 0) {
        return AM_STATUS_CANCELED;
    }
    return AM_STATUS_WORKING;
}

struct am_status_visual am_status_select(struct am_status_animation *animation, const am_session_t *session,
                                        uint32_t generation, int64_t now)
{
    enum am_status_kind kind = status_kind(session->state);
    if (!animation->initialized || animation->kind != kind || animation->generation != generation ||
        strcmp(animation->key, session->key) != 0) {
        animation->initialized = true;
        animation->kind = kind;
        animation->generation = generation;
        memcpy(animation->key, session->key, sizeof(animation->key));
        animation->started_us = now;
    }
    static const uint16_t s_colors[] = {
        TFT_RGB565(54, 226, 154), TFT_RGB565(139, 153, 255), TFT_RGB565(255, 176, 46), TFT_RGB565(255, 95, 122),
        TFT_RGB565(163, 175, 211), TFT_RGB565(54, 226, 154), TFT_RGB565(255, 59, 48), TFT_RGB565(74, 168, 255)
    };
    static const uint8_t s_periods[] = {24, 40, 20, 20, 30, 40, 24, 40};
    int64_t elapsed = now > animation->started_us ? now - animation->started_us : 0;
    return (struct am_status_visual){kind, s_colors[kind], (uint8_t)((elapsed / AM_STATUS_TICK_US) % s_periods[kind])};
}

static void initialize_geometry(void)
{
    const float arc = 15.70796327f;
    const float perimeter = 2 * (116 + 21) + 4 * arc;
    for (int y = 0; y < AM_STATUS_HEIGHT; ++y) {
        for (int x = 0; x < AM_STATUS_WIDTH; ++x) {
            float dx = fabsf(x - 71.0f) - 58.0f;
            float dy = fabsf(y - 23.5f) - 10.5f;
            float u = fmaxf(dx, 0), v = fmaxf(dy, 0);
            float distance = fabsf(sqrtf(u * u + v * v) + fminf(fmaxf(dx, dy), 0) - 10);
            size_t index = (size_t)y * AM_STATUS_WIDTH + x;
            s_geometry[index] = UINT16_MAX;
            if (distance > 3.5f) {
                continue;
            }
            float position;
            if (dx > 0 && dy > 0) {
                if (x >= 71 && y < 23.5f) {
                    position = 116 + 10 * atan2f(u, v);
                } else if (x >= 71) {
                    position = 116 + arc + 21 + 10 * atan2f(v, u);
                } else if (y >= 23.5f) {
                    position = 232 + 2 * arc + 21 + 10 * atan2f(u, v);
                } else {
                    position = 232 + 3 * arc + 42 + 10 * atan2f(v, u);
                }
            } else if (dy >= dx) {
                position = y < 23.5f ? x - 13 : 116 + 2 * arc + 21 + 129 - x;
            } else {
                position = x >= 71 ? 116 + arc + y - 13 : 232 + 3 * arc + 21 + 34 - y;
            }
            unsigned phase = (unsigned)(position * 256 / perimeter + 256) % 256;
            s_geometry[index] = (uint16_t)((phase << 8) | (unsigned)(distance * 32 + .5f));
        }
    }
    s_geometry_ready = true;
}

void am_status_prepare(void)
{
    if (!s_geometry_ready) {
        initialize_geometry();
    }
}

static unsigned trail(unsigned position, unsigned head, unsigned length, bool reverse)
{
    unsigned distance = reverse ? (position + 256 - head) % 256 : (head + 256 - position) % 256;
    return distance < length ? (length - distance) * 255 / length : 0;
}

static unsigned band(unsigned position, unsigned center)
{
    unsigned distance = (position + 256 - center) % 256;
    if (distance > 128) {
        distance = 256 - distance;
    }
    return distance < 64 ? (64 - distance) * 255 / 64 : 0;
}

static unsigned triangle(unsigned frame, unsigned period)
{
    frame %= period;
    unsigned half = period / 2;
    return frame <= half ? frame * 255 / half : (period - frame) * 255 / (period - half);
}

static unsigned highlight(enum am_status_kind kind, unsigned frame, unsigned position)
{
    if (kind == AM_STATUS_WORKING) {
        return trail(position, frame * 256 / 24, 48, false);
    }
    if (kind == AM_STATUS_THINKING) {
        unsigned first = trail(position, frame * 256 / 40, 32, false);
        unsigned second = trail(position, (frame * 256 / 40 + 128) % 256, 32, false);
        return first > second ? first : second;
    }
    if (kind == AM_STATUS_INPUT || kind == AM_STATUS_PERMISSION) {
        static const uint8_t s_pulse[] = {160, 255, 160, 40, 160, 255, 160, 40, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 40};
        unsigned fog = band(position, frame * 256 / 20);
        return (s_pulse[frame % 20] * (128 + fog / 2) + 32 * fog) / 287;
    }
    if (kind == AM_STATUS_STALE || kind == AM_STATUS_FAILED) {
        unsigned amount = triangle(frame, kind == AM_STATUS_STALE ? 30 : 24);
        unsigned center = kind == AM_STATUS_STALE ? 64 : 0;
        return (band(position, center) * amount + band(position, center + 128) * (255 - amount)) / 255;
    }
    bool reverse = kind == AM_STATUS_CANCELED;
    unsigned head = reverse ? (256 - frame * 256 / 40) % 256 : frame * 256 / 40;
    return trail(position, head, 28, reverse);
}

unsigned am_status_border_alpha(enum am_status_kind kind, unsigned frame, int x, int y)
{
    if (x < 0 || x >= AM_STATUS_WIDTH || y < 0 || y >= AM_STATUS_HEIGHT) {
        return 0;
    }
    am_status_prepare();
    uint16_t geometry = s_geometry[y * AM_STATUS_WIDTH + x];
    if (geometry == UINT16_MAX) {
        return 0;
    }
    unsigned distance = geometry & 255;
    unsigned edge = distance <= 16 ? 255 : distance < 48 ? (48 - distance) * 255 / 32 : 0;
    unsigned glow = distance < 112 ? (112 - distance) * (112 - distance) * 80 / (112 * 112) : 0;
    unsigned light = highlight(kind, frame, geometry >> 8);
    unsigned alpha = (edge * (65 + light * 190 / 255) + glow * light) / 255;
    return alpha > 255 ? 255 : alpha;
}
