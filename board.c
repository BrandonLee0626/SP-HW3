#include <string.h>
#include <unistd.h>
#include "led-matrix-c.h"

#define LED_ROW 64
#define LED_COL 64

void draw_box(struct LedCanvas *canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b) 
{
    for(int i=0;i<8;i++) draw_line(canvas, x, y+i, x + 7, y+i, r, g, b);

    draw_line(canvas, x, y, x + 7, y, 0, 0, 0);
    draw_line(canvas, x, y + 7, x + 7, y + 7, 0, 0, 0);
    draw_line(canvas, x, y, x, y + 7, 0, 0, 0);
    draw_line(canvas, x + 7, y, x + 7, y + 7, 0, 0, 0);
}

void draw_board(struct LedCanvas *canvas, int board[][10]) {
    for(int i=1;i<=8;i++){
        for(int j=1;j<=8;j++){
            if(board[i][j] == 2) draw_box(canvas, (i-1)*8, (8-j)*8, 255, 0, 0);
            if(board[i][j] == 3) draw_box(canvas, (i-1)*8, (8-j)*8, 0, 0, 255);
        }
    }
}

void clear_board(struct LedCanvas *canvas) {
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            led_canvas_set_pixel(canvas, x, y, 0, 0, 0);
        }
    }
}