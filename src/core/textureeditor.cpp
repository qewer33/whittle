#include "textureeditor.h"
#include "palette.h"
#include "icons.h"
#include "layout.h"
#include <algorithm>
#include <math.h>

// winding-agnostic point-in-triangle, rejects zero-area tris (tris are stored
// as quads {a,b,c,c})
static bool pointInTri(float px, float py, float ax, float ay, float bx,
                       float by, float cx, float cy)
{
    const float area2 = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (fabsf(area2) < 1.0f)
        return false;
    const float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    const float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    const float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

void TextureEditor::layout()
{
    const int bw = 30, BB = layout::kBarH;
    const int by = layout::kBottomBarY;
    btnTexMode = {0, by, 84, BB};
    // paint: brush/fill/eyedropper are the centered toolSwitch, brush size sits
    // left of the far-right color button (owned by Editor)
    btnBrushSize = {320 - 2 * bw - 4, by, bw, BB};
    // uv tools, right-aligned: auto-layout, fit
    btnAutoLayout = {320 - 2 * bw - 4, by, bw, BB};
    btnUvReset = {320 - bw, by, bw, BB};

    texModeMenu.setup(btnTexMode, widgets::Placement::Above, widgets::Align::Start, 90,
                      {{Icon_Paint, "Paint"}, {Icon_Move, "UV"}});
    toolSwitch.setup({Icon_Paint, Icon_Bucket, Icon_Pipette});

    fitCanvas();
}

void TextureEditor::fitCanvas()
{
    const float area = (float)layout::kContentH;
    canvasScale = area / Scene::kTexSize;
    canvasX = (320 - Scene::kTexSize * canvasScale) / 2.0f;
    canvasY = layout::kContentY + (area - Scene::kTexSize * canvasScale) / 2.0f;
}

bool TextureEditor::canvasTexel(int px, int py, int& tx, int& ty) const
{
    tx = (int)((px - canvasX) / canvasScale);
    ty = (int)((py - canvasY) / canvasScale);
    return tx >= 0 && tx < Scene::kTexSize && ty >= 0 && ty < Scene::kTexSize;
}

void TextureEditor::stampBrush(int tx, int ty)
{
    const u32 col = paintColor;
    const int lo = -(brushSize / 2);
    for (int oy = lo; oy < brushSize + lo; oy++)
        for (int ox = lo; ox < brushSize + lo; ox++)
        {
            const int x = tx + ox, y = ty + oy;
            if (x >= 0 && x < Scene::kTexSize && y >= 0 && y < Scene::kTexSize)
                scene.texture[y * Scene::kTexSize + x] = col;
        }
    scene.textureDirty = true;
}

// Bresenham line, stamp the brush at each step so fast drags leave no gaps
void TextureEditor::brushLine(int x0, int y0, int x1, int y1)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true)
    {
        stampBrush(x0, y0);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void TextureEditor::floodFill(int px, int py)
{
    int tx, ty;
    if (!canvasTexel(px, py, tx, ty))
        return;
    const int W = Scene::kTexSize;
    const u32 target = scene.texture[ty * W + tx];
    const u32 col = paintColor;
    if (target == col)
        return;
    std::vector<int> stack;
    stack.push_back(ty * W + tx);
    while (!stack.empty())
    {
        const int i = stack.back();
        stack.pop_back();
        if (scene.texture[i] != target)
            continue;
        scene.texture[i] = col;
        const int x = i % W, y = i / W;
        if (x > 0) stack.push_back(i - 1);
        if (x < W - 1) stack.push_back(i + 1);
        if (y > 0) stack.push_back(i - W);
        if (y < W - 1) stack.push_back(i + W);
    }
    scene.textureDirty = true;
}

void TextureEditor::eyedrop(int px, int py)
{
    int tx, ty;
    if (!canvasTexel(px, py, tx, ty))
        return;
    paintColor = scene.texture[ty * Scene::kTexSize + tx]; // exact pixel color
}

// canvas pixel to uv (v=1 at the top of the canvas, matching the display)
void TextureEditor::canvasToUv(int px, int py, float& u, float& v) const
{
    const float texPx = Scene::kTexSize * canvasScale;
    u = (px - canvasX) / texPx;
    v = 1.0f - (py - canvasY) / texPx;
}

bool TextureEditor::pickUvHandle(int px, int py)
{
    const float texPx = Scene::kTexSize * canvasScale;
    auto screenQuad = [&](const Face& f, float* sx, float* sy) {
        for (int c = 0; c < 4; c++)
        {
            sx[c] = canvasX + f.uv[c][0] * texPx;
            sy[c] = canvasY + (1.0f - f.uv[c][1]) * texPx;
        }
    };

    // current target: grab a corner or the whole quad, then a drag begins
    if (uvObj >= 0 && uvObj < (int)scene.objects.size() &&
        uvFaceIdx >= 0 && uvFaceIdx < (int)scene.objects[uvObj].faces.size())
    {
        const Face& f = scene.objects[uvObj].faces[uvFaceIdx];
        float sx[4], sy[4];
        screenQuad(f, sx, sy);
        int best = -1;
        float bestD = 12.0f * 12.0f;
        for (int c = 0; c < 4; c++)
        {
            const float d = (sx[c] - px) * (sx[c] - px) + (sy[c] - py) * (sy[c] - py);
            if (d < bestD) { bestD = d; best = c; }
        }
        for (int c = 0; c < 4; c++)
        {
            uvOrig[c][0] = f.uv[c][0];
            uvOrig[c][1] = f.uv[c][1];
        }
        canvasToUv(px, py, uvGrabU, uvGrabV);
        if (best >= 0) { uvGrab = best; return true; }
        if (pointInTri((float)px, (float)py, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]) ||
            pointInTri((float)px, (float)py, sx[0], sy[0], sx[2], sy[2], sx[3], sy[3]))
        {
            uvGrab = 4;
            return true;
        }
    }

    // otherwise, re-select another textured face's island as the edit target
    // (no drag yet, the next tap edits it)
    for (int o = 0; o < (int)scene.objects.size(); o++)
    {
        const Mesh& m = scene.objects[o];
        for (int fi = 0; fi < (int)m.faces.size(); fi++)
        {
            if (!m.faces[fi].textured || (o == uvObj && fi == uvFaceIdx))
                continue;
            float sx[4], sy[4];
            screenQuad(m.faces[fi], sx, sy);
            if (pointInTri((float)px, (float)py, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]) ||
                pointInTri((float)px, (float)py, sx[0], sy[0], sx[2], sy[2], sx[3], sy[3]))
            {
                uvObj = o;
                uvFaceIdx = fi;
                return false;
            }
        }
    }
    return false;
}

