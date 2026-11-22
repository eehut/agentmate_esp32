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
# Description: AgentMate portable XMODEM-CRC and XMODEM-1K receiver
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

#include <string.h>

#define AM_X_SOH 0x01
#define AM_X_STX 0x02
#define AM_X_EOT 0x04
#define AM_X_ACK 0x06
#define AM_X_NAK 0x15
#define AM_X_CAN 0x18
#define AM_X_CRC 0x43

uint16_t am_xmodem_crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
        }
    }
    return crc;
}

static void send_byte(struct am_xmodem *receiver, uint8_t byte)
{
    receiver->io.send(receiver->io.context, byte);
}

static void fail(struct am_xmodem *receiver, const char *error)
{
    receiver->error = error;
    receiver->result = AM_XMODEM_FAILED;
    send_byte(receiver, AM_X_CAN);
    send_byte(receiver, AM_X_CAN);
}

static void retry(struct am_xmodem *receiver)
{
    receiver->used = 0;
    receiver->can_seen = false;
    if (++receiver->errors >= 10) {
        fail(receiver, "OTA_RETRY_LIMIT");
    } else {
        send_byte(receiver, AM_X_NAK);
    }
}

void am_xmodem_start(struct am_xmodem *receiver, const struct am_xmodem_io *io, uint32_t size, int64_t now)
{
    memset(receiver, 0, sizeof(*receiver));
    receiver->io = *io;
    receiver->size = size;
    receiver->expected = 1;
    receiver->last_us = now;
    receiver->progress_us = now;
    receiver->request_us = now;
    send_byte(receiver, AM_X_CRC);
}

static void accept_packet(struct am_xmodem *receiver)
{
    size_t length = receiver->packet_size - 5;
    uint8_t *packet = receiver->packet;
    uint16_t crc = (uint16_t)((packet[length + 3] << 8) | packet[length + 4]);
    if ((uint8_t)(packet[1] ^ packet[2]) != 0xff || am_xmodem_crc(packet + 3, length) != crc) {
        retry(receiver);
        return;
    }
    receiver->used = 0;
    if (receiver->have_previous && packet[1] == (uint8_t)(receiver->expected - 1)) {
        if (length != receiver->previous_size || memcmp(packet + 3, receiver->previous, length) != 0) {
            fail(receiver, "OTA_SEQUENCE");
            return;
        }
        receiver->errors = 0;
        send_byte(receiver, AM_X_ACK);
        return;
    }
    if (packet[1] != receiver->expected) {
        fail(receiver, "OTA_SEQUENCE");
        return;
    }
    if (receiver->received == receiver->size) {
        fail(receiver, "OTA_SIZE");
        return;
    }
    size_t remaining = receiver->size - receiver->received;
    size_t actual = remaining < length ? remaining : length;
    if (!receiver->io.write(receiver->io.context, packet + 3, actual)) {
        fail(receiver, "OTA_WRITE");
        return;
    }
    memcpy(receiver->previous, packet + 3, length);
    receiver->previous_size = length;
    receiver->have_previous = true;
    receiver->received += actual;
    receiver->progress_us = receiver->last_us;
    receiver->expected++;
    receiver->errors = 0;
    send_byte(receiver, AM_X_ACK);
}

void am_xmodem_feed(struct am_xmodem *receiver, uint8_t byte, int64_t now)
{
    if (receiver->result != AM_XMODEM_ACTIVE) {
        return;
    }
    receiver->last_us = now;
    if (receiver->ending) {
        if (byte == AM_X_EOT) {
            send_byte(receiver, AM_X_ACK);
        }
        return;
    }
    if (receiver->resync) {
        return;
    }
    if (receiver->used) {
        receiver->packet[receiver->used++] = byte;
        if (receiver->used == receiver->packet_size) {
            accept_packet(receiver);
        }
        return;
    }
    if (byte == AM_X_CAN) {
        if (receiver->can_seen) {
            fail(receiver, "OTA_CANCELLED");
        }
        receiver->can_seen = true;
        return;
    }
    receiver->can_seen = false;
    if (byte == AM_X_SOH || byte == AM_X_STX) {
        receiver->packet_started_us = now;
        receiver->packet[0] = byte;
        receiver->used = 1;
        receiver->packet_size = byte == AM_X_SOH ? 133 : 1029;
    } else if (byte == AM_X_EOT) {
        if (receiver->received != receiver->size) {
            fail(receiver, "OTA_SIZE");
        } else {
            receiver->ending = true;
            receiver->ending_us = now;
            send_byte(receiver, AM_X_ACK);
        }
    } else {
        /* Drain noise to a quiet boundary before requesting a complete retransmission. */
        receiver->resync = true;
        receiver->used = 0;
    }
}

void am_xmodem_tick(struct am_xmodem *receiver, int64_t now)
{
    if (receiver->result != AM_XMODEM_ACTIVE) {
        return;
    }
    if (receiver->ending) {
        if (now - receiver->ending_us >= 2000000) {
            receiver->result = AM_XMODEM_COMPLETE;
        }
        return;
    }
    if (now - receiver->progress_us >= 30000000) {
        fail(receiver, "OTA_TIMEOUT");
        return;
    }
    if (receiver->resync) {
        if (now - receiver->last_us >= 200000) {
            receiver->resync = false;
            receiver->last_us = now;
            receiver->request_us = now;
            retry(receiver);
        }
    } else if (receiver->used && now - receiver->packet_started_us >= 3000000) {
        receiver->used = 0;
        receiver->resync = true;
    } else if (!receiver->used) {
        if (!receiver->have_previous && now - receiver->request_us >= 1000000) {
            send_byte(receiver, AM_X_CRC);
            receiver->request_us = now;
        } else if (receiver->have_previous && now - receiver->last_us >= 3000000) {
            receiver->last_us = now;
            retry(receiver);
        }
    }
}
