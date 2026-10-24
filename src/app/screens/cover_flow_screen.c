#include "cover_flow_screen.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "draw_profile.h"
#include "hal/clock.h"
#include "ui/cover_flow.h"
#include "ui/label.h"
#include "ui/marquee.h"
#include "ui/navigation.h"

enum {
    NO_POSITION = -1,
    TITLE_TOP = 126,
    ARTIST_TOP = 148,
    LINE_HEIGHT = 20,
    REFLECTION_ROWS = 34,
    REFLECTION_LIGHT = 80,
    FLOOR_LEVEL = 16
};

static const CoverFlowLayout LAYOUT = {
    .center_x = 160,
    .center_top = 8,
    .center_size = COVER_FULL_SIZE,
    .side_offset = 86,
    .side_spacing = 30,
    .side_width = 46,
    .side_near_height = 104,
    .side_far_height = 78,
    .side_near_light = 230,
    .side_far_light = 140,
    .fade_start = 2 * COVER_FLOW_ONE,
    .fade_end = 2 * COVER_FLOW_ONE + COVER_FLOW_ONE / 2,
};

static const Color BACKDROP = COLOR_RGB(0, 0, 0);

static const char STAR_PREFIX[] = "\xE2\x98\x85 ";
static const char EMPTY_MESSAGE[] = "Nothing here";

static int min_of(int first, int second)
{
    return first < second ? first : second;
}

static int max_of(int first, int second)
{
    return first > second ? first : second;
}

CoverFlowOrder cover_flow_order_make(const Library *library)
{
    uint32_t groups = library_count(library, LIBRARY_ALBUM_ARTISTS);
    CoverFlowOrder order = {.first_link = 0, .count = 0, .linked = groups > 0};
    uint32_t next = 0;
    for (uint32_t index = 0; index < groups && order.linked; index++) {
        LibraryGroup group;
        if (!library_read_group(library, LIBRARY_ALBUM_ARTISTS, index, &group) ||
            (index > 0 && group.first_link != next)) {
            order.linked = false;
            break;
        }
        order.first_link = index == 0 ? group.first_link : order.first_link;
        next = group.first_link + group.link_count;
    }
    order.count = order.linked ? next - order.first_link : library_count(library, LIBRARY_ALBUMS);
    return order;
}

bool cover_flow_order_album(const Library *library, CoverFlowOrder order, uint32_t position,
                            uint16_t *album)
{
    if (position >= order.count) {
        return false;
    }
    if (!order.linked) {
        *album = (uint16_t)position;
        return true;
    }
    return library_read_index(library, LIBRARY_LINKS, order.first_link + position, album);
}

bool cover_flow_order_find(const Library *library, CoverFlowOrder order, uint16_t album,
                           uint32_t *position)
{
    LibraryAlbum record;
    LibraryGroup group;
    if (!library_read_album(library, album, &record) || album >= order.count) {
        return false;
    }
    if (!order.linked) {
        *position = album;
        return true;
    }
    if (!library_read_group(library, LIBRARY_ALBUM_ARTISTS, record.album_artist, &group)) {
        return false;
    }
    for (uint32_t link = 0; link < group.link_count; link++) {
        uint16_t value = 0;
        if (library_read_index(library, LIBRARY_LINKS, group.first_link + link, &value) &&
            value == album) {
            *position = group.first_link + link - order.first_link;
            return true;
        }
    }
    return false;
}

static uint16_t album_of(const CoverFlowScreen *flow, int position)
{
    uint16_t album = LIBRARY_NONE;
    if (position < 0 ||
        !cover_flow_order_album(flow->config.library, flow->order, (uint32_t)position, &album)) {
        return LIBRARY_NONE;
    }
    return album;
}

static int center_of(const CoverFlowScreen *flow)
{
    return (int)((flow->position + COVER_FLOW_ONE / 2) / COVER_FLOW_ONE);
}

static void load_text(CoverFlowScreen *flow, uint16_t album_index)
{
    const Library *library = flow->config.library;
    LibraryAlbum album;
    LibraryGroup artist;
    char title[COVER_FLOW_TEXT_CAPACITY];
    flow->title[0] = '\0';
    flow->artist[0] = '\0';
    if (!library_read_album(library, album_index, &album) ||
        !library_read_string(library, album.title, title, sizeof title)) {
        return;
    }
    StringBuilder builder = string_builder_make(flow->title, sizeof flow->title);
    if (favorites_store_contains_album(flow->config.favorites, album_index)) {
        string_builder_append(&builder, STAR_PREFIX);
    }
    string_builder_append(&builder, title);
    if (library_read_group(library, LIBRARY_ALBUM_ARTISTS, album.album_artist, &artist)) {
        library_read_string(library, artist.name, flow->artist, sizeof flow->artist);
    }
}