void TextureEditor::navCanvas(const circlePosition& pad, u32 held)
{
    static constexpr float kDead = 20.0f, kPan = 4.0f, kZoom = 1.04f;
    if (fabsf((float)pad.dx) > kDead)
        canvasX -= (pad.dx / 156.0f) * kPan;
    if (fabsf((float)pad.dy) > kDead)
        canvasY += (pad.dy / 156.0f) * kPan;

    // zoom around the canvas-area center (L/R or D-pad up/down)
    float factor = 1.0f;
    if (held & (KEY_L | KEY_DUP))
        factor = kZoom;
    else if (held & (KEY_R | KEY_DDOWN))
        factor = 1.0f / kZoom;
    if (factor != 1.0f)
    {
        const float cx = 160.0f, cy = 110.0f;
        canvasX = cx - (cx - canvasX) * factor;
        canvasY = cy - (cy - canvasY) * factor;
        canvasScale *= factor;
        if (canvasScale < 0.5f) canvasScale = 0.5f;
        if (canvasScale > 16.0f) canvasScale = 16.0f;
    }
}

void TextureEditor::clearSheet()
{
    scene.snapshot();
    std::fill(scene.texture.begin(), scene.texture.end(), paintColor);
    scene.textureDirty = true;
}

// pack textured faces into a uniform grid on the sheet, one slot each. grid
// adapts to face count, slots snap to texels. callers snapshot first.
void TextureEditor::autoLayout()
{
    int n = 0;
    for (const Mesh& m : scene.objects)
        for (const Face& f : m.faces)
            if (f.textured)
                n++;
    if (n == 0)
        return;

    int grid = 1;
    while (grid * grid < n)
        grid++;
    const int cellTx = Scene::kTexSize / grid; // texels per slot
    const float cell = (float)cellTx / Scene::kTexSize;

    int slot = 0;
    for (Mesh& m : scene.objects)
        for (Face& f : m.faces)
        {
            if (!f.textured)
                continue;
            const int gx = slot % grid, gy = slot / grid;
            const float u0 = gx * cell, u1 = u0 + cell;
            const float v1 = 1.0f - gy * cell;       // top of slot (V-flipped)
            const float v0 = 1.0f - (gy + 1) * cell; // bottom
            f.uv[0][0] = u0; f.uv[0][1] = v1;        // top-left
            f.uv[1][0] = u1; f.uv[1][1] = v1;        // top-right
            f.uv[2][0] = u1; f.uv[2][1] = v0;        // bottom-right
            f.uv[3][0] = u0; f.uv[3][1] = v0;        // bottom-left
            slot++;
        }

    // point the UV editor at the first textured face
    for (int o = 0; o < (int)scene.objects.size(); o++)
        for (int fi = 0; fi < (int)scene.objects[o].faces.size(); fi++)
            if (scene.objects[o].faces[fi].textured)
            {
                uvObj = o;
                uvFaceIdx = fi;
                return;
            }
}

