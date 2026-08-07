#include "editor.h"
#include "viewrender.h"
#include "platform.h"
#include "meshexport.h"
#include <algorithm>
#include <math.h>
#include <string>

using namespace widgets;

static float snapToGrid(float v, float grid)
{
    return roundf(v / grid) * grid;
}

// world point to screen pixel (inverse of Viewport::tapToWorld)
static void worldToScreen(const Viewport& vp, const Vec3& p, float& sx, float& sy)
{
    const float fsx = vp.flipped ? -1.0f : 1.0f;
    sx = (vp.x + vp.w / 2) + fsx * (getAxis(p, vp.axisX) - vp.centerX) * vp.scale;
    sy = (vp.y + vp.h / 2) - (getAxis(p, vp.axisY) - vp.centerY) * vp.scale;
}

// screen pixel to world
static void viewportWorld(const Viewport& vp, int px, int py, float& wx, float& wy)
{
    const float fsx = vp.flipped ? -1.0f : 1.0f;
    wx = vp.centerX + fsx * (px - (vp.x + vp.w / 2)) / vp.scale;
    wy = vp.centerY - (py - (vp.y + vp.h / 2)) / vp.scale;
}

static bool pointInTri(float px, float py, float ax, float ay, float bx,
                       float by, float cx, float cy)
{
    // reject zero-area triangles
    const float area2 = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (fabsf(area2) < 1.0f)
        return false;

    const float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    const float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    const float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos); // inside if all same sign (winding agnostic)
}

Editor::Editor()
{
    // two bars top and bottom, viewports fill the width between them
    const int TB = layout::kBarH, BB = layout::kBarH;
    const int vh = (240 - TB - BB) / 2;
    const float scale = 25.0f;
    viewports[0] = {0, TB, 320, vh, 0, 2, 0.0f, 0.0f, scale, false};       // top: X right, Z up
    viewports[1] = {0, TB + vh, 160, vh, 0, 1, 0.0f, 0.0f, scale, false};  // front: X right, Y up
    viewports[2] = {160, TB + vh, 160, vh, 2, 1, 0.0f, 0.0f, scale, false};// side: Z right, Y up
    for (int i = 0; i < 3; i++)
        baseView[i] = {viewports[i].x, viewports[i].y, viewports[i].w, viewports[i].h};

    // top bar: 3D/2D workspace switch (left), add, del, undo, redo cluster,
    // view toggles + hamburger menu (right)
    const int bw = 30, ws = 52;
    btnWorkspace = {0, 0, ws, TB}; // labeled switch, top-left corner
    btnAdd = {ws + 4, 0, bw, TB};
    btnDel = {ws + 4 + bw, 0, bw, TB};
    btnUndo = {ws + 8 + 2 * bw, 0, bw, TB};
    btnRedo = {ws + 8 + 3 * bw, 0, bw, TB};
    btnMenu = {320 - bw, 0, bw, TB};        // hamburger, top-right corner
    btnView = {320 - 2 * bw - 4, 0, bw, TB}; // view toggles popup (3D workspace)

    // bottom bar: mode button (icon+text) left, the tool switch centered
    const int by = 240 - BB;
    btnMode = {0, by, 84, BB};
    // edit/face verbs, right side: subdivide, extrude
    btnExtrude = {320 - bw, by, bw, BB};
    btnSubdivide = {320 - 2 * bw - 4, by, bw, BB};
    // edit/edge verb, right side: split
    btnSplit = {320 - bw, by, bw, BB};
    // paint/texture: the active-color button (opens the picker), far right
    btnColor = {320 - bw, by, bw, BB};
    // texture mode: overflow menu (texture all / untexture all), far right
    btnTexMenu = {320 - bw, by, bw, BB};

    // toolbar popups. each anchors to its button and lays itself out.
    modeMenu.setup(btnMode, Placement::Above, Align::Start, 90,
                   {{Icon_Box, "Object"}, {Icon_Pencil, "Edit"}, {Icon_Paint, "Paint"},
                    {Icon_Texture, "Texture"}});
    shapeMenu.setup(btnAdd, Placement::Below, Align::Start, 90,
                    {{Icon_Box, "Cube"}, {Icon_Circle, "Sphere"}, {Icon_Pyramid, "Pyramid"},
                     {Icon_Cylinder, "Cylinder"}, {Icon_Square, "Plane"}});
    fileMenu.setup(btnMenu, Placement::Below, Align::End, 90,
                   {{Icon_Save, "Save"}, {Icon_Load, "Load"}, {Icon_Export, "Export"},
                    {Icon_Exit, "Exit"}});
    exportMenu.setup(fileMenu.itemRect(2), Placement::LeftOf, Align::Start, 90,
                     {{Icon_Export, "OBJ"}, {Icon_Export, "STL"}});
    viewMenu.setup(btnView, Placement::Below, Align::End, 90,
                   {{Icon_Box, "Wireframe"}, {Icon_Square, "Faces"}, {Icon_Flip, "Flip"},
                    {Icon_Shade, "Shading"}},
                   false); // toggles: stay open on pick
    texActionMenu.setup(btnTexMenu, Placement::Above, Align::End, 112,
                        {{Icon_Texture, "Texture All"}, {Icon_Eraser, "Untexture All"}});

    // per-mode tool switches (centered pill in the bottom bar)
    transformSwitch.setup({Icon_Move, Icon_Rotate, Icon_Scale});
    subLevelSwitch.setup({Icon_Vertex, Icon_Edge, Icon_Square});
    paintSwitch.setup({Icon_Paint, Icon_Pipette});
    texSwitch.setup({Icon_Texture, Icon_Eraser});
}