static void refresh_text(CoverFlowScreen *flow, bool force)
{
    int center = center_of(flow);
    if (flow->count == 0 || (!force && center == flow->text_position)) {
        return;
    }
    load_text(flow, album_of(flow, center));
    flow->text_position = center;
    flow->text_changed_ms = flow->now_ms;
}

static bool load_nearest_half(CoverFlowScreen *flow)
{
    int center = center_of(flow);
    for (int distance = 0; distance <= COVER_CACHE_SLOTS / 2; distance++) {
        for (int side = -1; side <= 1; side += 2) {
            int position = center + side * distance;
            if (position >= 0 && position < flow->count &&
                cover_cache_load_half(flow->config.covers, position, album_of(flow, position))) {
                return true;
            }
        }
    }
    return false;
}

static void load_covers(CoverFlowScreen *flow)
{
    if (flow->count == 0 || load_nearest_half(flow)) {
        return;
    }
    if (flow->position == (int32_t)flow->target * COVER_FLOW_ONE) {
        cover_cache_load_full(flow->config.covers, album_of(flow, flow->target));
    }
}

static int playing_position(const CoverFlowScreen *flow)
{
    PlayerStatus status = player_status(flow->config.player);
    LibraryTrack track;
    if (!status.has_track || !library_read_track(flow->config.library, status.track, &track)) {
        return NO_POSITION;
    }
    uint32_t position = 0;
    if (!cover_flow_order_find(flow->config.library, flow->order, track.album, &position)) {
        return NO_POSITION;
    }
    return (int)position;
}

static void move_target(CoverFlowScreen *flow, int delta)
{
    flow->target = max_of(0, min_of(flow->count - 1, flow->target + delta));
}

static ScreenAction open_album(CoverFlowScreen *flow)
{
    BrowseQuery child;
    BrowseItem item = {
        .kind = BROWSE_ITEM_ALBUM, .group = LIBRARY_ALBUMS, .index = album_of(flow, flow->target)};
    if (flow->count == 0 || !browse_child(browse_root_query(BROWSE_ALBUMS), item, &child)) {
        return screen_action_none();
    }
    Screen *album = browse_pool_open(flow->config.browse, 0, child);
    return album != NULL ? screen_action_push(album) : screen_action_none();
}

static ScreenAction toggle_favorite(CoverFlowScreen *flow)
{
    uint16_t album = album_of(flow, flow->target);
    if (flow->count == 0 || album == LIBRARY_NONE) {
        return screen_action_none();
    }
    FavoriteResult result = favorites_store_toggle_album(flow->config.favorites, album);
    toast_show(flow->config.toast, favorites_store_message(result), flow->now_ms);
    refresh_text(flow, true);
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    CoverFlowScreen *flow = context;
    flow->now_ms = now_ms;
    switch (navigation_from_key(event).intent) {
    case NAVIGATION_PREVIOUS:
        move_target(flow, -1);
        return screen_action_none();
    case NAVIGATION_NEXT:
        move_target(flow, 1);
        return screen_action_none();
    case NAVIGATION_SELECT:
        return open_album(flow);
    case NAVIGATION_OPTIONS:
        return toggle_favorite(flow);
    case NAVIGATION_BACK:
        return screen_action_pop();
    case NAVIGATION_HOME:
        return screen_action_pop_to_root();
    case NAVIGATION_NONE:
        return screen_action_none();
    }
    return screen_action_none();
}

static void enter(void *context, uint32_t now_ms)
{
    CoverFlowScreen *flow = context;
    if (!flow->positioned) {
        int playing = playing_position(flow);
        flow->target = playing != NO_POSITION ? playing : 0;
        flow->positioned = true;
    }
    flow->position = (int32_t)flow->target * COVER_FLOW_ONE;
    flow->last_ms = now_ms;
    flow->now_ms = now_ms;
    refresh_text(flow, true);
}

static void update(void *context, uint32_t now_ms)
{
    CoverFlowScreen *flow = context;
    int32_t target = (int32_t)flow->target * COVER_FLOW_ONE;
    flow->position = cover_flow_approach(flow->position, target, now_ms - flow->last_ms);
    flow->last_ms = now_ms;
    flow->now_ms = now_ms;
    load_covers(flow);
    refresh_text(flow, false);
}

static bool cover_image(const CoverFlowScreen *flow, int position, Image *image)
{
    uint16_t album = album_of(flow, position);
    return cover_cache_full(flow->config.covers, album, image) == COVER_LOADED ||
           cover_cache_half(flow->config.covers, position, album, image) == COVER_LOADED;
}

