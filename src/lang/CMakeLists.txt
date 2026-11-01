file(READ ${PROJECT_SOURCE_DIR}/assets/lang/languages.json ECHO_LANGUAGE_CONFIG)
string(JSON ECHO_LANGUAGE_TOTAL LENGTH "${ECHO_LANGUAGE_CONFIG}" languages)
math(EXPR ECHO_LANGUAGE_LAST "${ECHO_LANGUAGE_TOTAL} - 1")
set(ECHO_LANGUAGE_PACK_FILES "")
foreach(language RANGE 0 ${ECHO_LANGUAGE_LAST})
    string(JSON language_code GET "${ECHO_LANGUAGE_CONFIG}" languages ${language} code)
    list(APPEND ECHO_LANGUAGE_PACK_FILES ${CMAKE_CURRENT_SOURCE_DIR}/packs/${language_code}.bin)
endforeach()
set(ECHO_LANGUAGE_PACK_FILES "${ECHO_LANGUAGE_PACK_FILES}" CACHE INTERNAL "language packs in language order")
set(ECHO_LANGUAGE_TOTAL "${ECHO_LANGUAGE_TOTAL}" CACHE INTERNAL "languages including English")
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/assets/lang/languages.json
)
