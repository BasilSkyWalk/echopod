#include "fake_hal.h"

#include "hal/clock.h"
#include "hal/display.h"
#include "hal/input.h"
#include "hal/storage.h"

#include <string.h>

enum {
    TRANSITION_CAPACITY = 16,
    FILE_CAPACITY = 4
};

typedef struct FakeFile {
    const char *path;
    const uint8_t *bytes;
    uint32_t size;
} FakeFile;

typedef struct FakeHal {
    uint16_t panel[HAL_DISPLAY_HEIGHT][HAL_DISPLAY_WIDTH];
    long pixels_written;
    int present_count;
    uint32_t now_ms;
    uint32_t now_us;
    uint32_t microseconds_per_read;
    HalKeyTransition transitions[TRANSITION_CAPACITY];
    int transition_count;
    int next_transition;
    FakeFile files[FILE_CAPACITY];
    int file_count;
} FakeHal;

static FakeHal fake;

void fake_hal_reset(void)
{
    fake = (FakeHal){0};
}

void fake_hal_add_file(const char *path, const uint8_t *bytes, uint32_t size)
{
    fake.files[fake.file_count++] = (FakeFile){.path = path, .bytes = bytes, .size = size};
}

void fake_hal_set_milliseconds(uint32_t now_ms)
{
    fake.now_ms = now_ms;
}

void fake_hal_set_microseconds_per_read(uint32_t step_us)
{
    fake.microseconds_per_read = step_us;
}

void fake_hal_queue_transition(Key key, bool pressed, uint32_t time_ms)
{
    if (fake.transition_count == TRANSITION_CAPACITY) {
        return;
    }
    fake.transitions[fake.transition_count++] =
        (HalKeyTransition){.key = key, .pressed = pressed, .time_ms = time_ms};
}

uint16_t fake_hal_pixel(int x, int y)
{
    return fake.panel[y][x];
}

long fake_hal_pixels_written(void)
{
    return fake.pixels_written;
}

int fake_hal_present_count(void)
{
    return fake.present_count;
}

void hal_display_write(int x, int y, int width, int height, const uint16_t *pixels)
{
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            fake.panel[y + row][x + column] = pixels[row * width + column];
        }
    }
    fake.pixels_written += (long)width * height;
}

void hal_display_present(void)
{
    fake.present_count++;
}

bool hal_input_next_transition(HalKeyTransition *transition)
{
    if (fake.next_transition == fake.transition_count) {
        fake.next_transition = 0;
        fake.transition_count = 0;
        return false;
    }
    *transition = fake.transitions[fake.next_transition++];
    return true;
}

uint32_t hal_clock_milliseconds(void)
{
    return fake.now_ms;
}

uint32_t hal_clock_microseconds(void)
{
    fake.now_us += fake.microseconds_per_read;
    return fake.now_us;
}

bool hal_file_open(HalFile *file, const char *path)
{
    for (int index = 0; index < fake.file_count; index++) {
        if (strcmp(fake.files[index].path, path) == 0) {
            *file = (HalFile){.handle = index, .size = fake.files[index].size};
            return true;
        }
    }
    *file = (HalFile){.handle = -1, .size = 0};
    return false;
}

bool hal_file_read(HalFile file, uint32_t offset, void *buffer, uint32_t length)
{
    if (file.handle < 0 || file.handle >= fake.file_count) {
        return false;
    }
    const FakeFile *source = &fake.files[file.handle];
    if (offset > source->size || length > source->size - offset) {
        return false;
    }
    memcpy(buffer, source->bytes + offset, length);
    return true;
}

void hal_file_close(HalFile *file)
{
    file->handle = -1;
}
