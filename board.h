#ifndef BOARD_H
#define BOARD_H

#include "led-matrix-c.h"
#include <stdint.h>

void draw_box(struct LedCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b);

void draw_board(struct LedCanvas *canvas, int board[][10]);

void clear_board(struct LedCanvas *canvas);

#endif