#pragma once

#include <3ds.h>

// order must match the atlas (ICON_PNGS in CMakeLists.txt / gen_icons.sh)
enum Icon
{
    Icon_Menu = 0,
    Icon_Plus,
    Icon_Trash,
    Icon_Undo,
    Icon_Redo,
    Icon_Vertex,
    Icon_Move,
    Icon_Rotate,
    Icon_Scale,
    Icon_Box,
    Icon_Eye,
    Icon_Flip,
    Icon_Circle,
    Icon_Pyramid,
    Icon_Cylinder,
    Icon_Square,
    Icon_Save,
    Icon_Load,
    Icon_Exit,
    Icon_Paint,
    Icon_Image,
    Icon_Bucket,
    Icon_Pipette,
    Icon_Frame,
    Icon_Fit,
    Icon_Texture,
    Icon_Layout,
    Icon_Edge,
    Icon_Pencil,
    Icon_Extrude,
    Icon_Subdivide,
    Icon_Split,
    Icon_Shade,
    Icon_Minimize,
    Icon_Eraser,
    Icon_More,
    Icon_Count,
};

namespace icons
{
    // load the embedded atlas, call after C2D_Init()
    bool init();
    void exit();

    // draw icon in a size x size box at top-left (x,y), tinted color 0xRRGGBBAA
    void draw(Icon ic, float x, float y, float size, u32 color);
}
