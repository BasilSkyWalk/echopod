#include "cover_flow_screen.h"

#include <stddef.h>

#include "core/i18n.h"
#include "core/random.h"
#include "core/string_builder.h"
#include "draw_profile.h"
#include "hal/audio.h"
#include "hal/clock.h"
#include "settings/settings.h"
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
    FLOOR_LEVEL = 16,
    RANDOM_POSITION = 0,
    ORDER_BY_ARTIST = -1,
    ORDER_BY_TITLE = -2,
    REPEATS_PER_SPEED_UP = 8,
    FASTEST_STEP = 4,
    SHUFFLE_SIZE = 48,
    SHUFFLE_STROKE = 4,
    STACK_TITLE_TOP = 272,
    STACK_ARTIST_TOP = 294,
    STACK_FLOOR_TOP = 266,
    THREE_FADE_START = COVER_FLOW_ONE,
    THREE_FADE_END = COVER_FLOW_ONE + COVER_FLOW_ONE / 2,
    FOLDER_DEPTH_LIMIT = 64,
    LINKS_PER_AUDIO_SERVICE = 16
};

typedef struct PlaylistScan {
    uint16_t playing_track;
    uint16_t playing_album;
    uint16_t previous;
    uint32_t runs;
    CoverFlowTile playing;
} PlaylistScan;

static const CoverStackLayout STACK = {
    .center_x = 85,
    .center_top = 84,
    .center_size = COVER_FULL_SIZE,
    .side_offset = 77,
    .side_spacing = 30,
    .side_width = 100,
    .side_height = 30,
    .side_shrink = COVER_FLOW_ONE / 4,
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
    .side_shrink = COVER_FLOW_ONE / 4,
    .side_near_light = 230,
    .side_far_light = 140,
    .fade_start = 2 * COVER_FLOW_ONE,
    .fade_end = 2 * COVER_FLOW_ONE + COVER_FLOW_ONE / 2,
};

static const Color BACKDROP = COLOR_RGB(0, 0, 0);

static const char STAR_PREFIX[] = "\xE2\x98\x85 ";
static const Color SHUFFLE_BACKGROUND = COLOR_RGB(44, 46, 58);
static const Color SHUFFLE_MARK = COLOR_RGB(255, 255, 255);

static Color shuffle_pixels[SHUFFLE_SIZE * SHUFFLE_SIZE];
static bool shuffle_drawn;

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
    uint32_t groups = library_music_count(library, LIBRARY_ALBUM_ARTISTS);
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
    order.count =
        order.linked ? next - order.first_link : library_music_count(library, LIBRARY_ALBUMS);
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
    if (!library_read_album(library, album, &record)) {
        return false;
    }
    if (!order.linked) {
        *position = album;
        return album < order.count;
    }
    uint32_t first = order.first_link;
    uint32_t end = order.first_link + order.count;
    if (library_read_group(library, LIBRARY_ALBUM_ARTISTS, record.album_artist, &group) &&
        group.first_link >= first && group.first_link + group.link_count <= end) {
        first = group.first_link;
        end = group.first_link + group.link_count;
    }
    for (uint32_t link = first; link < end; link++) {
        uint16_t value = 0;
        if (library_read_index(library, LIBRARY_LINKS, link, &value) && value == album) {
            *position = link - order.first_link;
            return true;
        }
    }
    return false;
}

static bool is_random(const CoverFlowScreen *flow, int position)
{
    return flow->first > 0 && position == RANDOM_POSITION;
}

static uint16_t album_of(const CoverFlowScreen *flow, int position)
{
    if (position < flow->first) {
        return LIBRARY_NONE;
    }
    const CoverFlowTile *tile = &flow->tiles[position % COVER_FLOW_TILES];
    return tile->position == position ? tile->album : LIBRARY_NONE;
}

static uint16_t link_album(const Library *library, uint32_t link)
{
    uint16_t track_index = 0;
    LibraryTrack track;
    if (!library_read_index(library, LIBRARY_LINKS, link, &track_index) ||
        !library_read_track(library, track_index, &track)) {
        return LIBRARY_NONE;
    }
    return track.album;
}

