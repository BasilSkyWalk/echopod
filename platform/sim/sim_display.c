#include "sim_display.h"

#include <SDL.h>

#include "hal/display.h"

enum {
    BITS_PER_PIXEL = 16,
    PANEL_ROW_BYTES = HAL_DISPLAY_WIDTH * 2,
    FULL_INTENSITY = 255,
    DIMMEST_LEVEL_OFFSET = 2
};

typedef struct SimDisplay {
    uint16_t panel[HAL_DISPLAY_HEIGHT][HAL_DISPLAY_WIDTH];
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    bool powered;
    uint8_t brightness;
} SimDisplay;

static SimDisplay display = {.powered = true, .brightness = HAL_DISPLAY_BRIGHTNESS_MAX};

static bool area_fits_panel(int x, int y, int width, int height)
{
    return x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= HAL_DISPLAY_WIDTH &&
           y + height <= HAL_DISPLAY_HEIGHT;
}

bool sim_display_open(int scale)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    display.window =
        SDL_CreateWindow("EchoPod", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         HAL_DISPLAY_WIDTH * scale, HAL_DISPLAY_HEIGHT * scale, SDL_WINDOW_SHOWN);
    if (display.window == NULL) {
        return false;
    }
    display.renderer = SDL_CreateRenderer(display.window, -1, SDL_RENDERER_ACCELERATED);
    if (display.renderer == NULL) {
        return false;
    }
    display.texture =
        SDL_CreateTexture(display.renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                          HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT);
    return display.texture != NULL;
}

void sim_display_close(void)
{
    if (display.texture != NULL) {
        SDL_DestroyTexture(display.texture);
    }
    if (display.renderer != NULL) {
        SDL_DestroyRenderer(display.renderer);
    }
    if (display.window != NULL) {
        SDL_DestroyWindow(display.window);
    }
    display = (SimDisplay){.powered = true, .brightness = HAL_DISPLAY_BRIGHTNESS_MAX};
}

void sim_display_set_title(const char *title)
{
    if (display.window != NULL) {
        SDL_SetWindowTitle(display.window, title);
    }
}

bool sim_display_save_bmp(const char *path)
{
    SDL_Surface *frame =
        SDL_CreateRGBSurfaceWithFormatFrom(display.panel, HAL_DISPLAY_WIDTH, HAL_DISPLAY_HEIGHT,
                                           BITS_PER_PIXEL, PANEL_ROW_BYTES, SDL_PIXELFORMAT_RGB565);
    if (frame == NULL) {
        return false;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(frame, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(frame);
    if (converted == NULL) {
        return false;
    }
    bool saved = SDL_SaveBMP(converted, path) == 0;
    SDL_FreeSurface(converted);
    return saved;
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    if (!area_fits_panel(x, y, width, height)) {
        SDL_Log("display write outside the panel: %d,%d %dx%d", x, y, width, height);
        return;
    }
    for (int row = 0; row < height; row++) {
        SDL_memcpy(&display.panel[y + row][x], &pixels[row * width],
                   (size_t)width * sizeof(uint16_t));
    }
}

static Uint8 brightness_intensity(uint8_t level)
{
    int span = HAL_DISPLAY_BRIGHTNESS_MAX + DIMMEST_LEVEL_OFFSET;
    return (Uint8)(FULL_INTENSITY * (level + DIMMEST_LEVEL_OFFSET) / span);
}

static void show_panel(void)
{
    if (display.texture == NULL) {
        return;
    }
    SDL_SetRenderDrawColor(display.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(display.renderer);
    if (display.powered) {
        Uint8 intensity = brightness_intensity(display.brightness);
        SDL_SetTextureColorMod(display.texture, intensity, intensity, intensity);
        SDL_RenderCopy(display.renderer, display.texture, NULL, NULL);
    }
    SDL_RenderPresent(display.renderer);
}

void hal_display_present(void)
{
    if (display.texture == NULL) {
        return;
    }
    SDL_UpdateTexture(display.texture, NULL, display.panel, PANEL_ROW_BYTES);
    show_panel();
}

void hal_display_set_power(bool on)
{
    display.powered = on;
    show_panel();
}

void hal_display_set_brightness(uint8_t level)
{
    display.brightness = level;
    show_panel();
}
