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
# Description: AgentMate serial OTA lifecycle and boot validation interfaces
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

#ifndef AGENTMATE_OTA_H
#define AGENTMATE_OTA_H

#include "app.h"
#include <stddef.h>

/* These functions belong exclusively to the UART protocol task. No model lock may be held. */
const char *am_ota_state_name(enum am_ota_state state);
uint32_t am_ota_max_size(void);
const char *am_ota_begin(am_app_t *app, const char *version, uint32_t size, const char *sha256);
const char *am_ota_commit(am_app_t *app);
void am_ota_abort(am_app_t *app);
bool am_ota_serial_active(void);
void am_ota_start_serial(am_app_t *app, bool skip_lf);
void am_ota_serial_feed(am_app_t *app, const uint8_t *data, size_t length);
/* Returns true exactly once when AT parsing can resume and the terminal result should be printed. */
bool am_ota_serial_tick(am_app_t *app);
void am_ota_boot_watch(am_app_t *app);

#endif
