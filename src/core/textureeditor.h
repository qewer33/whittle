#pragma once

#include <3ds.h>
#include "rect.h"
#include "scene.h"
#include "widgets/menu.h"
#include "widgets/button.h"
#include "widgets/toolswitch.h"

// pixel-editing tool in the texture workspace
enum class TexTool
{
    Brush,
    Fill,
    Eyedropper,
};

// texture workspace mode: paint pixels, or edit UVs
enum class TexMode
{
    Paint,
    Uv,
};

// interaction layer for the texture workspace: canvas nav, pixel painting, uv
// editing. operates on the shared Scene and the editor's paint color (by ref).
struct TextureEditor
{
    static constexpr int kNumTexModes = 2; // paint, uv

    TextureEditor(Scene& s, u32& color) : scene(s), paintColor(color) { layout(); }

    Scene& scene;
    u32& paintColor; // the active paint color (0xRRGGBBAA), owned by Editor

    // canvas transform: top-left at (canvasX, canvasY), canvasScale display px
    // per texel.
    float canvasScale = 1.0f;
    float canvasX = 0.0f, canvasY = 0.0f;

    static constexpr int kMaxBrush = 16; // brush size range is 1..kMaxBrush texels

    TexMode texMode = TexMode::Paint;
    int uvObj = -1, uvFaceIdx = -1; // face whose UVs are being edited
    TexTool texTool = TexTool::Brush;
    int brushSize = 1;

    widgets::Menu texModeMenu;                // paint/uv switch popup (bottom bar, left)
    widgets::ToolSwitch toolSwitch;           // brush/fill/eyedropper (paint mode)
    widgets::Button btnTexMode;               // bottom bar, left
    widgets::Button btnBrushSize;             // bottom bar, right (paint, tools are toolSwitch)
    widgets::Button btnAutoLayout, btnUvReset; // bottom bar, right (uv)

    // brush-size popup: a vertical slider with the size in px below it. right-
    // anchored above the brush-size button so it clears the color button.
    bool brushMenuOpen = false;
    static constexpr int kBrushMenuW = 46, kBrushMenuH = 126;
    Rect brushMenu() const
    {
        return {btnBrushSize.x + btnBrushSize.w - kBrushMenuW, btnBrushSize.y - kBrushMenuH,
                kBrushMenuW, kBrushMenuH};
    }
    // forgiving hit column for the slider (draw a thin track inside it)
    Rect brushTrack() const
    {
        const Rect p = brushMenu();
        return {p.x + 6, p.y + 12, p.w - 12, 90};
    }

    void layout(); // compute the toolbar rects + fit the canvas

    // input while the texture workspace is active (the editor forwards these)
    void handleTouchDown(int px, int py);
    void handleTouchMove(int px, int py);
    void handleTouchUp();

    void navCanvas(const circlePosition& pad, u32 held); // circle-pad pan + L/R zoom
    void fitCanvas();  // reset the canvas transform to fit the sheet centered
    void clearSheet(); // fill the sheet with the current color (snapshots)
    void autoLayout(); // pack textured faces into an atlas grid (no snapshot)

private:
    bool paintingTex = false;              // dragging a stroke on the canvas
    int lastPaintTx = 0, lastPaintTy = 0;  // previous stamp texel, for interpolation
    bool draggingBrush = false;            // dragging the brush-size slider

    void setBrushFromTrack(int py); // map a y within the slider track to brushSize

    // uv drag state: -1 none, 0-3 a corner, 4 the whole quad
    int uvGrab = -1;
    float uvOrig[4][2] = {};
    float uvGrabU = 0.0f, uvGrabV = 0.0f;

    bool canvasTexel(int px, int py, int& tx, int& ty) const;
    void stampBrush(int tx, int ty);
    void brushLine(int x0, int y0, int x1, int y1);
    void floodFill(int px, int py);
    void eyedrop(int px, int py);
    void canvasToUv(int px, int py, float& u, float& v) const;
    bool pickUvHandle(int px, int py);
};
