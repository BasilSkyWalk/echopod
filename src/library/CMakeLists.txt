add_library(echo_library STATIC
    block_cache.c
    covers.c
    favorites.c
    library.c
    library_browse.c
    music_fingerprint.c
    sort_initial.c
)
target_include_directories(echo_library PUBLIC include)
target_link_libraries(echo_library
    PUBLIC echo_core
    PRIVATE echo_options
)

add_library(echo_library_scan STATIC
    cover_build.c
    library_builder.c
    library_builder_encode.c
    library_builder_model.c
    library_builder_paths.c
    library_scan.c
    library_scan_entry.c
    playlist_reader.c
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
    track_fields.c
    track_tags.c
)
target_include_directories(echo_library_scan PUBLIC include)
target_link_libraries(echo_library_scan
    PUBLIC echo_core echo_image echo_library
    PRIVATE echo_options
)
