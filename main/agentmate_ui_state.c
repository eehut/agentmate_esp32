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
# Description: AgentMate page selection and standby rotation
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-24 4.6      Codex         Share session display eligibility with arc indicators.
# 2026-10-13 1.7      Codex         Preview clock themes for five seconds below attention priority.
# 2026-10-11 1.6      Codex         Skip invalid Session metrics and use ten/three/three-second phases.
# 2026-10-11 1.5      Codex         Rotate Session duration, context and available quota independently.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-09 1.4      Codex         Select session context every five seconds.
# 2026-10-06 1.3      Codex         Select published Codex quota windows for session pages too.
# 2026-10-03 1.2      Codex         Prioritize attention, results and work without same-group interruption.
# 2026-10-03 1.1      Codex         Rotate usage and clock within standby every five seconds.
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#include "agentmate_ui_state.h"

#include <string.h>

bool am_state_advancing(const char *state)
{
    return strcmp(state, "working") == 0 || strcmp(state, "thinking") == 0 || strcmp(state, "waiting_for_input") == 0 ||
           strcmp(state, "permission_required") == 0;
}

bool am_state_result(const char *state)
{
    return strcmp(state, "task_success") == 0 || strcmp(state, "task_failed") == 0 || strcmp(state, "task_canceled") == 0;
}

uint64_t am_task_duration(const am_session_t *session, int64_t now)
{
    uint64_t seconds = session->task_seconds;
    if (am_state_advancing(session->state) && now > session->task_base_us) {
        seconds += (uint64_t)(now - session->task_base_us) / 1000000;
    }
    return seconds;
}

static enum am_ui_group session_group(const char *state)
{
    if (strcmp(state, "permission_required") == 0 || strcmp(state, "waiting_for_input") == 0) {
        return AM_GROUP_ATTENTION;
    }
    if (am_state_result(state)) {
        return AM_GROUP_RESULT;
    }
    if (strcmp(state, "working") == 0 || strcmp(state, "thinking") == 0 || strcmp(state, "stale") == 0) {
        return AM_GROUP_WORK;
    }
    return AM_GROUP_IDLE;
}

bool am_session_displayable(const am_session_t *session)
{
    return session->used && session_group(session->state) != AM_GROUP_IDLE;
}

static enum am_ui_group highest_group(const am_model_t *model)
{
    enum am_ui_group group = AM_GROUP_IDLE;
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used) {
            enum am_ui_group candidate = session_group(model->sessions[i].state);
            if (candidate < group) {
                group = candidate;
            }
        }
    }
    return group;
}

void am_ui_request_clock_preview(am_model_t *model, int64_t now)
{
    model->clock_preview_until_us = 0;
    if (!model->factory_mode && highest_group(model) != AM_GROUP_ATTENTION) {
        model->clock_preview_until_us = now + 5000000;
        model->info_until_us = 0;
    }
    model->revision++;
}

bool am_event_in_group(const am_model_t *model)
{
    if (model->link != AM_LINK_ONLINE) {
        return false;
    }
    enum am_ui_group group = highest_group(model);
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        const am_session_t *entry = &model->sessions[i];
        if (entry->used && strcmp(entry->key, model->alert_key) == 0 && strcmp(entry->provider, model->alert_provider) == 0 &&
            session_group(entry->state) == group && group != AM_GROUP_IDLE) {
            return true;
        }
    }
    return false;
}

bool am_session_context_available(const am_session_t *session)
{
    return session->context_mode == 2 && session->ctx_window > 0 && session->ctx_pct_bp <= 10000;
}

bool am_session_quota_available(const am_usage_t *usage)
{
    return usage->used && strcmp(usage->provider, "codex") == 0 &&
           (usage->window_seconds == 18000 || usage->window_seconds == 604800) && usage->used_bp >= 0 && usage->used_bp <= 10000;
}

static void select_usage(const am_model_t *model, int *windows, bool valid_only)
{
    const char *metric = NULL;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        const am_usage_t *u = &model->usages[i];
        if (u->used && (!valid_only || am_session_quota_available(u)) && strcmp(u->provider, "codex") == 0 &&
            (u->window_seconds == 18000 || u->window_seconds == 604800) &&
            (!metric || strcmp(u->metric, metric) < 0)) {
            metric = u->metric;
        }
    }
    if (!metric) {
        return;
    }
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        const am_usage_t *u = &model->usages[i];
        if (!u->used || (valid_only && !am_session_quota_available(u)) || strcmp(u->provider, "codex") != 0 || strcmp(u->metric, metric) != 0) {
            continue;
        }
        int slot = u->window_seconds == 18000 ? 0 : u->window_seconds == 604800 ? 1 : -1;
        if (slot >= 0 && (windows[slot] < 0 || strcmp(u->window, model->usages[windows[slot]].window) < 0)) {
            windows[slot] = i;
        }
    }
}

