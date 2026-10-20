#set(APP_SOURCES
#	kotte/src/main.cpp
#)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/kotte/src/*.cpp"
)