void Editor::setStatus(const char* m)
{
    statusMsg = m;
    statusTime = 1.6f;
}

void Editor::serviceFileOps()
{
    const FileOp op = pendingFileOp;
    pendingFileOp = FileOp::None;
    if (op == FileOp::Save)
    {
        if (scene.projectPath.empty()) // untitled: prompt for a name first
        {
            std::string name;
            if (platform::inputText("Project name", scene.projectName.c_str(), name))
                setStatus(scene.saveAs(name) ? "Saved" : "Save failed");
            else
                setStatus("Cancelled");
        }
        else
            setStatus(scene.save() ? "Saved" : "Save failed");
    }
    else if (op == FileOp::Load)
    {
        browser.open();
        screen = AppScreen::Browser;
    }
    else if (op == FileOp::ExportObj || op == FileOp::ExportStl)
    {
        // export under the project name, prompt for one if still untitled
        std::string name = scene.projectName;
        bool ok = !name.empty();
        if (!ok)
            ok = platform::inputText("Export name", "", name);
        if (ok)
        {
            const bool r = op == FileOp::ExportObj ? meshexport::exportObj(scene, name)
                                                   : meshexport::exportStl(scene, name);
            setStatus(r ? (op == FileOp::ExportObj ? "Exported OBJ" : "Exported STL") : "Export failed");
        }
        else
            setStatus("Cancelled");
    }
}

void Editor::deleteSelected()
{
    if (mode == EditMode::Object)
        scene.deleteSelectedObjects();
    else if (mode == EditMode::Edit)
    {
        if (subLevel == SubLevel::Vertex)
            scene.deleteSelectedVerts();
        else if (subLevel == SubLevel::Edge)
            scene.deleteSelectedEdges();
        else
            scene.deleteSelectedFaces();
    }
}

// maximize view i to fill the ortho area, or restore the three-up layout
void Editor::toggleMax(int i)
{
    const bool wasMax = (maxView == i);
    if (maxView >= 0) // restore whatever is currently maximized
    {
        viewports[maxView].x = baseView[maxView].x;
        viewports[maxView].y = baseView[maxView].y;
        viewports[maxView].w = baseView[maxView].w;
        viewports[maxView].h = baseView[maxView].h;
    }
    maxView = wasMax ? -1 : i;
    if (maxView >= 0) // fill the area between the two bars
    {
        const Rect c = layout::content();
        viewports[maxView].x = c.x;
        viewports[maxView].y = c.y;
        viewports[maxView].w = c.w;
        viewports[maxView].h = c.h;
    }
}

Viewport* Editor::viewportAt(int px, int py)
{
    if (maxView >= 0)
        return viewports[maxView].contains(px, py) ? &viewports[maxView] : nullptr;
    for (int i = 0; i < 3; i++)
        if (viewports[i].contains(px, py))
            return &viewports[i];
    return nullptr;
}

bool Editor::pickVertexAny(const Viewport& vp, int px, int py, int& outObj, int& outVert)
{
    float wx, wy;
    viewportWorld(vp, px, py, wx, wy);

    int bestObj = -1, bestVert = -1;
    float bestDist = kSelectRadiusPx * kSelectRadiusPx;
    for (int o = 0; o < (int)scene.objects.size(); o++)
    {
        const Mesh& m = scene.objects[o];
        for (int i = 0; i < (int)m.positions.size(); i++)
        {
            const Vec3& p = m.positions[i];
            const float dx = (getAxis(p, vp.axisX) - wx) * vp.scale;
            const float dy = (getAxis(p, vp.axisY) - wy) * vp.scale;
            const float d = dx * dx + dy * dy;
            if (d < bestDist)
            {
                bestDist = d;
                bestObj = o;
                bestVert = i;
            }
        }
    }
    outObj = bestObj;
    outVert = bestVert;
    return bestVert >= 0;
}