static uint16_t folder_album(const Library *library, uint16_t folder)
{
    uint32_t count = library_count(library, LIBRARY_FOLDERS);
    LibraryFolder record;
    for (uint32_t index = folder; index < count; index++) {
        if (!library_read_folder(library, index, &record) ||
            (index > folder && record.parent < folder)) {
            break;
        }
        if (record.track_count > 0) {
            return link_album(library, record.first_link + record.subfolder_count);
        }
    }
    return LIBRARY_NONE;
}

static bool step_run(const CoverFlowScreen *flow, CoverFlowTile *tile, int direction)
{
    const Library *library = flow->config.library;
    uint32_t first = flow->order.first_link;
    uint32_t link = tile->link;
    uint16_t album = tile->album;
    if (direction > 0) {
        while (album == tile->album) {
            if (++link >= first + flow->order.links) {
                return false;
            }
            album = link_album(library, link);
        }
    } else {
        if (link <= first) {
            return false;
        }
        album = link_album(library, --link);
        while (link > first && link_album(library, link - 1) == album) {
            link--;
        }
    }
    *tile = (CoverFlowTile){tile->position + direction, album, album, link};
    return true;
}

static CoverFlowTile first_run(const CoverFlowScreen *flow)
{
    uint16_t album = link_album(flow->config.library, flow->order.first_link);
    return (CoverFlowTile){flow->first, album, album, flow->order.first_link};
}

static int distance(int from, int to)
{
    return from < to ? to - from : from - to;
}

static bool playlist_tile(const CoverFlowScreen *flow, int position, CoverFlowTile *tile)
{
    *tile = first_run(flow);
    for (int slot = 0; slot < COVER_FLOW_TILES; slot++) {
        const CoverFlowTile *known = &flow->tiles[slot];
        if (known->position >= flow->first &&
            distance(known->position, position) < distance(tile->position, position)) {
            *tile = *known;
        }
    }
    while (tile->position != position) {
        if (!step_run(flow, tile, tile->position < position ? 1 : -1)) {
            return false;
        }
    }
    return true;
}

static bool folder_tile(const CoverFlowScreen *flow, int position, CoverFlowTile *tile)
{
    uint32_t link = flow->order.first_link + (uint32_t)(position - flow->first);
    if (!library_read_index(flow->config.library, LIBRARY_LINKS, link, &tile->item)) {
        return false;
    }
    tile->album = folder_album(flow->config.library, tile->item);
    return true;
}

static bool find_tile(const CoverFlowScreen *flow, int position, CoverFlowTile *tile)
{
    switch (flow->order.kind) {
    case COVER_FLOW_PLAYLIST:
        return playlist_tile(flow, position, tile);
    case COVER_FLOW_FOLDERS:
        return folder_tile(flow, position, tile);
    case COVER_FLOW_ALBUMS:
        break;
    }
    bool found = cover_flow_order_album(flow->config.library, flow->order,
                                        (uint32_t)(position - flow->first), &tile->album);
    tile->item = tile->album;
    return found;
}

static bool resolve(CoverFlowScreen *flow, int position, CoverFlowTile *tile)
{
    *tile = (CoverFlowTile){position, LIBRARY_NONE, LIBRARY_NONE, 0};
    if (position < flow->first || position >= flow->count) {
        return false;
    }
    CoverFlowTile *slot = &flow->tiles[position % COVER_FLOW_TILES];
    if (slot->position == position) {
        *tile = *slot;
        return true;
    }
    if (!find_tile(flow, position, tile)) {
        *tile = (CoverFlowTile){position, LIBRARY_NONE, LIBRARY_NONE, 0};
        return false;
    }
    *slot = *tile;
    return true;
}

static int center_of(const CoverFlowScreen *flow)
{
    return (int)((flow->position + COVER_FLOW_ONE / 2) / COVER_FLOW_ONE);
}

static void fill_tiles(CoverFlowScreen *flow)
{
    int center = center_of(flow);
    CoverFlowTile tile;
    for (int position = center - COVER_FLOW_TILES / 2; position <= center + COVER_FLOW_TILES / 2;
         position++) {
        resolve(flow, position, &tile);
    }
}

static void plot(int x, int y)
{
    for (int row = y - SHUFFLE_STROKE / 2; row < y + SHUFFLE_STROKE / 2; row++) {
        for (int column = x - SHUFFLE_STROKE / 2; column < x + SHUFFLE_STROKE / 2; column++) {
            if (row >= 0 && row < SHUFFLE_SIZE && column >= 0 && column < SHUFFLE_SIZE) {
                shuffle_pixels[row * SHUFFLE_SIZE + column] = SHUFFLE_MARK;
            }
        }
    }
}

