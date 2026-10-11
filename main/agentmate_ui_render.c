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
# Description: AgentMate bounded RGB565 scene renderer
# Author: Liu Chuansen <liuchuansen@outlook.com>
#
#####################################################################################
#
# Revision History:
# 2026-10-12 1.5      Codex         Render cached antialiased curved headers and outlines.
# 2026-10-11 1.4      Codex         Add Session panels, black background and mapped status borders.
#
# Date       Version  Author       Description
# ---------- -------- ------------ -----------------------------------------------
# 2026-10-06 1.3      Codex         Render quota progress tracks with bounded updates.
# 2026-10-03 1.2      Codex         Composite animated borders and submit unchanged bounds once.
# 2026-10-03 1.1      Codex         Render two directory sizes from cropped shared glyphs.
# 2026-10-02 1.0      Codex         Created phase-one implementation.
#
#####################################################################################
*/

#include "agentmate_ui_render.h"
#include "agentmate_text.h"

#include <string.h>
#include <math.h>

uint16_t g_am_ui_pixels[240 * 32];

static const struct am_glyph *find_glyph(const struct am_font *font, uint32_t code)
{
    int low = 0;
    int high = font->count;
    while (low < high) {
        int mid = (low + high) / 2;
        if (font->glyphs[mid].code < code) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (low < font->count && font->glyphs[low].code == code) {
        return &font->glyphs[low];
    }
    return code == '?' ? NULL : find_glyph(font, '?');
}

static int font_scale(const struct am_font *font, int value)
{
    return (value * font->scale_num + font->scale_den / 2) / font->scale_den;
}

int am_text_width(const struct am_font *font, const char *text)
{
    int width = 0;
    while (*text) {
        const struct am_glyph *glyph = find_glyph(font, am_utf8_next(&text));
        if (glyph) {
            width += font_scale(font, glyph->advance);
        }
    }
    return width;
}

int am_text_ink_width(const struct am_font *font, const char *text)
{
    int width = 0, cursor = 0;
    while (*text) {
        const struct am_glyph *glyph = find_glyph(font, am_utf8_next(&text));
        if (glyph) {
            int right = cursor + font_scale(font, glyph->width);
            if (right > width) {
                width = right;
            }
            cursor += font_scale(font, glyph->advance);
        }
    }
    return width;
}

static struct am_ui_op *new_op(struct am_ui_scene *scene, enum am_op_kind kind, int x, int y, int width, int height, uint16_t color)
{
    if (scene->count == AM_UI_OPS || width <= 0 || height <= 0) {
        return NULL;
    }
    struct am_ui_op *op = &scene->ops[scene->count++];
    memset(op, 0, sizeof(*op));
    op->kind = kind;
    op->x = x;
    op->y = y;
    op->width = width;
    op->height = height;
    op->color = color;
    return op;
}

void am_scene_text(struct am_ui_scene *scene, int x, int y, int width, const struct am_font *font, uint16_t color,
                   const char *text, int alignment)
{
    char clipped[96];
    size_t length = strlen(text);
    if (length >= sizeof(clipped)) {
        length = sizeof(clipped) - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) {
            length--;
        }
    }
    memcpy(clipped, text, length);
    clipped[length] = 0;
    if (am_text_width(font, clipped) > width) {
        const char *p = clipped;
        size_t keep = 0;
        int used = am_text_width(font, "…");
        while (*p) {
            const struct am_glyph *g = find_glyph(font, am_utf8_next(&p));
            if (!g || used + font_scale(font, g->advance) > width || (size_t)(p - clipped) > sizeof(clipped) - 4) {
                break;
            }
            used += font_scale(font, g->advance);
            keep = (size_t)(p - clipped);
        }
        memcpy(clipped + keep, "…", 4);
    }
    int measured = am_text_width(font, clipped);
    if (alignment == 0) {
        x += (width - measured) / 2;
    } else if (alignment > 0) {
        x += width - measured;
    }
    int ink_width = am_text_ink_width(font, clipped);
    struct am_ui_op *op = new_op(scene, AM_OP_TEXT, x, y, ink_width, font->height, color);
    if (op) {
        op->font = font;
        memcpy(op->text, clipped, strlen(clipped) + 1);
    }
}

void am_scene_rect(struct am_ui_scene *scene, int x, int y, int width, int height, uint16_t color, int radius)
{
    struct am_ui_op *op = new_op(scene, AM_OP_RECT, x, y, width, height, color);
    if (op) {
        op->radius = radius;
    }
}

