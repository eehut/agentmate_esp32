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
# 2026-10-03 1.1      Codex         Enable standby quota display only on successful publication.
# 2026-10-03 1.0      Codex         Created atomic account usage updates.
#
#####################################################################################
*/

#include "agentmate_usage.h"

#include <stdio.h>
#include <string.h>

static void publish(am_app_t *app)
{
    int count = 0;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        const am_usage_t *entry = &app->model.usages[i];
        count += entry->used;
        if (entry->used && strcmp(entry->provider, "codex") == 0 && (entry->window_seconds == 18000 || entry->window_seconds == 604800)) {
            app->model.usage_supported = true;
        }
    }
    app->model.usage_count = count;
    app->model.revision++;
}

enum am_usage_error am_usage_begin(am_app_t *app, const char *provider, uint64_t id, int64_t now)
{
    struct am_usage_batch *batch = &app->usage_batch;
    if (batch->active) {
        return !batch->failed && batch->id == id && strcmp(batch->provider, provider) == 0 ? AM_USAGE_OK : AM_USAGE_BAD_STATE;
    }
    if (!id || id <= batch->last_id) {
        return AM_USAGE_BAD_STATE;
    }
    memcpy(batch->entries, app->model.usages, sizeof(batch->entries));
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        if (batch->entries[i].used && strcmp(batch->entries[i].provider, provider) == 0) {
            memset(&batch->entries[i], 0, sizeof(batch->entries[i]));
        }
    }
    batch->active = true;
    batch->failed = false;
    batch->id = id;
    batch->started_us = now;
    snprintf(batch->provider, sizeof(batch->provider), "%s", provider);
    return AM_USAGE_OK;
}

enum am_usage_error am_usage_write(am_app_t *app, const am_usage_t *entry)
{
    struct am_usage_batch *batch = &app->usage_batch;
    if (batch->failed || (batch->active && strcmp(batch->provider, entry->provider) != 0)) {
        batch->failed = true;
        return AM_USAGE_BAD_STATE;
    }
    am_usage_t *entries = batch->active ? batch->entries : app->model.usages;
    int free_slot = -1;
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        if (!entries[i].used) {
            if (free_slot < 0) {
                free_slot = i;
            }
        } else if (strcmp(entries[i].provider, entry->provider) == 0 && strcmp(entries[i].metric, entry->metric) == 0 &&
                   strcmp(entries[i].window, entry->window) == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        batch->failed = batch->active;
        return AM_USAGE_CAPACITY;
    }
    entries[free_slot] = *entry;
    if (!batch->active) {
        publish(app);
    }
    return AM_USAGE_OK;
}

enum am_usage_error am_usage_end(am_app_t *app, const char *provider, uint64_t id)
{
    struct am_usage_batch *batch = &app->usage_batch;
    if (!batch->active) {
        return !batch->failed && id == batch->last_id && id && strcmp(provider, batch->last_provider) == 0 ? AM_USAGE_OK : AM_USAGE_BAD_STATE;
    }
    if (id != batch->id || strcmp(provider, batch->provider) != 0) {
        return AM_USAGE_BAD_STATE;
    }
    batch->active = false;
    if (batch->failed) {
        memset(batch->entries, 0, sizeof(batch->entries));
        return AM_USAGE_BAD_STATE;
    }
    memcpy(app->model.usages, batch->entries, sizeof(batch->entries));
    batch->last_id = id;
    snprintf(batch->last_provider, sizeof(batch->last_provider), "%s", provider);
    memset(batch->entries, 0, sizeof(batch->entries));
    publish(app);
    return AM_USAGE_OK;
}

void am_usage_clear(am_app_t *app, const char *provider)
{
    app->usage_batch.active = false;
    app->usage_batch.failed = false;
    memset(app->usage_batch.entries, 0, sizeof(app->usage_batch.entries));
    for (int i = 0; i < AM_MAX_USAGE_ENTRIES; ++i) {
        if (app->model.usages[i].used && strcmp(app->model.usages[i].provider, provider) == 0) {
            memset(&app->model.usages[i], 0, sizeof(app->model.usages[i]));
        }
    }
    publish(app);
}

void am_usage_expire(am_app_t *app, int64_t now)
{
    if (app->usage_batch.active && now - app->usage_batch.started_us >= 30000000) {
        app->usage_batch.active = false;
        app->usage_batch.failed = true;
        memset(app->usage_batch.entries, 0, sizeof(app->usage_batch.entries));
    }
}
