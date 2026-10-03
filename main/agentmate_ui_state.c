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
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
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

static void select_usage(const am_model_t *model, int *windows)
{
    const char *metric = NULL;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        const am_usage_t *u = &model->usages[i];
        if (u->used && strcmp(u->provider, "codex") == 0 && (u->window_seconds == 18000 || u->window_seconds == 604800) &&
            (!metric || strcmp(u->metric, metric) < 0)) {
            metric = u->metric;
        }
    }
    if (!metric) {
        return;
    }
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        const am_usage_t *u = &model->usages[i];
        if (!u->used || strcmp(u->provider, "codex") != 0 || strcmp(u->metric, metric) != 0) {
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
    if (schedule->remote_generation != model->remote_generation) {
        memset(schedule, 0, sizeof(*schedule));
        schedule->remote_generation = model->remote_generation;
    }
    enum am_ui_group group = model->link == AM_LINK_ONLINE ? highest_group(model) : AM_GROUP_CONNECT;
    selection->group = group;
    bool session_group_active = group == AM_GROUP_ATTENTION || group == AM_GROUP_RESULT || group == AM_GROUP_WORK;
    for (int i = 0; i < AM_MAX_SESSIONS; ++i) {
        if (model->sessions[i].used && session_group_active && session_group(model->sessions[i].state) == group) {
            selection->slots[selection->count++] = i;
        }
    }
    bool changed = group != schedule->group;
    if (changed) {
        selection->preempted = session_group_active && (group < schedule->group || group == AM_GROUP_ATTENTION);
        schedule->current_key[0] = 0;
        schedule->page_since_us = now;
    }
    selection->page = group == AM_GROUP_CONNECT ? AM_PAGE_CONNECT : group == AM_GROUP_IDLE ? AM_PAGE_IDLE :
                      group == AM_GROUP_RESULT ? AM_PAGE_RESULT : AM_PAGE_TASK;
    if (group == AM_GROUP_IDLE && model->usage_supported) {
        select_usage(model, selection->usage);
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
    schedule->group = group;
    schedule->attention_sequence = model->attention_sequence;
    if (now < model->info_until_us) {
        selection->page = AM_PAGE_INFO;
    }
}
