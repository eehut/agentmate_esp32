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
# Description: AgentMate animated session status border
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

#ifndef AGENTMATE_STATUS_H
#define AGENTMATE_STATUS_H

#include "app.h"

#define AM_STATUS_WIDTH 143
#define AM_STATUS_HEIGHT 48
#define AM_STATUS_TICK_US 100000LL

enum am_status_kind {
    AM_STATUS_WORKING,
    AM_STATUS_THINKING,
    AM_STATUS_INPUT,
    AM_STATUS_PERMISSION,
    AM_STATUS_STALE,
    AM_STATUS_SUCCESS,
    AM_STATUS_FAILED,
    AM_STATUS_CANCELED,
};

struct am_status_animation {
    bool initialized;
    enum am_status_kind kind;
    char key[9];
    uint32_t generation;
    int64_t started_us;
};

struct am_status_visual {
    enum am_status_kind kind;
    uint16_t color;
    uint8_t frame;
};

void am_status_prepare(void);
struct am_status_visual am_status_select(struct am_status_animation *animation, const am_session_t *session,
                                        uint32_t generation, int64_t now);
unsigned am_status_border_alpha(enum am_status_kind kind, unsigned frame, int x, int y);

#endif
