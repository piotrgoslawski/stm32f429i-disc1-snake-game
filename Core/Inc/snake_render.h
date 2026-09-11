#ifndef SNAKE_RENDER_H
#define SNAKE_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "snake.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SNAKE_CELL_PX   10
#define SNAKE_ORIGIN_X  0
#define SNAKE_ORIGIN_Y  20 /* HUD occupies rows 0..19 */

/* Full redraw: clears the screen, draws the HUD, snake, and food.
   Only call on game start/restart. */
void SnakeRender_DrawPlayfield(const SnakeGame *g);

/* Incremental redraw for one Snake_Step: draws the new head, erases the
   vacated tail cell (unless food was eaten this step, in which case the
   snake grew and no tail cell was vacated), draws new food and refreshes
   the HUD score when food was eaten. */
void SnakeRender_UpdateStep(const SnakeGame *g, uint8_t old_tail_x, uint8_t old_tail_y, bool ate_food);

/* Overlays an end-of-game message and final score on top of the playfield
   (which is left visible underneath, i.e. no clear). */
void SnakeRender_DrawEndScreen(const SnakeGame *g, const char *msg);

/* Clears the screen and shows a single centred message line. Used for
   transient status screens (e.g. calibration) that precede a game. */
void SnakeRender_DrawMessage(const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* SNAKE_RENDER_H */