// shortest distance from a point to a segment, screen space
static float distToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay;
    const float wx = px - ax, wy = py - ay;
    const float len2 = vx * vx + vy * vy;
    float t = (len2 > 1e-6f) ? (wx * vx + wy * vy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float cx = ax + t * vx, cy = ay + t * vy;
    const float dx = px - cx, dy = py - cy;
    return sqrtf(dx * dx + dy * dy);
}

// nearest edge (a face-border segment) to the tap, returns its two vert indices
bool Editor::pickEdge(const Viewport& vp, int px, int py, int& outObj, int& outV0, int& outV1)
{
    float bestD = 10.0f; // px
    int bO = -1, bV0 = -1, bV1 = -1;
    for (int o = 0; o < (int)scene.objects.size(); o++)
    {
        const Mesh& m = scene.objects[o];
        for (const Face& f : m.faces)
            for (int k = 0; k < 4; k++)
            {
                const int a = f.indices[k], b = f.indices[(k + 1) & 3];
                if (a == b) continue; // degenerate edge (tri stored as a quad)
                float ax, ay, bx, by;
                worldToScreen(vp, m.positions[a], ax, ay);
                worldToScreen(vp, m.positions[b], bx, by);
                const float d = distToSegment((float)px, (float)py, ax, ay, bx, by);
                if (d < bestD) { bestD = d; bO = o; bV0 = a; bV1 = b; }
            }
    }
    outObj = bO;
    outV0 = bV0;
    outV1 = bV1;
    return bO >= 0;
}

// shoelace, sign tells winding, front-facing quads come out positive here
static float quadSignedArea(const float* sx, const float* sy)
{
    float a = 0.0f;
    for (int k = 0; k < 4; k++)
    {
        const int n = (k + 1) & 3;
        a += sx[k] * sy[n] - sx[n] * sy[k];
    }
    return a;
}

bool Editor::pickFace(const Viewport& vp, int px, int py, int& outObj, int& outFace)
{
    // search back-to-front (later objects draw on top). ortho has no depth, so
    // resolve front/back overlap by winding, not mesh order.
    for (int o = (int)scene.objects.size() - 1; o >= 0; o--)
    {
        const Mesh& m = scene.objects[o];
        int bestFace = -1;
        float bestArea = 0.0f;
        for (int fi = 0; fi < (int)m.faces.size(); fi++)
        {
            const Face& f = m.faces[fi];
            float sx[4], sy[4];
            for (int k = 0; k < 4; k++)
                worldToScreen(vp, m.positions[f.indices[k]], sx[k], sy[k]);
            if (pointInTri((float)px, (float)py, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]) ||
                pointInTri((float)px, (float)py, sx[0], sy[0], sx[2], sy[2], sx[3], sy[3]))
            {
                const float area = quadSignedArea(sx, sy);
                if (bestFace < 0 || area > bestArea)
                {
                    bestFace = fi;
                    bestArea = area;
                }
            }
        }
        if (bestFace >= 0)
        {
            outObj = o;
            outFace = bestFace;
            return true;
        }
    }
    return false;
}

int Editor::pickObject(const Viewport& vp, int px, int py)
{
    int o, f;
    return pickFace(vp, px, py, o, f) ? o : -1;
}

void Editor::syncZoom(float cameraDistance)
{
    if (cameraDistance < 0.01f)
        cameraDistance = 0.01f;
    const float scale = kZoomScale / cameraDistance;
    for (int i = 0; i < 3; i++)
        viewports[i].scale = scale;
}

void Editor::handleKeys(u32 kDown)
{
    if (screen == AppScreen::Browser)
        return;
    if (kDown & KEY_X)
        wireframe = !wireframe;
    if (kDown & KEY_Y)
        deleteSelected();
    if (kDown & KEY_B)
        scene.undo();
    if (kDown & KEY_A)
        showFaces = !showFaces;
    if (kDown & (KEY_DLEFT | KEY_DRIGHT))
    {
        flipViews = !flipViews;
        applyFlip();
    }
}

void Editor::applyFlip()
{
    for (int i = 0; i < 3; i++)
        viewports[i].flipped = flipViews;
}

