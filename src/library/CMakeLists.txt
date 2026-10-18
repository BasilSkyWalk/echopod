add_library(echo_library STATIC
    block_cache.c
    covers.c
    favorites.c
    library.c
    library_browse.c
    sort_fold_table.c
    sort_key.c
    tags_ape.c
    tags_asf.c
    tags_genres.c
    tags_id3.c
    tags_mp4.c
    tags_mpeg.c
    tags_riff.c
    tags_vorbis.c
    track_tags.c
)
target_include_directories(echo_library PUBLIC include)
target_link_libraries(echo_library
    PUBLIC echo_core
    PRIVATE echo_options
)
