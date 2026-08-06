#pragma once

#include "scene.h"
#include "viewport.h"
#include "renderer.h"

// draws the scene into the three ortho viewports: grid, optional faces, mesh
// edges (selected objects highlighted), and the sub-object selection highlight
// for the current Edit level (verts / edges / faces).
namespace viewrender
{
    void render(const Scene& scene, const Viewport viewports[3], EditMode mode,
                SubLevel subLevel, bool extruding, bool showFaces, int onlyView,
                Renderer& r);
}