void Editor::handleTouchDown(int px, int py)
{
    if (screen == AppScreen::Browser) { browser.handleTouchDown(px, py); return; }
    touching = true;
    pressInViewport = false;
    pressPx = px;
    pressPy = py;
    dragMoved = false;
    dragSnapshotted = false;
    pressedObject = -1;
    pressedObjWasSelected = false;
    draggingObjects = false;
    objDragViewport = -1;
    pressedVertObj = pressedVertIdx = -1;
    pressedVertWasSelected = false;
    pressedFaceObj = pressedFaceIdx = -1;
    pressedFaceWasSelected = false;
    pressedEdgeObj = pressedEdgeV0 = pressedEdgeV1 = -1;
    pressedEdgeWasSelected = false;
    pressedSub = false;
    extruding = false; // re armed below by the extrude button / grab consume
    draggingSub = false;
    subDragViewport = -1;
    panning = false;
    panViewport = -1;

    // toolbar popups: dispatch the tap to whichever menu is open, act on the
    // picked row. exportMenu is the file menu's flyout, so it eats first.
    if (exportMenu.open)
    {
        const int r = exportMenu.handle(px, py);
        if (r == 0) pendingFileOp = FileOp::ExportObj;
        else if (r == 1) pendingFileOp = FileOp::ExportStl;
        fileMenu.open = false; // flyout ended, close its parent too
        return;
    }
    if (fileMenu.open)
    {
        const int r = fileMenu.handle(px, py);
        if (r == 0) pendingFileOp = FileOp::Save;
        else if (r == 1) pendingFileOp = FileOp::Load;
        else if (r == 2) { fileMenu.open = true; exportMenu.open = true; } // keep parent, fly out
        else if (r == 3) wantQuit = true;
        return;
    }
    if (modeMenu.open)
    {
        const int r = modeMenu.handle(px, py);
        if (r >= 0) mode = (EditMode)r;
        return;
    }
    if (shapeMenu.open)
    {
        const int r = shapeMenu.handle(px, py);
        if (r >= 0)
        {
            const int idx = scene.addShape(r);
            if (idx >= 0 && mode == EditMode::Object)
                scene.selectedObjects.push_back(idx);
        }
        return;
    }
    if (viewMenu.open) // toggles: stay open, flip the picked flag
    {
        const int r = viewMenu.handle(px, py);
        if (r == 0) wireframe = !wireframe;
        else if (r == 1) showFaces = !showFaces;
        else if (r == 2) { flipViews = !flipViews; applyFlip(); }
        else if (r == 3) shading = !shading;
        return;
    }
    if (texActionMenu.open)
    {
        const int r = texActionMenu.handle(px, py);
        if (r >= 0)
        {
            scene.snapshot();
            if (r == 0) { scene.textureAllFaces(); tex.autoLayout(); }
            else scene.untextureAllFaces();
        }
        return;
    }
    if (tex.texModeMenu.open)
    {
        const int r = tex.texModeMenu.handle(px, py);
        if (r >= 0) tex.texMode = (TexMode)r;
        return;
    }
    // color picker popup: presets, the SV square, the hue bar, tap-out closes
    if (colorPickerOpen)
    {
        for (int i = 0; i < kPaletteCount; i++)
            if (pickerSwatch(i).contains(px, py))
            {
                paintColor = kPalette[i];
                rgb2hsv(paintColor, pickerH, pickerS, pickerV);
                return;
            }
        const Rect sv = pickerSv();
        if (sv.contains(px, py))
        {
            pickerS = (float)(px - sv.x) / sv.w;
            pickerV = 1.0f - (float)(py - sv.y) / sv.h;
            paintColor = hsv2rgb(pickerH, pickerS, pickerV);
            pickingSv = true;
            return;
        }
        const Rect hue = pickerHue();
        if (hue.contains(px, py))
        {
            pickerH = (float)(py - hue.y) / hue.h;
            paintColor = hsv2rgb(pickerH, pickerS, pickerV);
            pickingHue = true;
            return;
        }
        if (!pickerPanel().contains(px, py))
            colorPickerOpen = false;
        return;
    }

    // top bar. in the 2D workspace + and delete become recenter and clear
    if (btnWorkspace.contains(px, py)) { workspace = is3D() ? Workspace::TwoD : Workspace::ThreeD; closeMenus(); tex.texModeMenu.open = false; return; }
    if (btnMenu.contains(px, py)) { closeMenus(); fileMenu.open = true; tex.texModeMenu.open = false; return; }
    if (btnAdd.contains(px, py))
    {
        if (is2D())
            tex.fitCanvas();
        else { closeMenus(); shapeMenu.open = true; tex.texModeMenu.open = false; }
        return;
    }
    if (btnDel.contains(px, py))
    {
        if (is2D())
            tex.clearSheet();
        else
            deleteSelected();
        return;
    }
    if (btnUndo.contains(px, py)) { scene.undo(); return; }
    if (btnRedo.contains(px, py)) { scene.redo(); return; }
    if (is3D() && btnView.contains(px, py)) { const bool willOpen = !viewMenu.open; closeMenus(); viewMenu.open = willOpen; return; }

    // active-color button (paint contexts) opens the picker
    if (colorActive() && btnColor.contains(px, py))
    {
        colorPickerOpen = true;
        tex.brushMenuOpen = false;
        rgb2hsv(paintColor, pickerH, pickerS, pickerV);
        return;
    }

    // bottom bar: texture tools handled entirely by the texture editor
    if (is2D())
    {
        tex.handleTouchDown(px, py);
        return;
    }

    // bottom bar: model controls
    if (btnMode.contains(px, py)) { closeMenus(); modeMenu.open = true; return; }
    // segmented tool switch, per mode
    if (mode == EditMode::Object)
    {
        const int c = transformSwitch.handle(px, py);
        if (c >= 0) { transformTool = (TransformTool)c; return; }
    }
    else if (mode == EditMode::Edit)
    {
        const int c = subLevelSwitch.handle(px, py);
        if (c >= 0) { subLevel = (SubLevel)c; return; }
    }
    else if (mode == EditMode::Paint)
    {
        const int c = paintSwitch.handle(px, py);
        if (c >= 0) { paintTool = (PaintTool)c; return; }
    }
    else if (mode == EditMode::Texture)
    {
        const int c = texSwitch.handle(px, py);
        if (c >= 0) { faceTexTool = (FaceTexTool)c; return; }
    }

    // Edit/Face verb: extrude the selected faces (keeps them selected as caps,
    // and arms a grab so the next viewport drag pulls the caps out)
    if (mode == EditMode::Edit && subLevel == SubLevel::Face && btnExtrude.contains(px, py))
    {
        if (!scene.selectedFaces.empty())
        {
            scene.extrudeSelectedFaces();
            grabSelection = true;
            extruding = true;
            setStatus("Drag to extrude");
        }
        return;
    }
    if (mode == EditMode::Edit && subLevel == SubLevel::Face && btnSubdivide.contains(px, py))
    {
        scene.subdivideSelectedFaces();
        return;
    }
    if (mode == EditMode::Edit && subLevel == SubLevel::Edge && btnSplit.contains(px, py))
    {
        scene.splitSelectedEdges();
        return;
    }

    // Texture mode: open the overflow menu (texture all / untexture all)
    if (mode == EditMode::Texture && btnTexMenu.contains(px, py))
    {
        closeMenus();
        texActionMenu.open = true;
        return;
    }

    // maximize/restore button in the top corner of each visible ortho view
    if (maxView >= 0)
    {
        if (viewMaxBtn(viewports[maxView]).contains(px, py)) { toggleMax(maxView); return; }
    }
    else
        for (int i = 0; i < 3; i++)
            if (viewMaxBtn(viewports[i]).contains(px, py)) { toggleMax(i); return; }

    Viewport* vp = viewportAt(px, py);
    if (!vp)
        return;
    pressInViewport = true;
    const int vpIndex = (int)(vp - viewports);

    // extrude just ran: move the caps directly (edge-on here, so a re-pick fails),
    // reusing its snapshot for one undo.
    if (mode == EditMode::Edit && grabSelection)
    {
        grabSelection = false;
        pressedSub = true;
        extruding = true; // stays active through the pull drag
        subDragViewport = vpIndex;
        dragSnapshotted = true;
        return;
    }

    if (mode == EditMode::Paint)
    {
        int o, fi;
        if (pickFace(*vp, px, py, o, fi))
        {
            Face& f = scene.objects[o].faces[fi];
            if (paintTool == PaintTool::Brush)
            {
                scene.snapshot();
                f.textured = false;
                f.color = paintColor;
            }
            else // eyedropper: pick the exact face color
                paintColor = f.color;
            return;
        }
        // empty space pans (fall through)
    }
    else if (mode == EditMode::Texture)
    {
        int o, fi;
        if (pickFace(*vp, px, py, o, fi))
        {
            Face& f = scene.objects[o].faces[fi];
            if (faceTexTool == FaceTexTool::Texture)
            {
                if (!f.textured)
                {
                    scene.snapshot();
                    f.textured = true;
                    // default uv: whole sheet, V-flipped so it reads upright
                    f.uv[0][0] = 0; f.uv[0][1] = 1;
                    f.uv[1][0] = 1; f.uv[1][1] = 1;
                    f.uv[2][0] = 1; f.uv[2][1] = 0;
                    f.uv[3][0] = 0; f.uv[3][1] = 0;
                }
                // select this face as the UV-edit target (keeps its uvs)
                tex.uvObj = o;
                tex.uvFaceIdx = fi;
            }
            else if (f.textured) // untexture: revert to a flat color
            {
                scene.snapshot();
                f.textured = false;
            }
            return;
        }
        // empty space pans (fall through)
    }
    else if (mode == EditMode::Edit && subLevel == SubLevel::Vertex)
    {
        int o, v;
        if (pickVertexAny(*vp, px, py, o, v))
        {
            pressedVertObj = o;
            pressedVertIdx = v;
            pressedVertWasSelected = scene.isVertSelected(o, v);
            if (!pressedVertWasSelected)
                scene.selectedVerts.push_back({o, v}); // greedy add
            scene.activeObject = o;
            pressedSub = true;
            subDragViewport = vpIndex;
            return;
        }
        scene.selectedVerts.clear();
    }
    else if (mode == EditMode::Edit && subLevel == SubLevel::Edge)
    {
        int o, v0, v1;
        if (pickEdge(*vp, px, py, o, v0, v1))
        {
            pressedEdgeObj = o;
            pressedEdgeV0 = v0;
            pressedEdgeV1 = v1;
            pressedEdgeWasSelected = scene.isEdgeSelected(o, v0, v1);
            if (!pressedEdgeWasSelected)
                scene.selectedEdges.push_back({o, v0, v1}); // greedy add
            scene.activeObject = o;
            pressedSub = true;
            subDragViewport = vpIndex;
            return;
        }
        scene.selectedEdges.clear();
    }
    else if (mode == EditMode::Edit && subLevel == SubLevel::Face)
    {
        int o, fi;
        if (pickFace(*vp, px, py, o, fi))
        {
            pressedFaceObj = o;
            pressedFaceIdx = fi;
            pressedFaceWasSelected = scene.isFaceSelected(o, fi);
            if (!pressedFaceWasSelected)
                scene.selectedFaces.push_back({o, fi}); // greedy add
            scene.activeObject = o;
            pressedSub = true;
            subDragViewport = vpIndex;
            return;
        }
        scene.selectedFaces.clear();
    }
    else if (mode == EditMode::Object)
    {
        const int hitObj = pickObject(*vp, px, py);
        if (hitObj >= 0)
        {
            pressedObject = hitObj;
            pressedObjWasSelected = scene.isObjectSelected(hitObj);
            if (!pressedObjWasSelected)
                scene.selectedObjects.push_back(hitObj);
            scene.activeObject = hitObj;
            objDragViewport = vpIndex;
            return;
        }
        scene.selectedObjects.clear();
    }

    // empty space pans
    panning = true;
    panViewport = vpIndex;
    lastPanPx = px;
    lastPanPy = py;
}