void TextureEditor::setBrushFromTrack(int py)
{
    const Rect tr = brushTrack();
    float t = (float)(tr.y + tr.h - py) / tr.h; // 0 at bottom, 1 at top
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    brushSize = 1 + (int)(t * (kMaxBrush - 1) + 0.5f);
}

void TextureEditor::handleTouchDown(int px, int py)
{
    paintingTex = false;

    // brush-size popup eats taps while open: grab the slider, or tap-out closes
    if (brushMenuOpen)
    {
        if (brushTrack().contains(px, py)) { setBrushFromTrack(py); draggingBrush = true; return; }
        if (!brushMenu().contains(px, py))
            brushMenuOpen = false;
        return;
    }

    if (btnTexMode.contains(px, py)) { texModeMenu.open = true; return; }
    const bool uv = texMode == TexMode::Uv;
    if (!uv) // brush / fill / eyedropper segmented switch
    {
        const int c = toolSwitch.handle(px, py);
        if (c >= 0) { texTool = (TexTool)c; return; }
    }
    if (!uv && btnBrushSize.contains(px, py)) { brushMenuOpen = true; return; }
    if (uv && btnAutoLayout.contains(px, py)) { scene.snapshot(); autoLayout(); return; }
    if (uv && btnUvReset.contains(px, py))
    {
        if (uvObj >= 0 && uvObj < (int)scene.objects.size() &&
            uvFaceIdx >= 0 && uvFaceIdx < (int)scene.objects[uvObj].faces.size())
        {
            scene.snapshot();
            Face& f = scene.objects[uvObj].faces[uvFaceIdx];
            f.uv[0][0] = 0; f.uv[0][1] = 1;
            f.uv[1][0] = 1; f.uv[1][1] = 1;
            f.uv[2][0] = 1; f.uv[2][1] = 0;
            f.uv[3][0] = 0; f.uv[3][1] = 0;
        }
        return;
    }

    // canvas: paint / fill / eyedrop, or grab a uv handle
    if (uv)
    {
        if (pickUvHandle(px, py))
            scene.snapshot();
    }
    else if (texTool == TexTool::Eyedropper)
        eyedrop(px, py);
    else if (texTool == TexTool::Fill)
    {
        scene.snapshot();
        floodFill(px, py);
    }
    else
    {
        int tx, ty;
        if (canvasTexel(px, py, tx, ty))
        {
            scene.snapshot();
            stampBrush(tx, ty);
            lastPaintTx = tx;
            lastPaintTy = ty;
            paintingTex = true;
        }
    }
}

void TextureEditor::handleTouchMove(int px, int py)
{
    if (draggingBrush) { setBrushFromTrack(py); return; }

    if (texMode == TexMode::Uv)
    {
        if (uvGrab >= 0 && uvObj >= 0 && uvObj < (int)scene.objects.size() &&
            uvFaceIdx < (int)scene.objects[uvObj].faces.size())
        {
            float u, v;
            canvasToUv(px, py, u, v);
            const float g = (float)Scene::kTexSize;
            Face& f = scene.objects[uvObj].faces[uvFaceIdx];
            if (uvGrab < 4)
            {
                u = roundf(u * g) / g; // always snap uvs to the texel grid
                v = roundf(v * g) / g;
                f.uv[uvGrab][0] = u;
                f.uv[uvGrab][1] = v;
            }
            else
            {
                float du = roundf((u - uvGrabU) * g) / g;
                float dv = roundf((v - uvGrabV) * g) / g;
                for (int c = 0; c < 4; c++)
                {
                    f.uv[c][0] = uvOrig[c][0] + du;
                    f.uv[c][1] = uvOrig[c][1] + dv;
                }
            }
        }
    }
    else if (paintingTex)
    {
        int tx, ty;
        if (canvasTexel(px, py, tx, ty))
        {
            brushLine(lastPaintTx, lastPaintTy, tx, ty);
            lastPaintTx = tx;
            lastPaintTy = ty;
        }
    }
}

void TextureEditor::handleTouchUp()
{
    paintingTex = false;
    uvGrab = -1;
    draggingBrush = false;
}