static void stroke(int from_x, int from_y, int to_x, int to_y)
{
    int steps = max_of(max_of(to_x - from_x, from_x - to_x), max_of(to_y - from_y, from_y - to_y));
    for (int step = 0; step <= steps; step++) {
        int x = from_x + (to_x - from_x) * step / max_of(steps, 1);
        int y = from_y + (to_y - from_y) * step / max_of(steps, 1);
        plot(x, y);
    }
}

static void draw_shuffle_mark(void)
{
    for (int index = 0; index < SHUFFLE_SIZE * SHUFFLE_SIZE; index++) {
        shuffle_pixels[index] = SHUFFLE_BACKGROUND;
    }
    static const int16_t LINES[][4] = {
        {9, 34, 16, 34},  {16, 34, 30, 14}, {30, 14, 39, 14}, {39, 14, 34, 9},
        {39, 14, 34, 19}, {9, 14, 16, 14},  {16, 14, 22, 23}, {25, 27, 30, 34},
        {30, 34, 39, 34}, {39, 34, 34, 29}, {39, 34, 34, 39},
    };
    for (size_t index = 0; index < sizeof LINES / sizeof LINES[0]; index++) {
        stroke(LINES[index][0], LINES[index][1], LINES[index][2], LINES[index][3]);
    }
    shuffle_drawn = true;
}

static Image shuffle_image(void)
{
    if (!shuffle_drawn) {
        draw_shuffle_mark();
    }
    return (Image){.pixels = shuffle_pixels, .width = SHUFFLE_SIZE, .height = SHUFFLE_SIZE};
}

static void load_text(CoverFlowScreen *flow, const CoverFlowTile *tile)
{
    const Library *library = flow->config.library;
    LibraryAlbum album;
    LibraryGroup artist;
    LibraryFolder folder;
    char title[COVER_FLOW_TEXT_CAPACITY];
    bool folders = flow->order.kind == COVER_FLOW_FOLDERS;
    flow->title[0] = '\0';
    flow->artist[0] = '\0';
    if (folders && library_read_folder(library, tile->item, &folder)) {
        browse_folder_name(library, &folder, flow->title, sizeof flow->title);
    }
    if (!library_read_album(library, tile->album, &album) ||
        !library_read_string(library, album.title, title, sizeof title)) {
        return;
    }
    if (!folders) {
        StringBuilder builder = string_builder_make(flow->title, sizeof flow->title);
        if (favorites_store_contains_album(flow->config.favorites, tile->album)) {
            string_builder_append(&builder, STAR_PREFIX);
        }
        string_builder_append(&builder, title);
    }
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
    if (is_random(flow, center)) {
        StringBuilder title = string_builder_make(flow->title, sizeof flow->title);
        string_builder_append(&title, i18n_text(STR_MENU_RANDOM_ALBUM));
        StringBuilder hint = string_builder_make(flow->artist, sizeof flow->artist);
        string_builder_append(&hint, i18n_text(STR_MENU_PLAY_ANY_ALBUM));
    } else {
        CoverFlowTile tile;
        resolve(flow, center, &tile);
        load_text(flow, &tile);
    }
    flow->text_position = center;
    flow->text_changed_ms = flow->now_ms;
}

