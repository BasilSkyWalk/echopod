#include "app/app.h"
#include "board.h"

void board_main(void)
{
    board_clock_start();
    board_input_start();
    board_display_start();
    app_start();
    for (;;) {
        app_step();
    }
}
