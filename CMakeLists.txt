cmake_minimum_required(VERSION 3.22)

project(alterecho VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE)

juce_add_gui_app(alterecho
    PRODUCT_NAME "alterecho"
    COMPANY_NAME "alterecho"
)

target_sources(alterecho
    PRIVATE
        source/main.cpp
)

target_compile_definitions(alterecho
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(alterecho
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
)