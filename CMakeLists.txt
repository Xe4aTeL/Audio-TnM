cmake_minimum_required(VERSION 3.15)
project(AudioTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# wxWidgets
set(wxWidgets_DIR "${CMAKE_SOURCE_DIR}/external_libs/wxWidgets/lib/cmake/wxWidgets-3.2")
find_package(wxWidgets 3.2.12 REQUIRED CONFIG COMPONENTS base core)

# FFMPEG
set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/external_libs/ffmpeg")

# Whisper.cpp
set(ggml_DIR "${CMAKE_SOURCE_DIR}/external_libs/whisper-cpp/lib/cmake/ggml")
find_package(ggml REQUIRED CONFIG)
set(whisper_DIR "${CMAKE_SOURCE_DIR}/external_libs/whisper-cpp/lib/cmake/whisper")
find_package(whisper REQUIRED CONFIG)

# libsodium
set(libsodium_DIR "${CMAKE_SOURCE_DIR}/external_libs/libsodium")
set(libsodium_INCLUDE_DIR "${libsodium_DIR}/include")
set(libsodium_LIBRARY "${libsodium_DIR}/x64/Debug/v145/dynamic")

set(SOURCES
    main.cpp
    gui/gui.cpp
    utils/audio_utils.cpp
    utils/text_utils.cpp
    utils/audio_player.cpp
    methods/transcribe/transcription.cpp
    methods/modify/modify.cpp
)

# WIN32 prevents a terminal window from opening in the background on Windows
# MACOSX_BUNDLE packages it correctly as a .app on macOS <-- unchecked
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${wxWidgets_INCLUDE_DIRS}
    ${WHISPER_INCLUDE_DIR}
    "${FFMPEG_DIR}/include"
    ${libsodium_INCLUDE_DIR}
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${wxWidgets_LIBRARIES}
    ${whisper_LIBRARY}
    ${GGML_LIBRARY}
    #/bin bc configure of FFMPEG puts them there when compiled for Windows
    "${FFMPEG_DIR}/bin/*.lib"
    "${libsodium_LIBRARY}/libsodium.lib"
)

if(WIN32)
    # FFMPEG
    file(GLOB FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    # Whisper.cpp
    set(w_DIR "${CMAKE_SOURCE_DIR}/external_libs/whisper-cpp/")
    file(GLOB WHISPER_DLLS "${w_DIR}/bin/*.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WHISPER_DLLS}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )

    # wxWidgets
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )

    # libsodium
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${libsodium_LIBRARY}/libsodium.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()
