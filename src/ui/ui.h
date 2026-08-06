#pragma once

#include "editor.h"

namespace ui
{
    // call after C2D_Init(), before the first draw()
    void init();
    void exit();

    // draws the toolbars and, in the texture workspace, the paint canvas (from
    // the shared texture). must run inside a 2D scene (C2D_Prepare +
    // C2D_SceneBegin). viewport contents are citro3d (Editor::renderViewports).
    void draw(Editor& editor, C3D_Tex* texture);

    // top-screen overlay: project name + unsaved dot. run inside a 2D scene
    // bound to the top target.
    void drawTop(Editor& editor);
}
