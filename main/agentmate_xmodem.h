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
# Description: AgentMate bounded XMODEM-CRC receiver interfaces
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-12 1.0      Codex         Created UART OTA implementation.
#
#####################################################################################
*/

#ifndef AGENTMATE_XMODEM_H
#define AGENTMATE_XMODEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum am_xmodem_result { AM_XMODEM_ACTIVE, AM_XMODEM_COMPLETE, AM_XMODEM_FAILED };

struct am_xmodem_io {
    void *context;
    void (*send)(void *context, uint8_t byte);
    bool (*write)(void *context, const uint8_t *data, size_t length);
};

struct am_xmodem {
    struct am_xmodem_io io;
    enum am_xmodem_result result;
    const char *error;
    uint32_t size;
    uint32_t received;
    uint8_t expected;
    unsigned errors;
    uint8_t packet[1029];
    size_t used;
    size_t packet_size;
    uint8_t previous[1024];
    size_t previous_size;
    bool have_previous;
    bool can_seen;
    bool resync;
    bool ending;
    int64_t last_us;
    int64_t progress_us;
    int64_t packet_started_us;
    int64_t request_us;
    int64_t ending_us;
};

uint16_t am_xmodem_crc(const uint8_t *data, size_t length);
void am_xmodem_start(struct am_xmodem *receiver, const struct am_xmodem_io *io, uint32_t size, int64_t now);
void am_xmodem_feed(struct am_xmodem *receiver, uint8_t byte, int64_t now);
void am_xmodem_tick(struct am_xmodem *receiver, int64_t now);

#endif
