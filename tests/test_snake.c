#include "snake.h"
#include "test_util.h"

static void test_init_defaults(void)
{
    SnakeGame g;
    Snake_Init(&g, 1);

    bool ok = (g.length == 3)
        && (g.body[0].x == SNAKE_GRID_W / 2 && g.body[0].y == SNAKE_GRID_H / 2)
        && (g.body[1].x == g.body[0].x - 1 && g.body[1].y == g.body[0].y)
        && (g.body[2].x == g.body[0].x - 2 && g.body[2].y == g.body[0].y)
        && (g.heading == SNAKE_DIR_RIGHT)
        && (g.pending_dir == SNAKE_DIR_RIGHT)
        && (g.score == 0)
        && (g.state == SNAKE_STATE_RUNNING)
        && (g.food.x < SNAKE_GRID_W && g.food.y < SNAKE_GRID_H)
        && (!Snake_CellOccupied(&g, g.food.x, g.food.y));
    CHECK(ok);
}

static void test_occupancy(void)
{
    SnakeGame g;
    Snake_Init(&g, 2);

    bool ok = Snake_CellOccupied(&g, g.body[0].x, g.body[0].y)
        && Snake_CellOccupied(&g, g.body[1].x, g.body[1].y)
        && Snake_CellOccupied(&g, g.body[2].x, g.body[2].y)
        && !Snake_CellOccupied(&g, 0, 0)
        && !Snake_CellOccupied(&g, SNAKE_GRID_W - 1, SNAKE_GRID_H - 1);
    CHECK(ok);
}

static void test_food_off_snake_and_in_bounds(void)
{
    SnakeGame g;
    Snake_Init(&g, 3);

    bool ok = (g.food.x < SNAKE_GRID_W)
        && (g.food.y < SNAKE_GRID_H)
        && (!Snake_CellOccupied(&g, g.food.x, g.food.y));
    CHECK(ok);
}

static void test_food_is_deterministic(void)
{
    SnakeGame g1, g2;
    Snake_Init(&g1, 777);
    Snake_Init(&g2, 777);
    bool ok = (g1.food.x == g2.food.x && g1.food.y == g2.food.y);

    /* Force both games through an identical eat and confirm the next food
       placement matches too (same seed, same history -> same food). */
    g1.food.x = (uint8_t)(g1.body[0].x + 1);
    g1.food.y = g1.body[0].y;
    g2.food.x = (uint8_t)(g2.body[0].x + 1);
    g2.food.y = g2.body[0].y;

    Snake_Step(&g1);
    Snake_Step(&g2);

    ok = ok
        && (g1.state == SNAKE_STATE_RUNNING)
        && (g1.length == 4 && g2.length == 4)
        && (g1.food.x == g2.food.x && g1.food.y == g2.food.y);
    CHECK(ok);
}

static void test_set_direction_buffers(void)
{
    SnakeGame g;
    Snake_Init(&g, 4);

    Snake_SetDirection(&g, SNAKE_DIR_DOWN);
    bool ok = (g.heading == SNAKE_DIR_RIGHT) && (g.pending_dir == SNAKE_DIR_DOWN);

    g.food.x = 0;
    g.food.y = SNAKE_GRID_H - 1;
    Snake_Step(&g);
    ok = ok && (g.heading == SNAKE_DIR_DOWN);
    CHECK(ok);
}

static void test_reversal_is_rejected(void)
{
    SnakeGame g;
    Snake_Init(&g, 5);

    Snake_SetDirection(&g, SNAKE_DIR_LEFT);
    CHECK(g.pending_dir == SNAKE_DIR_RIGHT);
}

static void test_step_moves_forward(void)
{
    SnakeGame g;
    Snake_Init(&g, 6);

    g.food.x = 0;
    g.food.y = 0;
    SnakeCell old_head = g.body[0];
    uint16_t old_length = g.length;

    Snake_Step(&g);

    bool ok = (g.body[0].x == old_head.x + 1)
        && (g.body[0].y == old_head.y)
        && (g.length == old_length)
        && (g.state == SNAKE_STATE_RUNNING);
    CHECK(ok);
}

static void test_step_tail_follows_two_steps(void)
{
    SnakeGame g;
    Snake_Init(&g, 7);

    g.food.x = 0;
    g.food.y = 0;
    SnakeCell orig_head = g.body[0];

    Snake_Step(&g);
    Snake_Step(&g);

    bool ok = (g.length == 3)
        && (g.body[2].x == orig_head.x && g.body[2].y == orig_head.y);
    CHECK(ok);
}

static void test_wall_collision_right(void)
{
    SnakeGame g;
    Snake_Init(&g, 8);

    g.food.x = 0;
    g.food.y = 0;
    g.body[0].x = SNAKE_GRID_W - 1;
    g.body[0].y = 5;
    g.body[1].x = SNAKE_GRID_W - 2;
    g.body[1].y = 5;
    g.body[2].x = SNAKE_GRID_W - 3;
    g.body[2].y = 5;
    g.heading = SNAKE_DIR_RIGHT;
    g.pending_dir = SNAKE_DIR_RIGHT;

    Snake_Step(&g);

    CHECK(g.state == SNAKE_STATE_GAME_OVER);
}