void am_ui_select(const am_model_t *model, struct am_ui_schedule *schedule, int64_t now, struct am_ui_selection *selection)
{
    memset(selection, 0, sizeof(*selection));
    selection->usage[0] = -1;
    selection->usage[1] = -1;
    select_usage(model, selection->usage, false);
    if (schedule->remote_generation != model->remote_generation) {
        memset(schedule, 0, sizeof(*schedule));
        schedule->remote_generation = model->remote_generation;
    }
    enum am_ui_group group = model->link == AM_LINK_ONLINE ? highest_group(model) : AM_GROUP_CONNECT;
    selection->group = group;
    bool session_group_active = group == AM_GROUP_ATTENTION || group == AM_GROUP_RESULT || group == AM_GROUP_WORK;
    if (session_group_active) {
        selection->usage[0] = -1;
        selection->usage[1] = -1;
        select_usage(model, selection->usage, true);
    }
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (am_session_displayable(&model->sessions[i]) && session_group_active && session_group(model->sessions[i].state) == group) {
            selection->slots[selection->count++] = i;
        }
    }
    bool changed = group != schedule->group;
    if (changed) {
        selection->preempted = session_group_active && (group < schedule->group || group == AM_GROUP_ATTENTION);
        schedule->current_key[0] = 0;
        schedule->page_since_us = now;
        memset(schedule->detail_elapsed_us, 0, sizeof(schedule->detail_elapsed_us));
        memset(schedule->detail_keys, 0, sizeof(schedule->detail_keys));
    } else if (session_group_active && schedule->current_key[0]) {
        int slot = schedule->current_slot;
        if (now > schedule->detail_updated_us && strcmp(schedule->detail_keys[slot], model->sessions[slot].key) == 0) {
            schedule->detail_elapsed_us[slot] += now - schedule->detail_updated_us;
        }
    }
    schedule->detail_updated_us = now;
    selection->page = group == AM_GROUP_CONNECT ? AM_PAGE_CONNECT : group == AM_GROUP_IDLE ? AM_PAGE_IDLE :
                      group == AM_GROUP_RESULT ? AM_PAGE_RESULT : AM_PAGE_TASK;
    if (group == AM_GROUP_IDLE && model->usage_supported) {
        if (!schedule->idle_usage_supported) {
            schedule->page_since_us = now;
        }
        selection->page = ((now - schedule->page_since_us) / AM_PAGE_DWELL_US) % 2 ? AM_PAGE_IDLE : AM_PAGE_USAGE;
    }
    schedule->idle_usage_supported = model->usage_supported;
    int current = -1;
    int attention = -1;
    for (int i = 0; i < selection->count; ++i) {
        const am_session_t *entry = &model->sessions[selection->slots[i]];
        if (strcmp(entry->key, schedule->current_key) == 0) {
            current = i;
        }
        if (strcmp(entry->key, model->attention_key) == 0) {
            attention = i;
        }
    }
    if (current < 0 && selection->count) {
        current = 0;
        if (changed && group == AM_GROUP_ATTENTION && model->attention_sequence != schedule->attention_sequence && attention >= 0) {
            current = attention;
        } else if (!changed && schedule->current_key[0]) {
            for (int i = 0; i < selection->count; ++i) {
                if (selection->slots[i] > schedule->current_slot) {
                    current = i;
                    break;
                }
            }
        }
        schedule->page_since_us = now;
    } else if (selection->count > 1 && now - schedule->page_since_us >= AM_PAGE_DWELL_US) {
        current = (current + 1) % selection->count;
        schedule->page_since_us = now;
    }
    if (current >= 0) {
        memcpy(schedule->current_key, model->sessions[selection->slots[current]].key, sizeof(schedule->current_key));
        selection->current = current;
        schedule->current_slot = selection->slots[current];
    }
    if (current >= 0) {
        int slot = selection->slots[current];
        const am_session_t *session = &model->sessions[slot];
        if (strcmp(schedule->detail_keys[slot], session->key) != 0) {
            memcpy(schedule->detail_keys[slot], session->key, sizeof(schedule->detail_keys[slot]));
            schedule->detail_elapsed_us[slot] = 0;
        }
        bool codex = strcmp(session->provider, "codex") == 0;
        enum am_session_detail details[3] = {AM_DETAIL_DURATION};
        int64_t dwell[3] = {AM_SESSION_DURATION_US, AM_SESSION_USAGE_US, AM_SESSION_USAGE_US};
        int count = 1;
        if (codex && (selection->usage[0] >= 0 || selection->usage[1] >= 0)) {
            details[count++] = AM_DETAIL_QUOTA;
        }
        if (am_session_context_available(session)) {
            details[count++] = AM_DETAIL_CONTEXT;
        }
        /* Resume each session's visible time on its next visit, including groups of three or six. */
        int64_t age = schedule->detail_elapsed_us[slot];
        int64_t cycle = AM_SESSION_DURATION_US + (count - 1) * AM_SESSION_USAGE_US;
        int64_t phase = age % cycle;
        for (int i = 0; i < count; ++i) {
            if (phase < dwell[i]) {
                selection->detail = details[i];
                break;
            }
            phase -= dwell[i];
        }
    }
    schedule->group = group;
    schedule->attention_sequence = model->attention_sequence;
    if (now < model->info_until_us) {
        selection->page = AM_PAGE_INFO;
    }
    if (now < model->clock_preview_until_us && highest_group(model) != AM_GROUP_ATTENTION && !model->factory_mode) {
        selection->page = AM_PAGE_IDLE;
        selection->clock_preview = true;
        selection->preempted = true;
    }
}