static void draw_cover(const CoverFlowScreen *flow, Canvas *canvas, int position)
{
    int32_t relative = (int32_t)position * COVER_FLOW_ONE - flow->position;
    Quad quad = cover_flow_quad(&LAYOUT, relative);
    QuadLight light = cover_flow_light(&LAYOUT, relative);
    if (light.left == 0 && light.right == 0) {
        return;
    }
    Image image;
    if (cover_image(flow, position, &image)) {
        uint32_t started_us = hal_clock_microseconds();
        image_draw_quad_lit(canvas, image, quad, light);
        uint32_t reflection_us = hal_clock_microseconds();
        draw_profile_add(DRAW_PART_COVERS, reflection_us - started_us);
        QuadLight reflected = {(uint16_t)(light.left * REFLECTION_LIGHT / COLOR_LIGHT_FULL),
                               (uint16_t)(light.right * REFLECTION_LIGHT / COLOR_LIGHT_FULL)};
        image_draw_reflection(canvas, image, quad, reflected, REFLECTION_ROWS);
        draw_profile_add(DRAW_PART_REFLECTIONS, hal_clock_microseconds() - reflection_us);
    } else {
        image_fill_quad(canvas, quad, flow->config.theme->colors.divider);
    }
}

static void draw_line(const CoverFlowScreen *flow, Canvas *canvas, Label label, TextStyle style)
{
    int width = text_width(style.font, label.text);
    if (width <= label.area.width) {
        label_draw(canvas, style,
                   (Label){label_centered_area(style.font, label.area, label.text), label.text});
        return;
    }
    MarqueeTiming timing = flow->config.theme->marquee;
    int offset =
        marquee_offset(timing, width, label.area.width, flow->now_ms - flow->text_changed_ms);
    label_draw_scrolling(canvas, style, label, (Marquee){.offset = offset, .gap = timing.gap_px});
}

static void draw_text(const CoverFlowScreen *flow, Canvas *canvas)
{
    const Theme *theme = flow->config.theme;
    int padding = theme->metrics.padding;
    int width = theme->metrics.screen_width - 2 * padding;
    TextStyle title = {.font = theme->fonts.title, .color = theme->colors.text};
    TextStyle artist = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    draw_line(flow, canvas, (Label){{padding, TITLE_TOP, width, LINE_HEIGHT}, flow->title}, title);
    draw_line(flow, canvas, (Label){{padding, ARTIST_TOP, width, LINE_HEIGHT}, flow->artist},
              artist);
}

static void draw_empty(const Theme *theme, Canvas *canvas)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    Rect screen = {0, 0, theme->metrics.screen_width, theme->metrics.screen_height};
    label_draw(canvas, style,
               (Label){label_centered_area(style.font, screen, EMPTY_MESSAGE), EMPTY_MESSAGE});
}

static void draw_backdrop(const Theme *theme, Canvas *canvas)
{
    int floor = LAYOUT.center_top + LAYOUT.center_size;
    int height = theme->metrics.screen_height;
    canvas_fill_rect(canvas, (Rect){0, 0, theme->metrics.screen_width, floor}, BACKDROP);
    for (int y = floor; y < height; y++) {
        uint8_t level = (uint8_t)(FLOOR_LEVEL * (height - y) / (height - floor));
        canvas_fill_rect(canvas, (Rect){0, y, theme->metrics.screen_width, 1},
                         color_from_rgb(level, level, level));
    }
}

static void draw(void *context, Canvas *canvas)
{
    const CoverFlowScreen *flow = context;
    uint32_t backdrop_started_us = hal_clock_microseconds();
    draw_backdrop(flow->config.theme, canvas);
    draw_profile_add(DRAW_PART_BACKDROP, hal_clock_microseconds() - backdrop_started_us);
    if (flow->count == 0) {
        draw_empty(flow->config.theme, canvas);
        return;
    }
    int order[COVER_FLOW_MAX_VISIBLE];
    int visible = cover_flow_draw_order(flow->position, flow->count, order);
    for (int index = 0; index < visible; index++) {
        draw_cover(flow, canvas, order[index]);
    }
    uint32_t text_started_us = hal_clock_microseconds();
    draw_text(flow, canvas);
    draw_profile_add(DRAW_PART_TEXT, hal_clock_microseconds() - text_started_us);
}

Screen *cover_flow_screen_init(CoverFlowScreen *flow, CoverFlowConfig config)
{
    flow->config = config;
    flow->order = cover_flow_order_make(config.library);
    flow->count = (int)flow->order.count;
    flow->target = 0;
    flow->position = 0;
    flow->positioned = false;
    flow->text_position = NO_POSITION;
    flow->title[0] = '\0';
    flow->artist[0] = '\0';
    flow->screen = (Screen){
        .title = "Cover Flow",
        .context = flow,
        .full_screen = true,
        .paints_every_pixel = true,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &flow->screen;
}
