#include "snake_render.h"

#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include "font8x8.h"

/* Logical (landscape) canvas size the game draws to; the LTDC framebuffer
   behind `display_dev` is portrait (240 wide x 320 tall), so every logical
   rect/glyph is rotated 90 degrees before being written. See
   SNAKE_RENDER_ROTATE_CW below. */
#define SNAKE_LOGICAL_W  320
#define SNAKE_LOGICAL_H  240

/* Which way the logical landscape canvas is rotated into the portrait
   framebuffer. Flip this single constant (1 <-> 0) if the picture comes
   out upside down or mirrored on hardware; do not touch tilt.h. */
#define SNAKE_RENDER_ROTATE_CW  1

/* RGB565 colours actually used by this renderer. */
#define SNAKE_BG_COLOR    0x0000u /* black */
#define SNAKE_BODY_COLOR  0x07E0u /* green */
#define SNAKE_FOOD_COLOR  0xFFE0u /* yellow */
#define SNAKE_HUD_FG      0xFFFFu /* white */
#define SNAKE_HUD_BG      SNAKE_BG_COLOR
#define SNAKE_END_FG      0xFFFFu /* white */
#define SNAKE_END_BG      SNAKE_BG_COLOR

static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* Widest single-row fill ever issued: a full-screen clear rotates to a
   240-wide portrait fill (SNAKE_LOGICAL_H). Bounded static storage only. */
static uint16_t row_buf[SNAKE_LOGICAL_H];

/* Maps a logical landscape rect (lx, ly, w, h) to the portrait framebuffer
   rect (px, py, pw, ph) and fills it a row at a time. */
static void fill_rect_logical(uint16_t lx, uint16_t ly, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t px, py, pw, ph;

#if SNAKE_RENDER_ROTATE_CW
    px = ly;
    py = (uint16_t)(SNAKE_LOGICAL_W - lx - w);
    pw = h;
    ph = w;
#else
    px = (uint16_t)(SNAKE_LOGICAL_H - ly - h);
    py = lx;
    pw = h;
    ph = w;
#endif

    for (uint16_t i = 0; i < pw; i++) {
        row_buf[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = (size_t)pw * 2u,
        .width = pw,
        .height = 1,
        .pitch = pw,
    };

    for (uint16_t row = 0; row < ph; row++) {
        display_write(display_dev, px, (uint16_t)(py + row), &desc, row_buf);
    }
}

static void cell_to_px(uint8_t x, uint8_t y, uint16_t *lx, uint16_t *ly)
{
    *lx = (uint16_t)(SNAKE_ORIGIN_X + (uint16_t)x * SNAKE_CELL_PX);
    *ly = (uint16_t)(SNAKE_ORIGIN_Y + (uint16_t)y * SNAKE_CELL_PX);
}

static void draw_cell(uint8_t x, uint8_t y, uint16_t color)
{
    uint16_t lx, ly;
    cell_to_px(x, y, &lx, &ly);
    fill_rect_logical(lx, ly, SNAKE_CELL_PX, SNAKE_CELL_PX, color);
}

/* Rotates one 8x8 glyph the same direction as fill_rect_logical and writes
   it in a single display_write at the rect-rotated position of
   (gx, gy, 8, 8). */
static void draw_glyph_logical(uint16_t gx, uint16_t gy, char c, uint16_t fg, uint16_t bg)
{
    if (c < 0x20 || c > 0x7E) {
        c = '?';
    }
    const uint8_t *glyph = font8x8[c - 0x20];

    uint16_t glyph_buf[8][8];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint16_t color = (bits & (1 << col)) ? fg : bg;
#if SNAKE_RENDER_ROTATE_CW
            glyph_buf[7 - col][row] = color;
#else
            glyph_buf[col][7 - row] = color;
#endif
        }
    }

    uint16_t px, py;
#if SNAKE_RENDER_ROTATE_CW
    px = gy;
    py = (uint16_t)(SNAKE_LOGICAL_W - gx - 8);
#else
    px = (uint16_t)(SNAKE_LOGICAL_H - gy - 8);
    py = gx;
#endif

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(glyph_buf),
        .width = 8,
        .height = 8,
        .pitch = 8,
    };
    display_write(display_dev, px, py, &desc, glyph_buf);
}

static void draw_string_logical(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    while (*str) {
        draw_glyph_logical(x, y, *str++, fg, bg);
        x += 8;
        if (x + 8 > SNAKE_LOGICAL_W) {
            x = 0;
            y += 8;
        }
    }
}

static void draw_hud(const SnakeGame *g)
{
    char line[24];
    snprintf(line, sizeof(line), "SCORE: %u", (unsigned)g->score);
    fill_rect_logical(0, 0, SNAKE_LOGICAL_W, SNAKE_ORIGIN_Y, SNAKE_HUD_BG);
    draw_string_logical(4, 4, line, SNAKE_HUD_FG, SNAKE_HUD_BG);
}

void SnakeRender_DrawPlayfield(const SnakeGame *g)
{
    fill_rect_logical(0, 0, SNAKE_LOGICAL_W, SNAKE_LOGICAL_H, SNAKE_BG_COLOR);
    draw_hud(g);

    for (uint16_t i = 0; i < g->length; i++) {
        draw_cell(g->body[i].x, g->body[i].y, SNAKE_BODY_COLOR);
    }
    draw_cell(g->food.x, g->food.y, SNAKE_FOOD_COLOR);
}

void SnakeRender_UpdateStep(const SnakeGame *g, uint8_t old_tail_x, uint8_t old_tail_y, bool ate_food)
{
    draw_cell(g->body[0].x, g->body[0].y, SNAKE_BODY_COLOR);

    if (!ate_food) {
        draw_cell(old_tail_x, old_tail_y, SNAKE_BG_COLOR);
    } else {
        draw_cell(g->food.x, g->food.y, SNAKE_FOOD_COLOR);
        draw_hud(g);
    }
}

void SnakeRender_DrawEndScreen(const SnakeGame *g, const char *msg)
{
    char score_line[24];
    snprintf(score_line, sizeof(score_line), "SCORE: %u", (unsigned)g->score);

    uint16_t y = SNAKE_LOGICAL_H / 2 - 8;
    draw_string_logical(4, y, msg, SNAKE_END_FG, SNAKE_END_BG);
    draw_string_logical(4, (uint16_t)(y + 16), score_line, SNAKE_END_FG, SNAKE_END_BG);
}

void SnakeRender_DrawMessage(const char *msg)
{
    fill_rect_logical(0, 0, SNAKE_LOGICAL_W, SNAKE_LOGICAL_H, SNAKE_BG_COLOR);

    uint16_t y = SNAKE_LOGICAL_H / 2 - 8;
    draw_string_logical(4, y, msg, SNAKE_END_FG, SNAKE_END_BG);
}