static void test_wall_collision_top(void)
{
    SnakeGame g;
    Snake_Init(&g, 9);

    g.food.x = 0;
    g.food.y = SNAKE_GRID_H - 1;
    g.body[0].x = 5;
    g.body[0].y = 0;
    g.body[1].x = 5;
    g.body[1].y = 1;
    g.body[2].x = 5;
    g.body[2].y = 2;
    g.heading = SNAKE_DIR_UP;
    g.pending_dir = SNAKE_DIR_UP;

    Snake_Step(&g);

    CHECK(g.state == SNAKE_STATE_GAME_OVER);
}

static void test_self_collision(void)
{
    SnakeGame g;
    Snake_Init(&g, 10);

    g.length = 5;
    g.body[0].x = 5;
    g.body[0].y = 5;
    g.body[1].x = 5;
    g.body[1].y = 6;
    g.body[2].x = 6;
    g.body[2].y = 5;
    g.body[3].x = 6;
    g.body[3].y = 6;
    g.body[4].x = 5;
    g.body[4].y = 7;
    g.food.x = 0;
    g.food.y = 0;
    g.heading = SNAKE_DIR_RIGHT;
    g.pending_dir = SNAKE_DIR_RIGHT;

    Snake_Step(&g);

    CHECK(g.state == SNAKE_STATE_GAME_OVER);
}

static void test_move_into_own_tail_is_allowed(void)
{
    SnakeGame g;
    Snake_Init(&g, 11);

    g.length = 4;
    g.body[0].x = 2;
    g.body[0].y = 1;
    g.body[1].x = 2;
    g.body[1].y = 2;
    g.body[2].x = 1;
    g.body[2].y = 2;
    g.body[3].x = 1;
    g.body[3].y = 1;
    g.food.x = 0;
    g.food.y = SNAKE_GRID_H - 1;
    g.heading = SNAKE_DIR_LEFT;
    g.pending_dir = SNAKE_DIR_LEFT;

    Snake_Step(&g);

    bool ok = (g.state == SNAKE_STATE_RUNNING)
        && (g.body[0].x == 1 && g.body[0].y == 1);
    CHECK(ok);
}

static void test_eat_food_grows_and_scores(void)
{
    SnakeGame g;
    Snake_Init(&g, 42);

    uint16_t old_length = g.length;
    uint16_t old_score = g.score;
    g.food.x = (uint8_t)(g.body[0].x + 1);
    g.food.y = g.body[0].y;

    Snake_Step(&g);

    bool ok = (g.length == old_length + 1)
        && (g.score == old_score + 1)
        && (g.state == SNAKE_STATE_RUNNING)
        && (g.food.x < SNAKE_GRID_W && g.food.y < SNAKE_GRID_H)
        && (!Snake_CellOccupied(&g, g.food.x, g.food.y));
    CHECK(ok);
}

static void test_step_is_noop_after_gameover(void)
{
    SnakeGame g;
    Snake_Init(&g, 12);

    g.state = SNAKE_STATE_GAME_OVER;
    SnakeCell head_before = g.body[0];
    uint16_t length_before = g.length;

    Snake_Step(&g);

    bool ok = (g.state == SNAKE_STATE_GAME_OVER)
        && (g.body[0].x == head_before.x && g.body[0].y == head_before.y)
        && (g.length == length_before);
    CHECK(ok);
}

static void test_win_when_grid_fills(void)
{
    SnakeGame g;
    Snake_Init(&g, 13);

    /* Build a Hamiltonian (boustrophedon) path over the whole grid. */
    static SnakeCell path[SNAKE_MAX_LENGTH];
    uint16_t idx = 0;
    for (uint8_t y = 0; y < SNAKE_GRID_H; y++) {
        if (y % 2 == 0) {
            for (uint8_t x = 0; x < SNAKE_GRID_W; x++) {
                path[idx].x = x;
                path[idx].y = y;
                idx++;
            }
        } else {
            for (int x = SNAKE_GRID_W - 1; x >= 0; x--) {
                path[idx].x = (uint8_t)x;
                path[idx].y = y;
                idx++;
            }
        }
    }

    /* Snake occupies every cell except the last path cell, which becomes
       the food; the head sits right next to it. */
    g.length = SNAKE_MAX_LENGTH - 1;
    for (uint16_t i = 0; i < g.length; i++) {
        g.body[i] = path[SNAKE_MAX_LENGTH - 2 - i];
    }
    g.food = path[SNAKE_MAX_LENGTH - 1];
    g.heading = SNAKE_DIR_LEFT;
    g.pending_dir = SNAKE_DIR_LEFT;

    Snake_Step(&g);

    CHECK(g.state == SNAKE_STATE_WON && g.length == SNAKE_MAX_LENGTH);
}

int main(void)
{
    test_init_defaults();
    test_occupancy();
    test_food_off_snake_and_in_bounds();
    test_food_is_deterministic();
    test_set_direction_buffers();
    test_reversal_is_rejected();
    test_step_moves_forward();
    test_step_tail_follows_two_steps();
    test_wall_collision_right();
    test_wall_collision_top();
    test_self_collision();
    test_move_into_own_tail_is_allowed();
    test_eat_food_grows_and_scores();
    test_step_is_noop_after_gameover();
    test_win_when_grid_fills();

    printf("%d tests, %d failed\n", g_tests, g_failed);
    return g_failed != 0;
}
