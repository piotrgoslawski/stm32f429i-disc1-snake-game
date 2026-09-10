#include "snake_render.h"

#include <stdio.h>

#include "ili9341.h"

#define SNAKE_BG_COLOR    ILI9341_BLACK
#define SNAKE_BODY_COLOR  ILI9341_GREEN
#define SNAKE_FOOD_COLOR  ILI9341_YELLOW
#define SNAKE_HUD_FG      ILI9341_WHITE
#define SNAKE_HUD_BG      ILI9341_BLACK
#define SNAKE_END_FG      ILI9341_WHITE
#define SNAKE_END_BG      ILI9341_BLACK

static void cell_to_px(uint8_t x, uint8_t y, uint16_t *px, uint16_t *py)
{
    *px = (uint16_t)(SNAKE_ORIGIN_X + (uint16_t)x * SNAKE_CELL_PX);
    *py = (uint16_t)(SNAKE_ORIGIN_Y + (uint16_t)y * SNAKE_CELL_PX);
}

static void draw_cell(uint8_t x, uint8_t y, uint16_t color)
{
    uint16_t px, py;
    cell_to_px(x, y, &px, &py);
    ILI9341_FillRect(px, py, SNAKE_CELL_PX, SNAKE_CELL_PX, color);
}

static void draw_hud(const SnakeGame *g)
{
    char line[24];
    snprintf(line, sizeof(line), "SCORE: %u", (unsigned)g->score);
    ILI9341_FillRect(0, 0, ILI9341_WIDTH, SNAKE_ORIGIN_Y, SNAKE_HUD_BG);
    ILI9341_DrawString(4, 4, line, SNAKE_HUD_FG, SNAKE_HUD_BG);
}

void SnakeRender_DrawPlayfield(const SnakeGame *g)
{
    ILI9341_FillScreen(SNAKE_BG_COLOR);
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

    uint16_t y = ILI9341_HEIGHT / 2 - 8;
    ILI9341_DrawString(4, y, msg, SNAKE_END_FG, SNAKE_END_BG);
    ILI9341_DrawString(4, (uint16_t)(y + 16), score_line, SNAKE_END_FG, SNAKE_END_BG);
}
