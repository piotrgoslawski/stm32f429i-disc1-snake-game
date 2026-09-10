#ifndef SNAKE_H
#define SNAKE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SNAKE_GRID_W 32
#define SNAKE_GRID_H 22
#define SNAKE_MAX_LENGTH (SNAKE_GRID_W * SNAKE_GRID_H)

typedef enum {
    SNAKE_DIR_RIGHT = 0,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_UP,
} SnakeDirection;

typedef enum {
    SNAKE_STATE_RUNNING = 0,
    SNAKE_STATE_GAME_OVER,
    SNAKE_STATE_WON,
} SnakeState;

typedef struct {
    uint8_t x, y;
} SnakeCell;

typedef struct {
    SnakeCell body[SNAKE_MAX_LENGTH]; /* body[0] = head */
    uint16_t length;
    SnakeCell food;
    uint16_t score;
    SnakeState state;
    SnakeDirection heading;     /* current committed direction */
    SnakeDirection pending_dir; /* next direction, applied at next Step */
    uint32_t seed;              /* PRNG state, advances on each food placement */
} SnakeGame;

/* Resets the game to its starting state: length 3, centred head heading
   right, score 0, state RUNNING, and food placed via the PRNG seeded from
   `seed` (0 is remapped to a fixed non-zero value). */
void Snake_Init(SnakeGame *g, uint32_t seed);

/* Records the direction the snake should turn to on the next Snake_Step.
   A 180-degree reversal relative to the current heading is ignored. */
void Snake_SetDirection(SnakeGame *g, SnakeDirection dir);

/* Advances the game by one tick. No-op unless state is RUNNING. */
void Snake_Step(SnakeGame *g);

/* Returns true if (x, y) is currently covered by the snake's body. */
bool Snake_CellOccupied(const SnakeGame *g, uint8_t x, uint8_t y);

#ifdef __cplusplus
}
#endif

#endif /* SNAKE_H */
