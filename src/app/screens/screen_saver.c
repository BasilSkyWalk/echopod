#include "screen_saver.h"

#include "core/i18n.h"
#include "core/string_builder.h"
#include "gfx/image.h"
#include "ui/label.h"

static bool wants_artwork(const ScreenSaverScreen *saver)
{
    ScreensaverStyle style = saver->store->settings.screensaver;
    return style == SCREENSAVER_ARTWORK || style == SCREENSAVER_CLOCK_ARTWORK;
}

static bool wants_clock(const ScreenSaverScreen *saver)
{
    ScreensaverStyle style = saver->store->settings.screensaver;
    return style == SCREENSAVER_CLOCK || style == SCREENSAVER_CLOCK_ARTWORK;
}

static void draw_centered(Canvas *canvas, TextStyle style, Rect area, const char *text)
{
    label_draw(canvas, style,
               (Label){.area = label_centered_area(style.font, area, text), .text = text});
}

static void update(void *context, uint32_t now_ms)
{
    ScreenSaverScreen *saver = context;
    saver->now_ms = now_ms;
    saver->has_album = false;
    if (!wants_artwork(saver) || !saver->player->has_track || saver->player->library == NULL) {
        return;
    }
    LibraryTrack track;
    LibraryAlbum album;
    if (library_read_track(saver->player->library, saver->player->track, &track) &&
        library_read_album(saver->player->library, track.album, &album)) {
        saver->album = track.album;
        saver->has_album = cover_cache_load_full(saver->covers, saver->album);
    }
}

static void draw(void *context, Canvas *canvas)
{
    const ScreenSaverScreen *saver = context;
    const Theme *theme = saver->theme;
    Rect area = canvas->bounds;
    canvas_fill_rect(canvas, area, theme->colors.background);
    Image cover;
    bool show_art = wants_artwork(saver) && saver->has_album &&
                    cover_cache_full(saver->covers, saver->album, &cover) == COVER_LOADED;
    bool show_clock = wants_clock(saver);
    if (show_art && show_clock) {
        if (theme_is_portrait(theme)) {
            Quad quad = {(area.width - COVER_FULL_SIZE) / 2, COVER_FULL_SIZE,
                         area.height / 2 - COVER_FULL_SIZE / 2, COVER_FULL_SIZE,
                         area.height / 2 - COVER_FULL_SIZE / 2, COVER_FULL_SIZE};
            image_draw_quad(canvas, cover, quad);
            char time[8];
            clock_screen_format(saver->clock, saver->now_ms, time, sizeof time);
            draw_centered(canvas,
                          (TextStyle){.font = theme->fonts.title, .color = theme->colors.text},
                          (Rect){0, area.height / 2 + 28, area.width, 52}, time);
        } else {
            Quad quad = {24, COVER_FULL_SIZE, (area.height - COVER_FULL_SIZE) / 2,
                         COVER_FULL_SIZE, (area.height - COVER_FULL_SIZE) / 2, COVER_FULL_SIZE};
            image_draw_quad(canvas, cover, quad);
            char time[8];
            clock_screen_format(saver->clock, saver->now_ms, time, sizeof time);
            draw_centered(canvas,
                          (TextStyle){.font = theme->fonts.title, .color = theme->colors.text},
                          (Rect){156, 0, 150, area.height}, time);
        }
    } else if (show_art) {
        Quad quad = {(area.width - COVER_FULL_SIZE) / 2, COVER_FULL_SIZE,
                     (area.height - COVER_FULL_SIZE) / 2, COVER_FULL_SIZE,
                     (area.height - COVER_FULL_SIZE) / 2, COVER_FULL_SIZE};
        image_draw_quad(canvas, cover, quad);
    } else if (show_clock) {
        char time[8];
        clock_screen_format(saver->clock, saver->now_ms, time, sizeof time);
        draw_centered(canvas,
                      (TextStyle){.font = theme->fonts.title, .color = theme->colors.text},
                      area, time);
    }
}

Screen *screen_saver_init(ScreenSaverScreen *saver, SettingsStore *store, Player *player,
                          CoverCache *covers, ClockScreen *clock, const Theme *theme)
{
    *saver = (ScreenSaverScreen){.theme = theme, .store = store, .player = player,
                                 .covers = covers, .clock = clock};
    saver->screen = (Screen){.title = i18n_text(STR_MENU_CLOCK),
                             .full_screen = true,
                             .paints_every_pixel = true,
                             .stays_on_wake = true,
                             .context = saver,
                             .update = update,
                             .draw = draw};
    return &saver->screen;
}
