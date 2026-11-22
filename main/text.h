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
# 2026-10-04 2.0      Codex         Declare protocol 2 label validation.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#ifndef AGENTMATE_TEXT_H
#define AGENTMATE_TEXT_H

#include <stdbool.h>
#include <stdint.h>

uint32_t am_utf8_next(const char **text);
bool am_project_decode(const char *encoded, char *out);
void am_project_encode(const char *text, char *out);
bool am_parse_offset(const char *text, int16_t *offset);

bool am_host_label_valid(const char *value);
bool am_session_short_name_valid(const char *value);

#endif