void Editor::beginObjectDrag(const Viewport& vp)
{
    draggingObjects = true;
    viewportWorld(vp, pressPx, pressPy, dragStartWx, dragStartWy);
    objDragPivot = scene.selectionCentroid();
    objDragOrig.clear();
    for (int o : scene.selectedObjects)
        objDragOrig.push_back(scene.objects[o].positions);
}

void Editor::applyObjectDrag(const Viewport& vp, float wx, float wy)
{
    const int ax = vp.axisX, ay = vp.axisY;
    const float pa = getAxis(objDragPivot, ax);
    const float pb = getAxis(objDragPivot, ay);

    float dX = 0, dY = 0, c = 1, s = 0, sxr = 1, syr = 1;
    if (transformTool == TransformTool::Move)
    {
        dX = snapToGrid(wx - dragStartWx, kSnap);
        dY = snapToGrid(wy - dragStartWy, kSnap);
    }
    else if (transformTool == TransformTool::Rotate)
    {
        const float a0 = atan2f(dragStartWy - pb, dragStartWx - pa);
        const float a1 = atan2f(wy - pb, wx - pa);
        float d = a1 - a0;
        d = roundf(d / kRotSnap) * kRotSnap;
        c = cosf(d);
        s = sinf(d);
    }
    else // scale, per-axis
    {
        const float denomX = dragStartWx - pa, denomY = dragStartWy - pb;
        sxr = (fabsf(denomX) > 1e-3f) ? (wx - pa) / denomX : 1.0f;
        syr = (fabsf(denomY) > 1e-3f) ? (wy - pb) / denomY : 1.0f;
        sxr = snapToGrid(sxr, kScaleSnap);
        syr = snapToGrid(syr, kScaleSnap);
        if (sxr < kScaleSnap) sxr = kScaleSnap;
        if (syr < kScaleSnap) syr = kScaleSnap;
    }

    for (size_t j = 0; j < scene.selectedObjects.size(); j++)
    {
        Mesh& m = scene.objects[scene.selectedObjects[j]];
        const std::vector<Vec3>& orig = objDragOrig[j];
        for (size_t i = 0; i < m.positions.size() && i < orig.size(); i++)
        {
            const float a = getAxis(orig[i], ax);
            const float b = getAxis(orig[i], ay);
            Vec3 np = orig[i];
            if (transformTool == TransformTool::Move)
            {
                setAxis(np, ax, a + dX);
                setAxis(np, ay, b + dY);
            }
            else if (transformTool == TransformTool::Rotate)
            {
                const float da = a - pa, db = b - pb;
                setAxis(np, ax, pa + da * c - db * s);
                setAxis(np, ay, pb + da * s + db * c);
            }
            else
            {
                setAxis(np, ax, pa + (a - pa) * sxr);
                setAxis(np, ay, pb + (b - pb) * syr);
            }
            m.positions[i] = np;
        }
    }
}