static bool load_nearest_half(CoverFlowScreen *flow)
{
    int center = center_of(flow);
    for (int distance = 0; distance <= COVER_CACHE_SLOTS / 2; distance++) {
        for (int side = -1; side <= 1; side += 2) {
            int position = center + side * distance;
            if (position >= flow->first && position < flow->count &&
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
    if (!is_random(flow, flow->target) &&
        flow->position == (int32_t)flow->target * COVER_FLOW_ONE) {
        cover_cache_load_full(flow->config.covers, album_of(flow, flow->target));
    }
}

static int playlist_find(CoverFlowScreen *flow, uint16_t album)
{
    CoverFlowTile tile = first_run(flow);
    while (tile.album != album) {
        if (!step_run(flow, &tile, 1)) {
            return NO_POSITION;
        }
    }
    flow->tiles[tile.position % COVER_FLOW_TILES] = tile;
    return tile.position;
}

static int scanned_playlist_find(CoverFlowScreen *flow, uint16_t track, uint16_t album)
{
    const CoverFlowTile *tile = &flow->playing_tile;
    if (track != flow->playing_track) {
        return playlist_find(flow, album);
    }
    if (tile->position != NO_POSITION) {
        flow->tiles[tile->position % COVER_FLOW_TILES] = *tile;
    }
    return tile->position;
}

static bool child_index(const Library *library, CoverFlowOrder order, uint16_t folder,
                        uint32_t *index)
{
    for (uint32_t link = 0; link < order.count; link++) {
        uint16_t value = LIBRARY_NONE;
        if (library_read_index(library, LIBRARY_LINKS, order.first_link + link, &value) &&
            value == folder) {
            *index = link;
            return true;
        }
    }
    return false;
}

static int folder_find(const CoverFlowScreen *flow, uint16_t folder)
{
    LibraryFolder record;
    uint32_t index = 0;
    for (int depth = 0; depth < FOLDER_DEPTH_LIMIT; depth++) {
        if (child_index(flow->config.library, flow->order, folder, &index)) {
            return (int)index + flow->first;
        }
        if (folder == flow->order.folder ||
            !library_read_folder(flow->config.library, folder, &record)) {
            break;
        }
        folder = record.parent;
    }
    return NO_POSITION;
}

static int playing_position(CoverFlowScreen *flow)
{
    PlayerStatus status = player_status(flow->config.player);
    LibraryTrack track;
    uint32_t position = 0;
    if (!status.has_track || !library_read_track(flow->config.library, status.track, &track)) {
        return NO_POSITION;
    }
    switch (flow->order.kind) {
    case COVER_FLOW_PLAYLIST:
        return scanned_playlist_find(flow, status.track, track.album);
    case COVER_FLOW_FOLDERS:
        return folder_find(flow, track.folder);
    case COVER_FLOW_ALBUMS:
        break;
    }
    if (!cover_flow_order_find(flow->config.library, flow->order, track.album, &position)) {
        return NO_POSITION;
    }
    return (int)position + flow->first;
}

static void move_target(CoverFlowScreen *flow, int delta, bool repeated)
{
    flow->held_repeats = repeated ? flow->held_repeats + 1 : 0;
    int step = min_of(FASTEST_STEP, 1 + flow->held_repeats / REPEATS_PER_SPEED_UP);
    flow->target = max_of(0, min_of(flow->count - 1, flow->target + delta * step));
}

static ScreenAction play_random(CoverFlowScreen *flow)
{
    Random random = random_seeded(hal_clock_microseconds());
    int pick = (int)random_below(&random, flow->order.count) + flow->first;
    CoverFlowTile tile;
    BrowseQuery child;
    if (!resolve(flow, pick, &tile)) {
        return screen_action_none();
    }
    BrowseItem item = {.kind = BROWSE_ITEM_ALBUM, .group = LIBRARY_ALBUMS, .index = tile.album};
    PlayerListStart start = {.position = 0, .seed = random_next(&random), .in_order = true};
    if (!browse_child(flow->parent, item, &child)) {
        return screen_action_none();
    }
    start.source = child;
    if (!player_play_from(flow->config.player, start)) {
        return screen_action_none();
    }
    flow->target = pick;
    return screen_action_push(flow->config.browse->config.now_playing);
}

static void use_order(CoverFlowScreen *flow, CoverFlowOrder order, int32_t key, BrowseQuery parent,
                      int album_level)
{
    if (key != flow->order_key) {
        flow->positioned = false;
        flow->order_key = key;
    }
    flow->order = order;
    flow->parent = parent;
    flow->album_level = album_level;
    flow->first = order.kind == COVER_FLOW_FOLDERS ? 0 : 1;
    flow->count = order.count > 0 ? (int)order.count + flow->first : 0;
    flow->text_position = NO_POSITION;
    flow->playing_track = LIBRARY_NONE;
    flow->playing_tile.position = NO_POSITION;
    for (int slot = 0; slot < COVER_FLOW_TILES; slot++) {
        flow->tiles[slot].position = NO_POSITION;
    }
}

static int32_t folder_key(uint16_t folder)
{
    return (int32_t)LIBRARY_FOLDERS << 16 | folder;
}

static bool folder_order(const Library *library, uint16_t folder, CoverFlowOrder *order)
{
    LibraryFolder record;
    if (!library_read_folder(library, folder, &record) || record.subfolder_count == 0 ||
        record.track_count > 0) {
        return false;
    }
    *order = (CoverFlowOrder){.first_link = record.first_link,
                              .count = record.subfolder_count,
                              .linked = true,
                              .kind = COVER_FLOW_FOLDERS,
                              .links = record.subfolder_count,
                              .folder = folder};
    return true;
}

static bool enter_folder(CoverFlowScreen *flow, uint16_t folder, uint16_t child)
{
    CoverFlowOrder order;
    uint32_t index = 0;
    if (!folder_order(flow->config.library, folder, &order)) {
        return false;
    }
    use_order(flow, order, folder_key(folder), flow->parent, flow->album_level);
    flow->target = child_index(flow->config.library, order, child, &index) ? (int)index : 0;
    flow->positioned = true;
    flow->position = (int32_t)flow->target * COVER_FLOW_ONE;
    fill_tiles(flow);
    refresh_text(flow, true);
    return true;
}

static bool leave_folder(CoverFlowScreen *flow)
{
    LibraryFolder record;
    return flow->order.kind == COVER_FLOW_FOLDERS && flow->order.folder != flow->origin &&
           library_read_folder(flow->config.library, flow->order.folder, &record) &&
           enter_folder(flow, record.parent, flow->order.folder);
}

static ScreenAction open_album(CoverFlowScreen *flow)
{
    if (flow->count > 0 && is_random(flow, flow->target)) {
        return play_random(flow);
    }
    BrowseQuery child;
    CoverFlowTile tile;
    if (flow->count == 0 || !resolve(flow, flow->target, &tile)) {
        return screen_action_none();
    }
    BrowseItem item = {.kind = BROWSE_ITEM_ALBUM, .group = LIBRARY_ALBUMS, .index = tile.album};
    if (flow->order.kind == COVER_FLOW_FOLDERS) {
        if (enter_folder(flow, tile.item, LIBRARY_NONE)) {
            return screen_action_none();
        }
        item =
            (BrowseItem){.kind = BROWSE_ITEM_FOLDER, .group = LIBRARY_ARTISTS, .index = tile.item};
    }
    if (!browse_child(flow->parent, item, &child)) {
        return screen_action_none();
    }
    BrowsePool *pool = flow->config.browse;
    Screen *album = browse_pool_open(pool, flow->album_level, child);
    if (album == NULL) {
        return screen_action_none();
    }
    pool->screens[flow->album_level].in_flow = true;
    return screen_action_push(album);
}

static ScreenAction toggle_favorite(CoverFlowScreen *flow)
{
    CoverFlowTile tile;
    if (flow->count == 0 || flow->order.kind == COVER_FLOW_FOLDERS ||
        !resolve(flow, flow->target, &tile) || tile.album == LIBRARY_NONE) {
        return screen_action_none();
    }
    FavoriteResult result = favorites_store_toggle_album(flow->config.favorites, tile.album);
    toast_show(flow->config.toast, favorites_store_message(result), flow->now_ms);
    refresh_text(flow, true);
    return screen_action_none();
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    CoverFlowScreen *flow = context;
    flow->now_ms = now_ms;
    Navigation navigation = navigation_from_key(event);
    switch (navigation.intent) {
    case NAVIGATION_PREVIOUS:
        move_target(flow, -1, navigation.repeated);
        return screen_action_none();
    case NAVIGATION_NEXT:
        move_target(flow, 1, navigation.repeated);
        return screen_action_none();
    case NAVIGATION_SELECT:
        return open_album(flow);
    case NAVIGATION_OPTIONS:
        return toggle_favorite(flow);
    case NAVIGATION_BACK:
        return leave_folder(flow) ? screen_action_none() : screen_action_pop();
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
        flow->target = playing != NO_POSITION ? playing : min_of(flow->first, flow->count - 1);
        flow->positioned = true;
    }
    flow->position = (int32_t)flow->target * COVER_FLOW_ONE;
    flow->last_ms = now_ms;
    flow->now_ms = now_ms;
    fill_tiles(flow);
    refresh_text(flow, true);
}

static void update(void *context, uint32_t now_ms)
{
    CoverFlowScreen *flow = context;
    int32_t target = (int32_t)flow->target * COVER_FLOW_ONE;
    uint32_t time_constant_ms =
        settings_animation_ms(flow->config.options, COVER_FLOW_TIME_CONSTANT_MS);
    flow->position =
        cover_flow_approach(flow->position, target, now_ms - flow->last_ms, time_constant_ms);
    flow->last_ms = now_ms;
    flow->now_ms = now_ms;
    fill_tiles(flow);
    load_covers(flow);
    refresh_text(flow, false);
}

static bool cover_image(const CoverFlowScreen *flow, int position, Image *image)
{
    if (is_random(flow, position)) {
        *image = shuffle_image();
        return true;
    }
    uint16_t album = album_of(flow, position);
    return cover_cache_full(flow->config.covers, album, image) == COVER_LOADED ||
           cover_cache_half(flow->config.covers, position, album, image) == COVER_LOADED;
}

static bool three_covers(const CoverFlowScreen *flow)
{
    const uint8_t *options = flow->config.options;
    return options != NULL && options[OPTION_COVER_FLOW_COVERS] == COVER_FLOW_THREE_COVERS;
}

static void draw_cover(const CoverFlowScreen *flow, Canvas *canvas, int position)
{
    int32_t relative = (int32_t)position * COVER_FLOW_ONE - flow->position;
    bool stacked = theme_is_portrait(flow->config.theme);
    Quad quad =
        stacked ? cover_flow_stacked_quad(&STACK, relative) : cover_flow_quad(&LAYOUT, relative);
    CoverFlowLayout layout = LAYOUT;
    if (three_covers(flow)) {
        layout.fade_start = THREE_FADE_START;
        layout.fade_end = THREE_FADE_END;
    }
    QuadLight light = cover_flow_light(&layout, relative);
    if (light.left == 0 && light.right == 0) {
        return;
    }
    if (stacked) {
        uint16_t near = light.left > light.right ? light.left : light.right;
        light = (QuadLight){near, near};
    }
    Image image;
    if (cover_image(flow, position, &image)) {
        uint32_t started_us = hal_clock_microseconds();
        image_draw_quad_lit(canvas, image, quad, light);
        uint32_t reflection_us = hal_clock_microseconds();
        draw_profile_add(DRAW_PART_COVERS, reflection_us - started_us);
        if (stacked) {
            return;
        }
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
    bool stacked = theme_is_portrait(theme);
    int title_top = stacked ? STACK_TITLE_TOP : TITLE_TOP;
    int artist_top = stacked ? STACK_ARTIST_TOP : ARTIST_TOP;
    draw_line(flow, canvas, (Label){{padding, title_top, width, LINE_HEIGHT}, flow->title}, title);
    draw_line(flow, canvas, (Label){{padding, artist_top, width, LINE_HEIGHT}, flow->artist},
              artist);
}

static void draw_empty(const Theme *theme, Canvas *canvas)
{
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    Rect screen = {0, 0, theme->metrics.screen_width, theme->metrics.screen_height};
    const char *message = i18n_text(STR_MENU_NOTHING_HERE);
    label_draw(canvas, style, (Label){label_centered_area(style.font, screen, message), message});
}

static void draw_backdrop(const Theme *theme, Canvas *canvas)
{
    int floor = theme_is_portrait(theme) ? STACK_FLOOR_TOP : LAYOUT.center_top + LAYOUT.center_size;
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

Screen *cover_flow_screen_open_all(CoverFlowScreen *flow)
{
    const uint8_t *options = flow->config.options;
    bool by_title = options != NULL && options[OPTION_COVER_ORDER] != 0;
    CoverFlowOrder titles = {.first_link = 0,
                             .count = library_music_count(flow->config.library, LIBRARY_ALBUMS),
                             .linked = false};
    use_order(flow, by_title ? titles : flow->artist_order,
              by_title ? ORDER_BY_TITLE : ORDER_BY_ARTIST, browse_root_query(BROWSE_ALBUMS), 0);
    return &flow->screen;
}

static void scan_link(PlaylistScan *scan, uint32_t link, uint16_t album)
{
    if (scan->runs > 0 && album == scan->previous) {
        return;
    }
    scan->runs++;
    scan->previous = album;
    if (scan->playing_track != LIBRARY_NONE && scan->playing.position == NO_POSITION &&
        album == scan->playing_album) {
        scan->playing = (CoverFlowTile){(int)scan->runs - 1, album, album, link};
    }
}

static PlaylistScan scan_start(const CoverFlowScreen *flow)
{
    PlaylistScan scan = {.playing_track = LIBRARY_NONE,
                         .playing_album = LIBRARY_NONE,
                         .previous = LIBRARY_NONE,
                         .runs = 0,
                         .playing = {NO_POSITION, LIBRARY_NONE, LIBRARY_NONE, 0}};
    PlayerStatus status = player_status(flow->config.player);
    LibraryTrack track;
    if (status.has_track && library_read_track(flow->config.library, status.track, &track)) {
        scan.playing_track = status.track;
        scan.playing_album = track.album;
    }
    return scan;
}

static PlaylistScan playlist_scan(const CoverFlowScreen *flow, CoverFlowOrder order)
{
    PlaylistScan scan = scan_start(flow);
    for (uint32_t step = 0; step < order.links; step++) {
        uint32_t link = order.first_link + step;
        scan_link(&scan, link, link_album(flow->config.library, link));
        if (step % LINKS_PER_AUDIO_SERVICE == LINKS_PER_AUDIO_SERVICE - 1) {
            hal_audio_service();
        }
    }
    return scan;
}

static Screen *open_playlist(CoverFlowScreen *flow, CoverFlowOrder order, uint16_t subject,
                             int album_level)
{
    PlaylistScan scan = playlist_scan(flow, order);
    BrowseQuery parent = {.kind = BROWSE_PLAYLIST_TRACKS,
                          .group = LIBRARY_PLAYLISTS,
                          .subject = subject,
                          .artist = LIBRARY_NONE};
    order.kind = COVER_FLOW_PLAYLIST;
    order.count = scan.runs;
    use_order(flow, order, (int32_t)LIBRARY_PLAYLISTS << 16 | subject, parent, album_level);
    flow->playing_track = scan.playing_track;
    flow->playing_tile = scan.playing;
    if (scan.playing.position != NO_POSITION) {
        flow->playing_tile.position += flow->first;
    }
    return &flow->screen;
}

static Screen *open_folder(CoverFlowScreen *flow, uint16_t folder, int album_level)
{
    CoverFlowOrder order;
    if (!folder_order(flow->config.library, folder, &order)) {
        return NULL;
    }
    flow->origin = folder;
    use_order(flow, order, folder_key(folder), browse_root_query(BROWSE_FOLDER), album_level);
    return &flow->screen;
}

Screen *cover_flow_screen_open_group(CoverFlowScreen *flow, LibrarySection group, uint16_t subject,
                                     int album_level)
{
    LibraryGroup record;
    if (group == LIBRARY_FOLDERS) {
        return open_folder(flow, subject, album_level);
    }
    if (!library_read_group(flow->config.library, group, subject, &record) ||
        record.link_count == 0) {
        return NULL;
    }
    CoverFlowOrder order = {.first_link = record.first_link,
                            .count = record.link_count,
                            .linked = true,
                            .kind = COVER_FLOW_ALBUMS,
                            .links = record.link_count,
                            .folder = LIBRARY_NONE};
    if (group == LIBRARY_PLAYLISTS) {
        return open_playlist(flow, order, subject, album_level);
    }
    BrowseQuery parent = {
        .kind = BROWSE_GROUP_ALBUMS, .group = group, .subject = subject, .artist = LIBRARY_NONE};
    use_order(flow, order, (int32_t)group << 16 | subject, parent, album_level);
    return &flow->screen;
}

Screen *cover_flow_screen_init(CoverFlowScreen *flow, CoverFlowConfig config)
{
    flow->config = config;
    flow->artist_order = cover_flow_order_make(config.library);
    flow->order_key = ORDER_BY_ARTIST;
    flow->origin = 0;
    flow->held_repeats = 0;
    use_order(flow, flow->artist_order, ORDER_BY_ARTIST, browse_root_query(BROWSE_ALBUMS), 0);
    flow->target = 0;
    flow->position = 0;
    flow->positioned = false;
    flow->text_position = NO_POSITION;
    flow->title[0] = '\0';
    flow->artist[0] = '\0';
    flow->screen = (Screen){
        .title = i18n_text(STR_MENU_COVER_FLOW),
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
