#include "snake.h"

/* xorshift32: small, fast, deterministic PRNG. Never valid at state 0 (it
   would stay stuck at 0 forever), so Snake_Init remaps a zero seed. */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void place_food(SnakeGame *g)
{
    if (g->length >= SNAKE_MAX_LENGTH) {
        return;
    }

    for (;;) {
        uint32_t r = xorshift32(&g->seed);
        uint8_t x = (uint8_t)(r % SNAKE_GRID_W);
        uint8_t y = (uint8_t)((r / SNAKE_GRID_W) % SNAKE_GRID_H);
        if (!Snake_CellOccupied(g, x, y)) {
            g->food.x = x;
            g->food.y = y;
            return;
        }
    }
}

void Snake_Init(SnakeGame *g, uint32_t seed)
{
    g->seed = seed ? seed : 0x9E3779B9u;

    g->length = 3;
    g->score = 0;
    g->state = SNAKE_STATE_RUNNING;
    g->heading = SNAKE_DIR_RIGHT;
    g->pending_dir = SNAKE_DIR_RIGHT;

    g->body[0].x = SNAKE_GRID_W / 2;
    g->body[0].y = SNAKE_GRID_H / 2;
    g->body[1].x = (uint8_t)(g->body[0].x - 1);
    g->body[1].y = g->body[0].y;
    g->body[2].x = (uint8_t)(g->body[0].x - 2);
    g->body[2].y = g->body[0].y;

    place_food(g);
}

void Snake_SetDirection(SnakeGame *g, SnakeDirection dir)
{
    if (dir == (SnakeDirection)((g->heading + 2) % 4)) {
        return;
    }
    g->pending_dir = dir;
}

void Snake_Step(SnakeGame *g)
{
    if (g->state != SNAKE_STATE_RUNNING) {
        return;
    }

    g->heading = g->pending_dir;

    int8_t dx = 0, dy = 0;
    switch (g->heading) {
        case SNAKE_DIR_RIGHT: dx = 1;  dy = 0;  break;
        case SNAKE_DIR_DOWN:  dx = 0;  dy = 1;  break;
        case SNAKE_DIR_LEFT:  dx = -1; dy = 0;  break;
        case SNAKE_DIR_UP:    dx = 0;  dy = -1; break;
    }

    int16_t new_x = (int16_t)g->body[0].x + dx;
    int16_t new_y = (int16_t)g->body[0].y + dy;

    if (new_x < 0 || new_x >= SNAKE_GRID_W || new_y < 0 || new_y >= SNAKE_GRID_H) {
        g->state = SNAKE_STATE_GAME_OVER;
        return;
    }

    SnakeCell new_head = { (uint8_t)new_x, (uint8_t)new_y };
    bool eating = (new_head.x == g->food.x && new_head.y == g->food.y);

    uint16_t upper = eating ? g->length : (uint16_t)(g->length - 1);

    for (uint16_t i = 1; i < upper; i++) {
        if (g->body[i].x == new_head.x && g->body[i].y == new_head.y) {
            g->state = SNAKE_STATE_GAME_OVER;
            return;
        }
    }

    for (uint16_t i = upper; i >= 1; i--) {
        g->body[i] = g->body[i - 1];
    }
    g->body[0] = new_head;

    if (eating) {
        g->length++;
        g->score++;
        if (g->length == SNAKE_MAX_LENGTH) {
            g->state = SNAKE_STATE_WON;
        } else {
            place_food(g);
        }
    }
}

bool Snake_CellOccupied(const SnakeGame *g, uint8_t x, uint8_t y)
{
    for (uint16_t i = 0; i < g->length; i++) {
        if (g->body[i].x == x && g->body[i].y == y) {
            return true;
        }
    }
    return false;
}
