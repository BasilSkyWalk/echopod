add_library(echo_fonts STATIC fonts.c)
target_include_directories(echo_fonts PUBLIC include)
target_link_libraries(echo_fonts
    PUBLIC echo_gfx
    PRIVATE echo_options
)

file(READ ${PROJECT_SOURCE_DIR}/assets/fonts/glyph_pack.json ECHO_GLYPH_PACK_CONFIG)
string(JSON ECHO_GLYPH_FACE_COUNT LENGTH "${ECHO_GLYPH_PACK_CONFIG}" faces)
math(EXPR ECHO_GLYPH_LAST_FACE "${ECHO_GLYPH_FACE_COUNT} - 1")
set(ECHO_GLYPH_FACE_FILES "")
foreach(face RANGE ${ECHO_GLYPH_LAST_FACE})
    string(JSON face_name GET "${ECHO_GLYPH_PACK_CONFIG}" faces ${face} name)
    list(APPEND ECHO_GLYPH_FACE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/packs/${face_name}.bin)
endforeach()
set(ECHO_GLYPH_FACE_FILES "${ECHO_GLYPH_FACE_FILES}" CACHE INTERNAL "glyph pack faces in face order")
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/assets/fonts/glyph_pack.json
)
