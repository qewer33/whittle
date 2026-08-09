#pragma once

#include "scene.h"
#include "viewport.h"
#include "renderer.h"

// draws the scene into the three ortho viewports: grid, optional faces, mesh
// edges (selected highlighted), and the current Edit-level selection.
namespace viewrender
{
    void render(const Scene& scene, const Viewport viewports[3], EditMode mode,
                SubLevel subLevel, bool extruding, bool showFaces, int onlyView,
                float gridSpacing, Renderer& r);
}