void am_scene_cap(struct am_ui_scene *scene, int height, int radius_x, int radius_y, uint16_t color, int border_width)
{
    struct am_ui_op *op = new_op(scene, AM_OP_CAP, 0, 0, 240, height, color);
    if (op) {
        op->radius = radius_x;
        op->split_y = radius_y;
        op->border_width = border_width;
    }
}

void am_scene_disc(struct am_ui_scene *scene, int x, int y, int radius, uint16_t color)
{
    new_op(scene, AM_OP_DISC, x - radius, y - radius, radius * 2 + 1, radius * 2 + 1, color);
}

void am_scene_panel(struct am_ui_scene *scene, int x, int y, int width, int height, int split_y, uint16_t upper, uint16_t lower)
{
    struct am_ui_op *op = new_op(scene, AM_OP_PANEL, x, y, width, height, upper);
    if (op) {
        op->radius = 9;
        op->split_y = split_y;
        op->track_color = lower;
    }
}

void am_scene_progress(struct am_ui_scene *scene, int x, int y, int width, int height, uint16_t color, uint16_t track_color,
                       unsigned remaining_bp)
{
    struct am_ui_op *op = new_op(scene, AM_OP_PROGRESS, x, y, width, height, color);
    if (op) {
        op->radius = height / 2 < width / 2 ? height / 2 : width / 2;
        op->track_color = track_color;
        op->remaining_bp = remaining_bp > 10000 ? 10000 : remaining_bp;
    }
}

void am_scene_mask(struct am_ui_scene *scene, int x, int y, const struct am_image *image, uint16_t color)
{
    struct am_ui_op *op = new_op(scene, AM_OP_MASK, x, y, image->width, image->height, color);
    if (op) {
        op->image = image;
    }
}

void am_scene_bitmap(struct am_ui_scene *scene, int x, int y, int width, int height, const uint16_t *bitmap)
{
    struct am_ui_op *op = new_op(scene, AM_OP_BITMAP, x, y, width, height, 0);
    if (op) {
        op->bitmap = bitmap;
    }
}

void am_scene_status(struct am_ui_scene *scene, const struct am_status_visual *visual)
{
    struct am_ui_op *op = new_op(scene, AM_OP_STATUS_FRAME, 77, 86, 145, 56, visual->color);
    if (op) {
        op->status_kind = visual->kind;
        op->animation_frame = visual->frame;
    }
}

