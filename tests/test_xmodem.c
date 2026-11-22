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
# Description: AgentMate host regression tests for XMODEM receiver
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

#include "xmodem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct am_xmodem s_receiver;
static uint8_t s_written[70000];
static uint8_t s_sent[4096];
static size_t s_length, s_sent_count, s_writes;
static bool s_write_fail;
static int64_t s_now;

static void send_byte(void *context, uint8_t byte)
{
    (void)context;
    assert(s_sent_count < sizeof(s_sent));
    s_sent[s_sent_count++] = byte;
}

static bool write_data(void *context, const uint8_t *data, size_t length)
{
    (void)context;
    if (s_write_fail) {
        return false;
    }
    assert(s_length + length <= sizeof(s_written));
    memcpy(s_written + s_length, data, length);
    s_length += length;
    s_writes++;
    return true;
}

static void start(uint32_t size)
{
    s_length = 0;
    s_sent_count = 0;
    s_writes = 0;
    s_write_fail = false;
    s_now = 1;
    struct am_xmodem_io io = {.send = send_byte, .write = write_data};
    am_xmodem_start(&s_receiver, &io, size, s_now);
    assert(s_sent_count == 1 && s_sent[0] == 'C');
}

static void feed(uint8_t byte)
{
    s_now += 10;
    am_xmodem_feed(&s_receiver, byte, s_now);
}

static void packet(uint8_t number, size_t length, uint8_t seed, bool corrupt)
{
    uint8_t bytes[1024];
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = (uint8_t)(seed + i);
    }
    uint16_t crc = am_xmodem_crc(bytes, length);
    feed(length == 128 ? 1 : 2);
    feed(number);
    feed((uint8_t)~number);
    for (size_t i = 0; i < length; ++i) {
        feed(bytes[i]);
        am_xmodem_tick(&s_receiver, s_now);
    }
    feed((uint8_t)(crc >> 8));
    feed((uint8_t)(crc ^ (corrupt ? 1 : 0)));
}

static void error_is(const char *error)
{
    assert(s_receiver.result == AM_XMODEM_FAILED);
    assert(strcmp(s_receiver.error, error) == 0);
    assert(s_sent[s_sent_count - 1] == 0x18 && s_sent[s_sent_count - 2] == 0x18);
}

static void finish(void)
{
    feed(4);
    assert(s_sent[s_sent_count - 1] == 6);
    feed(4);
    assert(s_sent[s_sent_count - 1] == 6);
    am_xmodem_tick(&s_receiver, s_now + 2000000);
    assert(s_receiver.result == AM_XMODEM_COMPLETE);
}

int main(void)
{
    assert(am_xmodem_crc((const uint8_t *)"123456789", 9) == 0x31c3);
    start(1157);
    packet(1, 1024, 0, false);
    packet(2, 128, 7, false);
    packet(3, 128, 11, false);
    assert(s_length == 1157 && s_writes == 3 && s_written[1156] == 15);
    finish();

    start(128);
    packet(1, 128, 1, true);
    assert(s_length == 0 && s_sent[s_sent_count - 1] == 0x15);
    packet(1, 128, 1, false);
    packet(1, 128, 1, false);
    assert(s_length == 128 && s_writes == 1);
    finish();

    start(256);
    packet(1, 128, 1, false);
    packet(1, 128, 2, false);
    error_is("OTA_SEQUENCE");
    start(256);
    packet(2, 128, 0, false);
    error_is("OTA_SEQUENCE");
    start(128);
    packet(0, 128, 0, false);
    error_is("OTA_SEQUENCE");
    start(128);
    packet(1, 128, 0, false);
    packet(2, 128, 0, false);
    error_is("OTA_SIZE");
    start(128);
    feed(4);
    error_is("OTA_SIZE");
    start(128);
    feed(0x18);
    feed(0x18);
    error_is("OTA_CANCELLED");
    start(128);
    packet(1, 128, 0x18, false);
    finish();
    start(128);
    s_write_fail = true;
    packet(1, 128, 0, false);
    error_is("OTA_WRITE");
    assert(s_receiver.received == 0);

    start(128 * 260);
    for (unsigned i = 1; i <= 260; ++i) {
        packet((uint8_t)i, 128, (uint8_t)i, false);
        if (i == 256) {
            packet(0, 128, 0, false);
        }
    }
    assert(s_length == 128 * 260 && s_writes == 260);
    finish();

    start(128);
    for (unsigned i = 0; i < 10; ++i) {
        packet(1, 128, 0, true);
    }
    error_is("OTA_RETRY_LIMIT");
    start(128);
    am_xmodem_tick(&s_receiver, s_now + 1000000);
    assert(s_sent_count == 2 && s_sent[1] == 'C');
    am_xmodem_tick(&s_receiver, s_now + 30000000);
    error_is("OTA_TIMEOUT");

    start(256);
    feed(1);
    feed(1);
    s_now += 3000000;
    am_xmodem_tick(&s_receiver, s_now);
    assert(s_receiver.resync && s_receiver.used == 0);
    feed(0x41);
    s_now += 200000;
    am_xmodem_tick(&s_receiver, s_now);
    assert(!s_receiver.resync && s_sent[s_sent_count - 1] == 0x15);
    packet(1, 128, 1, false);
    packet(2, 128, 2, false);
    finish();

    start(256);
    packet(1, 128, 1, false);
    for (unsigned i = 1; i <= 30; ++i) {
        s_now += 1000000;
        feed(0x33);
        am_xmodem_tick(&s_receiver, s_now);
    }
    error_is("OTA_TIMEOUT");
    start(256);
    packet(1, 128, 1, false);
    for (unsigned i = 1; i <= 30; ++i) {
        s_now += 1000000;
        packet(1, 128, 1, false);
        am_xmodem_tick(&s_receiver, s_now);
        if (s_receiver.result == AM_XMODEM_FAILED) {
            break;
        }
    }
    error_is("OTA_TIMEOUT");
    puts("PASS XMODEM: CRC, mixed blocks, padding, retransmission, wrap, cancellation, failure and recovery");
    return 0;
}
