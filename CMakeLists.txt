cmake_minimum_required(VERSION 3.20)
project(RefleksExternal VERSION 3.0)

set(CMAKE_CXX_STANDARD 17)

# Windows subsystem (konsol penceresi açılmasın)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# ImGui kaynak dosyaları
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_win32.cpp
    imgui/imgui_impl_dx11.cpp
)

add_executable(RefleksExternal WIN32
    main.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(RefleksExternal PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/imgui
)

target_link_libraries(RefleksExternal PRIVATE
    d3d11
    dwmapi
    dxgi
)