static uint16_t blend(uint16_t bg, uint16_t fg, unsigned alpha)
{
    unsigned r = (((bg >> 11) & 31) * (255 - alpha) + ((fg >> 11) & 31) * alpha + 127) / 255;
    unsigned g = (((bg >> 5) & 63) * (255 - alpha) + ((fg >> 5) & 63) * alpha + 127) / 255;
    unsigned b = ((bg & 31) * (255 - alpha) + (fg & 31) * alpha + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void paint_pixel(int x, int y, int bx, int by, int width, int height, uint16_t color, unsigned alpha)
{
    if (x >= bx && x < bx + width && y >= by && y < by + height) {
        size_t index = (size_t)(y - by) * width + x - bx;
        g_am_ui_pixels[index] = blend(g_am_ui_pixels[index], color, alpha);
    }
}

static unsigned glyph_sample(const struct am_font *font, const struct am_glyph *glyph, int x, int y)
{
    x = x < 0 ? 0 : x >= glyph->width ? glyph->width - 1 : x;
    y = y < 0 ? 0 : y >= glyph->height ? glyph->height - 1 : y;
    size_t index = (size_t)y * glyph->width + x;
    uint8_t packed = font->pixels[glyph->offset + index / 2];
    return index % 2 ? packed & 15 : packed >> 4;
}

static unsigned glyph_mask(const struct am_font *font, const struct am_glyph *glyph, int x, int y)
{
    if (font->scale_num == font->scale_den) {
        return glyph_sample(font, glyph, x, y);
    }
    /* Bilinear sampling of the native 14 px masks keeps the 12 px fallback smooth. */
    int sx = ((2 * x + 1) * font->scale_den - font->scale_num) * 128 / font->scale_num;
    int sy = ((2 * y + 1) * font->scale_den - font->scale_num) * 128 / font->scale_num;
    sx = sx < 0 ? 0 : sx;
    sy = sy < 0 ? 0 : sy;
    int ix = sx / 256, iy = sy / 256;
    unsigned fx = sx % 256, fy = sy % 256;
    unsigned upper = glyph_sample(font, glyph, ix, iy) * (256 - fx) + glyph_sample(font, glyph, ix + 1, iy) * fx;
    unsigned lower = glyph_sample(font, glyph, ix, iy + 1) * (256 - fx) + glyph_sample(font, glyph, ix + 1, iy + 1) * fx;
    return (upper * (256 - fy) + lower * fy + 32768) / 65536;
}

/* Two compact boundary caches cover the business cap and Session outline.
 * Reuse the geometry across frames; no full-screen bitmap or per-pixel square roots.
 */
struct cap_edges {
    int height, radius_x, radius_y, border;
    uint16_t outer[240], inner[240];
};

static uint16_t cap_edge(const struct am_ui_op *op, int x, int inset)
{
    float rx = op->radius - inset;
    float ry = op->split_y - inset;
    float at = x + 0.5f;
    float dx = at < op->radius ? op->radius - at : at > 240 - op->radius ? at - (240 - op->radius) : 0;
    if (rx <= 0 || ry <= 0 || at < inset || at > 240 - inset || dx > rx) {
        return 0;
    }
    float edge = op->height - op->split_y + ry * sqrtf(1.0f - dx * dx / (rx * rx));
    return (uint16_t)(edge * 256.0f + 0.5f);
}

static const struct cap_edges *cap_geometry(const struct am_ui_op *op)
{
    static struct cap_edges s_edges[2];
    struct cap_edges *edges = &s_edges[op->border_width ? 1 : 0];
    if (edges->height != op->height || edges->radius_x != op->radius ||
        edges->radius_y != op->split_y || edges->border != op->border_width) {
        edges->height = op->height;
        edges->radius_x = op->radius;
        edges->radius_y = op->split_y;
        edges->border = op->border_width;
        for (int x = 0; x < 240; ++x) {
            edges->outer[x] = cap_edge(op, x, 0);
            edges->inner[x] = op->border_width ? cap_edge(op, x, op->border_width) : 0;
        }
    }
    return edges;
}

static unsigned cap_coverage(uint16_t edge, int y)
{
    int coverage = edge - y * 256;
    return coverage <= 0 ? 0 : coverage >= 256 ? 255 : (unsigned)coverage * 255 / 256;
}

static void paint_op(const struct am_ui_op *op, int bx, int by, int width, int height, unsigned opacity)
{
    if (op->x >= bx + width || op->x + op->width <= bx || op->y >= by + height || op->y + op->height <= by) {
        return;
    }
    if (op->kind == AM_OP_TEXT) {
        const char *p = op->text;
        int gx = op->x;
        while (*p) {
            const struct am_glyph *g = find_glyph(op->font, am_utf8_next(&p));
            if (!g) {
                continue;
            }
            int glyph_y = op->y + font_scale(op->font, g->top);
            int scaled_height = font_scale(op->font, g->height);
            int scaled_width = font_scale(op->font, g->width);
            for (int y = 0; y < scaled_height; ++y) {
                if (glyph_y + y < by || glyph_y + y >= by + height) {
                    continue;
                }
                for (int x = 0; x < scaled_width; ++x) {
                    unsigned mask = glyph_mask(op->font, g, x, y);
                    if (mask) {
                        paint_pixel(gx + x, glyph_y + y, bx, by, width, height, op->color, mask * 17 * opacity / 255);
                    }
                }
            }
            gx += font_scale(op->font, g->advance);
        }
        return;
    }
    int start_y = op->y > by ? op->y : by;
    int end_y = op->y + op->height < by + height ? op->y + op->height : by + height;
    int start_x = op->x > bx ? op->x : bx;
    int end_x = op->x + op->width < bx + width ? op->x + op->width : bx + width;
    const struct cap_edges *edges = op->kind == AM_OP_CAP ? cap_geometry(op) : NULL;
    int filled_width = op->kind == AM_OP_PROGRESS ? (op->width * op->remaining_bp + 5000) / 10000 : 0;
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            unsigned alpha = opacity;
            uint16_t color = op->color;
            int dx = x - op->x;
            int dy = y - op->y;
            if (edges) {
                unsigned coverage = cap_coverage(edges->outer[dx], dy);
                if (op->border_width) {
                    coverage -= cap_coverage(edges->inner[dx], dy);
                }
                alpha = coverage * opacity / 255;
            } else if (op->kind == AM_OP_STATUS_FRAME) {
                alpha = am_status_border_alpha(op->status_kind, op->animation_frame,
                                               dx * AM_STATUS_WIDTH / op->width, dy * AM_STATUS_HEIGHT / op->height) * opacity / 255;
            } else if (op->kind == AM_OP_DISC) {
                int radius = op->width / 2;
                int cx = dx - radius;
                int cy = dy - radius;
                if (cx * cx + cy * cy > radius * radius) {
                    continue;
                }
            } else if ((op->kind == AM_OP_RECT || op->kind == AM_OP_PROGRESS || op->kind == AM_OP_PANEL) && op->radius) {
                int r = op->radius;
                int cx = dx < r ? r - dx : dx >= op->width - r ? dx - (op->width - r - 1) : 0;
                int cy = dy < r ? r - dy : dy >= op->height - r ? dy - (op->height - r - 1) : 0;
                if (cx * cx + cy * cy > r * r) {
                    continue;
                }
            } else if (op->kind == AM_OP_MASK) {
                size_t index = (size_t)dy * op->width + dx;
                uint8_t packed = op->image->mask[index / 2];
                alpha = (index % 2 ? packed & 15 : packed >> 4) * 17 * opacity / 255;
            } else if (op->kind == AM_OP_BITMAP) {
                color = op->bitmap[dy * op->width + dx];
            }
            if (op->kind == AM_OP_PROGRESS) {
                color = dx < filled_width ? op->color : op->track_color;
            } else if (op->kind == AM_OP_PANEL) {
                color = dy < op->split_y ? op->color : op->track_color;
            }
            paint_pixel(x, y, bx, by, width, height, color, alpha);
        }
    }
}

