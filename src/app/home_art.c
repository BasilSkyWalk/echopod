#include "home_art.h"

#include "gfx/image.h"
#include "ui/label.h"

enum {
    COVER_MARGIN_TOP = 8,
    REFLECTION_LIGHT = 80,
    NOTICE_MARGIN = 6
};

static const Color PANEL_BACKGROUND = COLOR_RGB(8, 8, 10);
static const Color NOTICE_TEXT = COLOR_RGB(200, 200, 206);

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme)
{
    *art = (HomeArt){.covers = covers, .store = store, .player = player, .theme = theme};
}

static uint32_t album_count(const HomeArt *art)
{
    return art->store->available ? library_music_count(&art->store->library, LIBRARY_ALBUMS) : 0;
}

static bool playing_album(const HomeArt *art, uint16_t *album)
{
    PlayerStatus status = player_status(art->player);
    LibraryTrack track;
    if (!status.has_track || !library_read_track(&art->store->library, status.track, &track)) {
        return false;
    }
    *album = track.album;
    return true;
}

static void start_search(HomeArt *art, uint32_t from, uint32_t count, uint32_t now_ms)
{
    art->searching = true;
    art->probe = (uint16_t)(from % count);
    art->probes_left = count;
    art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
}

static bool probe_next(HomeArt *art, uint32_t count)
{
    uint16_t album = (uint16_t)(art->probe % count);
    art->probe = (uint16_t)((album + 1u) % count);
    art->probes_left--;
    if (!cover_cache_has_cover(art->covers, album)) {
        return false;
    }
    art->album = album;
    art->chosen = true;
    return true;
}

static void search(HomeArt *art, uint32_t count)
{
    for (int tried = 0; tried < HOME_ART_ALBUMS_PER_UPDATE && art->searching; tried++) {
        art->searching = art->probes_left > 0 && !probe_next(art, count);
    }
}

static void cycle(HomeArt *art, uint32_t count, uint32_t now_ms)
{
    if (!art->cycling) {
        art->cycling = true;
        start_search(art, 0, count, now_ms);
    } else if ((int32_t)(now_ms - art->next_change_ms) >= 0) {
        start_search(art, (uint32_t)art->album + 1u, count, now_ms);
    }
    search(art, count);
}

static void forget_album(HomeArt *art)
{
    art->cycling = false;
    art->searching = false;
    art->chosen = false;
}

static void follow_playing(HomeArt *art, uint16_t album)
{
    forget_album(art);
    art->album = album;
    art->chosen = true;
}

void home_art_update(void *context, uint32_t now_ms)
{
    HomeArt *art = context;
    uint32_t count = album_count(art);
    uint16_t album;
    if (count == 0) {
        forget_album(art);
    } else if (playing_album(art, &album)) {
        follow_playing(art, album);
    } else if (art->covers->available) {
        cycle(art, count, now_ms);
    }
    art->has_album = art->chosen;
    if (art->chosen) {
        cover_cache_load_full(art->covers, art->album);
    }
}

void home_art_draw(void *context, Canvas *canvas, Rect area)
{
    const HomeArt *art = context;
    bool below = theme_is_portrait(art->theme);
    Rect divider =
        below ? (Rect){area.x, area.y, area.width, 1} : (Rect){area.x, area.y, 1, area.height};
    canvas_fill_rect(canvas, area, PANEL_BACKGROUND);
    canvas_fill_rect(canvas, divider, art->theme->colors.divider);
    if (art->notice != NULL) {
        TextStyle style = {.font = art->theme->fonts.small, .color = NOTICE_TEXT};
        Rect line = {area.x + NOTICE_MARGIN, area.y, area.width - 2 * NOTICE_MARGIN, area.height};
        Rect text = label_centered_area(style.font, line, art->notice);
        label_draw(canvas, style, (Label){.area = text, .text = art->notice});
        return;
    }
    Image cover;
    if (!art->has_album || cover_cache_full(art->covers, art->album, &cover) != COVER_LOADED) {
        return;
    }
    int left = area.x + (area.width - COVER_FULL_SIZE) / 2;
    int top = below ? area.y + (area.height - COVER_FULL_SIZE) / 2 : area.y + COVER_MARGIN_TOP;
    Quad quad = {left, COVER_FULL_SIZE, top, COVER_FULL_SIZE, top, COVER_FULL_SIZE};
    image_draw_quad(canvas, cover, quad);
    int rows = area.y + area.height - (top + COVER_FULL_SIZE);
    image_draw_reflection(canvas, cover, quad, (QuadLight){REFLECTION_LIGHT, REFLECTION_LIGHT},
                          rows);
}
