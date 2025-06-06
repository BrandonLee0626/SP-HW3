#include <string.h>
#include <unistd.h>
#include <stdlib.h>
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

void draw_X(struct LedCanvas *canvas, int x, int y)
{
    draw_line(canvas, x+1, y+1, x+6, y+6, 0, 255, 0);
    draw_line(canvas, x+6, y+1, x+1, y+6, 0, 255, 0);
}

void draw_board(struct LedCanvas *canvas, int board[][10]) {
    for(int i=1;i<=8;i++){
        for(int j=1;j<=8;j++){
            if(board[i][j] == 1) draw_X(canvas, (i-1)*8, (8-j)*8);
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

#ifdef BOARD_STANDALONE
int main()
{
    RGBLedMatrixOptions options;
    memset(&options, 0, sizeof(options));
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 50;
    options.disable_hardware_pulsing = 1;

    struct RGBLedMatrix *matrix = led_matrix_create_from_options(&options, NULL, NULL);
    if (matrix == NULL) {
        return 1;
    }

    struct LedCanvas *canvas = led_matrix_get_canvas(matrix);

    int board[10][10];

        char line[10], input[100];
    
    for(int i=1;i<=8;i++){
        fgets(line, 10, stdin);
        for(int j=0;j<8;j++){
            switch (line[j])
            {
            case '.':
                board[i][j+1] = 0;
                break;

            case '#':
                board[i][j+1] = 1;
                break;

            case 'R':
                board[i][j+1] = 2;
                break;

            case 'B':
                board[i][j+1] = 3;
                break;
            
            default:
                printf("Board input error");
                exit(0);
                break;
            }
        }
    }

    draw_board(canvas, board);

    sleep(10);
}
#endif