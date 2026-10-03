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
# Description: AgentMate atomic account usage updates
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-03 1.0      Codex         Created atomic account usage updates.
#
#####################################################################################
*/

#ifndef AGENTMATE_USAGE_H
#define AGENTMATE_USAGE_H

#include "agentmate_app.h"

enum am_usage_error { AM_USAGE_OK, AM_USAGE_BAD_STATE, AM_USAGE_CAPACITY };

/* Caller holds model_lock. Pending batches never change the published model. */
enum am_usage_error am_usage_begin(am_app_t *app, const char *provider, uint64_t id, int64_t now);
enum am_usage_error am_usage_write(am_app_t *app, const am_usage_t *entry);
enum am_usage_error am_usage_end(am_app_t *app, const char *provider, uint64_t id);
void am_usage_clear(am_app_t *app, const char *provider);
void am_usage_expire(am_app_t *app, int64_t now);

#endif