// gather the verts to move for the current sub-object level (deduped), and
// snapshot their start positions
void Editor::beginSubDrag(const Viewport& vp)
{
    draggingSub = true;
    viewportWorld(vp, pressPx, pressPy, dragStartWx, dragStartWy);

    dragVerts.clear();
    auto addVert = [&](int o, int v) {
        for (const VertRef& vr : dragVerts)
            if (vr.obj == o && vr.vert == v)
                return;
        dragVerts.push_back({o, v});
    };
    if (subLevel == SubLevel::Vertex)
    {
        for (const VertRef& vr : scene.selectedVerts)
            addVert(vr.obj, vr.vert);
    }
    else if (subLevel == SubLevel::Edge)
    {
        for (const EdgeRef& er : scene.selectedEdges)
        {
            addVert(er.obj, er.v0);
            addVert(er.obj, er.v1);
        }
    }
    else // Face
    {
        for (const FaceRef& fr : scene.selectedFaces)
        {
            if (fr.obj < 0 || fr.obj >= (int)scene.objects.size())
                continue;
            const Mesh& m = scene.objects[fr.obj];
            if (fr.face < 0 || fr.face >= (int)m.faces.size())
                continue;
            const Face& f = m.faces[fr.face];
            for (int k = 0; k < 4; k++)
                addVert(fr.obj, f.indices[k]);
        }
    }

    dragOrig.clear();
    for (const VertRef& vr : dragVerts)
    {
        if (vr.obj >= 0 && vr.obj < (int)scene.objects.size() && vr.vert >= 0 &&
            vr.vert < (int)scene.objects[vr.obj].positions.size())
            dragOrig.push_back(scene.objects[vr.obj].positions[vr.vert]);
        else
            dragOrig.push_back({0.0f, 0.0f, 0.0f});
    }
}

