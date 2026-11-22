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
# Description: AgentMate page selection and animation state
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-13 1.6      Codex         Expose attention-aware clock preview requests.
# 2026-10-11 1.5      Codex         Expose valid Session metric checks and 10/3-second dwell constants.
# 2026-10-11 1.4      Codex         Expose independent Session detail rotation.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-09 1.3      Codex         Expose the session context rotation phase.
# 2026-10-03 1.2      Codex         Separate priority groups and use ten-second dwell.
# 2026-10-03 1.1      Codex         Share standby rotation state without activity tracking.
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#ifndef AGENTMATE_UI_STATE_H
#define AGENTMATE_UI_STATE_H

#include "app.h"

enum am_page {
    AM_PAGE_CONNECT,
    AM_PAGE_TASK,
    AM_PAGE_RESULT,
    AM_PAGE_USAGE,
    AM_PAGE_IDLE,
    AM_PAGE_INFO,
    AM_PAGE_BOOT,
    AM_PAGE_BATTERY,
};

#define AM_SESSION_DURATION_US (10LL * 1000000)
#define AM_SESSION_USAGE_US (3LL * 1000000)
#define AM_PAGE_DWELL_US (10LL * 1000000)

enum am_session_detail { AM_DETAIL_DURATION, AM_DETAIL_CONTEXT, AM_DETAIL_QUOTA };

enum am_ui_group {
    AM_GROUP_CONNECT,
    AM_GROUP_ATTENTION,
    AM_GROUP_RESULT,
    AM_GROUP_WORK,
    AM_GROUP_IDLE,
};

struct am_ui_selection {
    enum am_ui_group group;
    enum am_page page;
    int slots[AM_MAX_SESSIONS];
    int count;
    int current;
    int usage[2];
    enum am_session_detail detail;
    bool preempted;
    bool clock_preview;
};

struct am_ui_schedule {
    enum am_ui_group group;
    char current_key[9];
    int current_slot;
    int64_t page_since_us;
    int64_t detail_updated_us;
    int64_t detail_elapsed_us[AM_MAX_SESSIONS];
    char detail_keys[AM_MAX_SESSIONS][9];
    bool idle_usage_available;
    bool idle_battery_available;
    uint32_t attention_sequence;
    uint32_t remote_generation;
};

bool am_state_advancing(const char *state);
bool am_state_result(const char *state);
bool am_battery_available(const am_model_t *model, int64_t now);
bool am_session_displayable(const am_session_t *session);
bool am_session_context_available(const am_session_t *session);
bool am_session_quota_available(const am_usage_t *usage);
uint64_t am_task_duration(const am_session_t *session, int64_t now);
bool am_event_in_group(const am_model_t *model);
/* Caller holds model_lock; a successful theme setting may replace the information page. */
void am_ui_request_clock_preview(am_model_t *model, int64_t now);
void am_ui_select(const am_model_t *model, struct am_ui_schedule *schedule, int64_t now, struct am_ui_selection *selection);

#endif
