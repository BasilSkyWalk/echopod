#include "home_art.h"

#include "gfx/image.h"

enum {
    COVER_MARGIN_TOP = 8,
    REFLECTION_LIGHT = 80
};

static const Color PANEL_BACKGROUND = COLOR_RGB(8, 8, 10);

void home_art_init(HomeArt *art, CoverCache *covers, const LibraryStore *store, Player *player,
                   const Theme *theme)
{
    *art = (HomeArt){.covers = covers, .store = store, .player = player, .theme = theme};
}

static uint32_t album_count(const HomeArt *art)
{
    return art->store->available ? library_count(&art->store->library, LIBRARY_ALBUMS) : 0;
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

static bool cover_missing(const HomeArt *art, uint16_t album)
{
    Image ignored;
    cover_cache_load_full(art->covers, album);
    return cover_cache_full(art->covers, album, &ignored) == COVER_MISSING;
}

static void show_next_with_a_cover(HomeArt *art, uint32_t count)
{
    for (uint32_t tried = 0; tried < count; tried++) {
        art->album = (uint16_t)((art->album + 1) % count);
        if (!cover_missing(art, art->album)) {
            return;
        }
    }
}

static void cycle(HomeArt *art, uint32_t count, uint32_t now_ms)
{
    if (!art->cycling) {
        art->cycling = true;
        art->album = (uint16_t)(count - 1);
        show_next_with_a_cover(art, count);
        art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
        return;
    }
    if ((int32_t)(now_ms - art->next_change_ms) >= 0) {
        show_next_with_a_cover(art, count);
        art->next_change_ms = now_ms + HOME_ART_CYCLE_MS;
    }
}

void home_art_update(void *context, uint32_t now_ms)
{
    HomeArt *art = context;
    uint32_t count = album_count(art);
    art->has_album = count > 0;
    if (!art->has_album) {
        return;
    }
    uint16_t album;
    if (playing_album(art, &album)) {
        art->cycling = false;
        art->album = album;
    } else {
        cycle(art, count, now_ms);
    }
    cover_cache_load_full(art->covers, art->album);
}

void home_art_draw(void *context, Canvas *canvas, Rect area)
{
    const HomeArt *art = context;
    canvas_fill_rect(canvas, area, PANEL_BACKGROUND);
    canvas_fill_rect(canvas, (Rect){area.x, area.y, 1, area.height}, art->theme->colors.divider);
    Image cover;
    if (!art->has_album || cover_cache_full(art->covers, art->album, &cover) != COVER_LOADED) {
        return;
    }
    int left = area.x + (area.width - COVER_FULL_SIZE) / 2;
    int top = area.y + COVER_MARGIN_TOP;
    Quad quad = {left, COVER_FULL_SIZE, top, COVER_FULL_SIZE, top, COVER_FULL_SIZE};
    image_draw_quad(canvas, cover, quad);
    int rows = area.y + area.height - (top + COVER_FULL_SIZE);
    image_draw_reflection(canvas, cover, quad, (QuadLight){REFLECTION_LIGHT, REFLECTION_LIGHT},
                          rows);
}