void Editor::applySubDrag(const Viewport& vp, float wx, float wy)
{
    const int ax = vp.axisX, ay = vp.axisY;
    const float dX = snapToGrid(wx - dragStartWx, kSnap);
    const float dY = snapToGrid(wy - dragStartWy, kSnap);
    for (size_t k = 0; k < dragVerts.size() && k < dragOrig.size(); k++)
    {
        const VertRef& vr = dragVerts[k];
        if (vr.obj < 0 || vr.obj >= (int)scene.objects.size())
            continue;
        Mesh& m = scene.objects[vr.obj];
        if (vr.vert < 0 || vr.vert >= (int)m.positions.size())
            continue;
        Vec3 np = dragOrig[k];
        setAxis(np, ax, getAxis(dragOrig[k], ax) + dX);
        setAxis(np, ay, getAxis(dragOrig[k], ay) + dY);
        m.positions[vr.vert] = np;
    }
}

void Editor::handleTouchMove(int px, int py)
{
    if (screen == AppScreen::Browser) { browser.handleTouchMove(px, py); return; }
    if (!touching)
        return;

    // dragging in the color picker (works in either workspace)
    if (pickingSv || pickingHue)
    {
        if (pickingSv)
        {
            const Rect sv = pickerSv();
            float s = (float)(px - sv.x) / sv.w, v = 1.0f - (float)(py - sv.y) / sv.h;
            pickerS = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
            pickerV = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }
        else
        {
            const Rect hue = pickerHue();
            float h = (float)(py - hue.y) / hue.h;
            pickerH = h < 0.0f ? 0.0f : (h > 1.0f ? 1.0f : h);
        }
        paintColor = hsv2rgb(pickerH, pickerS, pickerV);
        return;
    }

    if (is2D())
    {
        tex.handleTouchMove(px, py);
        return;
    }

    const int dx = px - pressPx;
    const int dy = py - pressPy;
    if (dx * dx + dy * dy > kDragThresholdPx * kDragThresholdPx)
    {
        dragMoved = true;
        if ((pressedObject >= 0 || pressedSub) && !dragSnapshotted)
        {
            scene.snapshot();
            dragSnapshotted = true;
        }
    }

    if (!dragMoved)
        return;

    if (panning && panViewport >= 0)
    {
        Viewport& vp = viewports[panViewport];
        const float sx = vp.flipped ? -1.0f : 1.0f;
        vp.centerX -= sx * (px - lastPanPx) / vp.scale;
        vp.centerY += (py - lastPanPy) / vp.scale;
        lastPanPx = px;
        lastPanPy = py;
        return;
    }

    if (pressedObject >= 0 && objDragViewport >= 0)
    {
        const Viewport& vp = viewports[objDragViewport];
        if (!draggingObjects)
            beginObjectDrag(vp);
        float wx, wy;
        viewportWorld(vp, px, py, wx, wy);
        applyObjectDrag(vp, wx, wy);
        return;
    }

    if (pressedSub && subDragViewport >= 0)
    {
        const Viewport& vp = viewports[subDragViewport];
        if (!draggingSub)
            beginSubDrag(vp);
        float wx, wy;
        viewportWorld(vp, px, py, wx, wy);
        applySubDrag(vp, wx, wy);
        return;
    }
}

