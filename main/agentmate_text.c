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
# Description: AgentMate UTF-8 and AT text helpers
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-04 2.0      Codex         Validate protocol 2 host and session labels.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#include "agentmate_text.h"
#include <string.h>

#include <stddef.h>

uint32_t am_utf8_next(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    unsigned char first = *p++;
    if (first < 0x80) {
        *text = (const char *)p;
        return first;
    }
    int count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : -1;
    if (count < 0) {
        *text = (const char *)p;
        return 0xfffd;
    }
    uint32_t code = first & ((1U << (6 - count)) - 1);
    for (int i = 0; i < count; ++i) {
        if ((p[i] & 0xc0) != 0x80) {
            *text = (const char *)p;
            return 0xfffd;
        }
        code = (code << 6) | (p[i] & 0x3f);
    }
    *text = (const char *)(p + count);
    if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) || (count == 3 && code < 0x10000) ||
        (code >= 0xd800 && code <= 0xdfff) || code > 0x10ffff) {
        return 0xfffd;
    }
    return code;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

bool am_project_decode(const char *encoded, char *out)
{
    size_t length = 0;
    while (*encoded) {
        unsigned char ch = (unsigned char)*encoded++;
        if (ch == '%') {
            if (!encoded[0] || !encoded[1]) {
                return false;
            }
            int hi = hex_value(encoded[0]);
            int lo = hex_value(encoded[1]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            ch = (unsigned char)(hi * 16 + lo);
            encoded += 2;
        } else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                     ch == '-' || ch == '_' || ch == '.' || ch == '~')) {
            return false;
        }
        if (length == 64 || ch < 0x20 || ch == 0x7f || ch == '/' || ch == '\\') {
            return false;
        }
        out[length++] = (char)ch;
    }
    out[length] = 0;
    const char *p = out;
    while (*p) {
        uint32_t code = am_utf8_next(&p);
        if (code == 0xfffd || (code >= 0x80 && code < 0xa0)) {
            return false;
        }
    }
    return true;
}

void am_project_encode(const char *text, char *out)
{
    static const char s_hex[] = "0123456789ABCDEF";
    while (*text) {
        unsigned char ch = (unsigned char)*text++;
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            *out++ = (char)ch;
        } else {
            *out++ = '%';
            *out++ = s_hex[ch >> 4];
            *out++ = s_hex[ch & 15];
        }
    }
    *out = 0;
}

bool am_parse_offset(const char *text, int16_t *offset)
{
    int sign = 1;
    if (*text == '-' || *text == '+') {
        sign = *text++ == '-' ? -1 : 1;
    }
    if (!*text) {
        return false;
    }
    int value = 0;
    while (*text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        value = value * 10 + *text++ - '0';
        if (value > 840) {
            return false;
        }
    }
    *offset = (int16_t)(sign * value);
    return true;
}


bool am_host_label_valid(const char *value)
{
    size_t length = strlen(value);
    if (length < 3 || length > 8) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!((value[i] >= 'A' && value[i] <= 'Z') || (value[i] >= '0' && value[i] <= '9') || value[i] == '_')) {
            return false;
        }
    }
    return true;
}

bool am_session_short_name_valid(const char *value)
{
    size_t length = strlen(value);
    if (length < 2 || length > 11 || value[0] != 'S' || value[1] < '1' || value[1] > '9') {
        return false;
    }
    uint64_t number = 0;
    for (size_t i = 1; i < length; ++i) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
        number = number * 10 + (uint64_t)(value[i] - '0');
    }
    return number <= UINT32_MAX;
}
