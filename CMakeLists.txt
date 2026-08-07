cmake_minimum_required(VERSION 3.16)
project(whittle LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_compile_options(-Wall -Wextra -O2 -fno-rtti -fno-exceptions)

add_executable(whittle
    src/main.cpp
    src/engine/camera.cpp
    src/engine/mesh.cpp
    src/engine/renderer.cpp
    src/engine/viewport.cpp
    src/core/editor.cpp
    src/core/textureeditor.cpp
    src/core/scene.cpp
    src/core/viewrender.cpp
    src/core/platform.cpp
    src/core/projectbrowser.cpp
    src/core/meshexport.cpp
    src/ui/icons.cpp
    src/ui/ui.cpp
    src/ui/uidraw.cpp
    src/ui/widgets/menu.cpp
    src/ui/widgets/button.cpp
)

ctr_add_shader_library(
    shader_main
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/main.shbin
    src/engine/shaders/main.v.pica
)

dkp_add_embedded_binary_library(shader_bin shader_main)

# Toolbar icon atlas: pack the Lucide PNGs into a t3x (order = Icon enum in
# src/icons.h) and embed it as a binary, the same way as the shader.
find_program(TEX3DS tex3ds REQUIRED)
set(ICON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons)
set(ICON_PNGS
    ${ICON_DIR}/00_menu.png ${ICON_DIR}/01_plus.png ${ICON_DIR}/02_trash.png
    ${ICON_DIR}/03_undo.png ${ICON_DIR}/04_redo.png ${ICON_DIR}/05_vertex.png
    ${ICON_DIR}/06_move.png ${ICON_DIR}/07_rotate.png ${ICON_DIR}/08_scale.png
    ${ICON_DIR}/09_box.png ${ICON_DIR}/10_eye.png ${ICON_DIR}/11_flip.png
    ${ICON_DIR}/12_circle.png ${ICON_DIR}/13_pyramid.png ${ICON_DIR}/14_cylinder.png
    ${ICON_DIR}/15_square.png ${ICON_DIR}/16_save.png ${ICON_DIR}/17_load.png
    ${ICON_DIR}/18_exit.png ${ICON_DIR}/19_paint.png ${ICON_DIR}/20_image.png
    ${ICON_DIR}/21_bucket.png ${ICON_DIR}/22_pipette.png ${ICON_DIR}/23_frame.png
    ${ICON_DIR}/24_fit.png ${ICON_DIR}/25_texture.png ${ICON_DIR}/26_layout.png
    ${ICON_DIR}/27_edge.png ${ICON_DIR}/28_pencil.png ${ICON_DIR}/29_extrude.png
    ${ICON_DIR}/30_subdivide.png ${ICON_DIR}/31_split.png ${ICON_DIR}/32_shade.png
    ${ICON_DIR}/33_minimize.png ${ICON_DIR}/34_eraser.png ${ICON_DIR}/35_more.png
    ${ICON_DIR}/36_back.png ${ICON_DIR}/37_export.png
)
set(ICON_T3X ${CMAKE_CURRENT_BINARY_DIR}/icons.t3x)
add_custom_command(
    OUTPUT ${ICON_T3X}
    COMMAND ${TEX3DS} --atlas -z auto -o ${ICON_T3X} ${ICON_PNGS}
    DEPENDS ${ICON_PNGS}
    COMMENT "Building icon atlas (icons.t3x)"
)
add_custom_target(icon_atlas DEPENDS ${ICON_T3X})
dkp_set_target_file(icon_atlas ${ICON_T3X})
dkp_add_embedded_binary_library(icons_bin icon_atlas)

target_link_libraries(whittle PRIVATE
    shader_bin
    icons_bin
    -lcitro2d
    -lcitro3d
    ${CTR_STANDARD_LIBRARIES}
)
# bare includes ("mesh.h", "scene.h", ...) resolve across the folders
target_include_directories(whittle PRIVATE
    ${CTR_STANDARD_INCLUDE_DIRECTORIES}
    src/engine
    src/core
    src/ui
)

ctr_generate_smdh(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/whittle.smdh
    NAME "whittle"
    DESCRIPTION "Low-poly 3D modeling for 3DS"
    AUTHOR "qewer"
)

ctr_create_3dsx(whittle SMDH ${CMAKE_CURRENT_BINARY_DIR}/whittle.smdh)