static esp_err_t render_rect(am_app_t *app, const struct am_ui_scene *scene, int x, int y, int width, int height)
{
    int end_x = x + width < 240 ? x + width : 240;
    int end_y = y + height < 240 ? y + height : 240;
    x = x < 0 ? 0 : x;
    y = y < 0 ? 0 : y;
    width = end_x - x;
    if (width <= 0 || end_y <= y) {
        return ESP_OK;
    }
    for (int row = y; row < end_y; row += 32) {
        int rows = end_y - row < 32 ? end_y - row : 32;
        for (int i = 0; i < width * rows; ++i) {
            g_am_ui_pixels[i] = AM_UI_BG;
        }
        for (int i = 0; i < scene->count; ++i) {
            const struct am_ui_op *op = &scene->ops[i];
            paint_op(op, x, row, width, rows, op->y < 39 ? 255 : scene->body_alpha);
        }
        esp_err_t err = tft_display_draw_bitmap_rgb565(app->display, x, row, width, rows, g_am_ui_pixels);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t am_scene_render(am_app_t *app, struct am_ui_scene *previous, const struct am_ui_scene *next, bool clear)
{
    esp_err_t err;
    if (clear || !previous || previous->session_background != next->session_background) {
        err = render_rect(app, next, 0, 0, 240, 240);
        if (err != ESP_OK) {
            return err;
        }
    } else if (previous->body_alpha != next->body_alpha) {
        if (next->session_background || previous->count < 2 || next->count < 2 || memcmp(previous->ops, next->ops, sizeof(next->ops[0]) * 2) != 0) {
            err = render_rect(app, next, 0, 0, 240, 39);
            if (err != ESP_OK) {
                return err;
            }
        }
        err = render_rect(app, next, 0, 39, 240, 201);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        int count = previous->count > next->count ? previous->count : next->count;
        for (int i = 0; i < count; ++i) {
            const struct am_ui_op *old = i < previous->count ? &previous->ops[i] : NULL;
            const struct am_ui_op *op = i < next->count ? &next->ops[i] : NULL;
            if (old && op && memcmp(old, op, sizeof(*op)) == 0) {
                continue;
            }
            bool same_bounds = old && op && old->x == op->x && old->y == op->y && old->width == op->width && old->height == op->height;
            if (old && !same_bounds) {
                err = render_rect(app, next, old->x, old->y, old->width, old->height);
                if (err != ESP_OK) {
                    return err;
                }
            }
            if (op) {
                err = render_rect(app, next, op->x, op->y, op->width, op->height);
                if (err != ESP_OK) {
                    return err;
                }
            }
        }
    }
    if (previous) {
        *previous = *next;
    }
    return ESP_OK;
}