void Editor::handleTouchUp(int px, int py)
{
    if (screen == AppScreen::Browser) { browser.handleTouchUp(px, py); return; }
    (void)px;
    (void)py;
    if (!touching)
        return;
    touching = false;
    pickingSv = pickingHue = false;

    if (is2D())
    {
        tex.handleTouchUp();
        return;
    }

    // a plain tap on an already selected item deselects it (a new one was added on press)
    if (!dragMoved)
    {
        if (mode == EditMode::Edit && subLevel == SubLevel::Vertex &&
            pressedVertObj >= 0 && pressedVertWasSelected)
        {
            scene.selectedVerts.erase(
                std::remove_if(scene.selectedVerts.begin(), scene.selectedVerts.end(),
                               [&](const VertRef& vr) {
                                   return vr.obj == pressedVertObj && vr.vert == pressedVertIdx;
                               }),
                scene.selectedVerts.end());
        }
        else if (mode == EditMode::Edit && subLevel == SubLevel::Edge &&
                 pressedEdgeObj >= 0 && pressedEdgeWasSelected)
        {
            scene.selectedEdges.erase(
                std::remove_if(scene.selectedEdges.begin(), scene.selectedEdges.end(),
                               [&](const EdgeRef& er) {
                                   return er.obj == pressedEdgeObj &&
                                          ((er.v0 == pressedEdgeV0 && er.v1 == pressedEdgeV1) ||
                                           (er.v0 == pressedEdgeV1 && er.v1 == pressedEdgeV0));
                               }),
                scene.selectedEdges.end());
        }
        else if (mode == EditMode::Edit && subLevel == SubLevel::Face &&
                 pressedFaceObj >= 0 && pressedFaceWasSelected)
        {
            scene.selectedFaces.erase(
                std::remove_if(scene.selectedFaces.begin(), scene.selectedFaces.end(),
                               [&](const FaceRef& fr) {
                                   return fr.obj == pressedFaceObj && fr.face == pressedFaceIdx;
                               }),
                scene.selectedFaces.end());
        }
        else if (mode == EditMode::Object && pressedObject >= 0 && pressedObjWasSelected)
        {
            scene.selectedObjects.erase(
                std::remove(scene.selectedObjects.begin(), scene.selectedObjects.end(), pressedObject),
                scene.selectedObjects.end());
        }
    }

    if (pressedSub) // the pull drag (or any sub-object touch) ends the extrude
        extruding = false;

    pressedObject = -1;
    draggingObjects = false;
    objDragViewport = -1;
    pressedVertObj = pressedVertIdx = -1;
    pressedFaceObj = pressedFaceIdx = -1;
    pressedEdgeObj = pressedEdgeV0 = pressedEdgeV1 = -1;
    pressedSub = false;
    draggingSub = false;
    subDragViewport = -1;
    panning = false;
    panViewport = -1;
}

void Editor::renderViewports(Renderer& r)
{
    viewrender::render(scene, viewports, mode, subLevel, extruding, showFaces, maxView, r);
}
