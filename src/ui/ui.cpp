#include "ui.h"
#include "icons.h"
#include "palette.h"
#include <citro2d.h>
#include <cstdio>

static const u32 kBarBg = 0x1B1E26FF;     // toolbar background
static const u32 kActiveBg = 0x4E7BCCFF;   // active/open button background
static const u32 kItemBg = 0x333A4AFF;     // popup item background
static const u32 kPanelBg = 0x272B36FF;    // popup panel background
static const u32 kBorderCol = 0x66707EFF;
static const u32 kIconIdle = 0xC2C8D4FF;   // normal icon
static const u32 kIconActive = 0xFFFFFFFF; // icon on an active/open button
static const u32 kIconDim = 0x565D6EFF;    // disabled icon (e.g. undo/redo)
static const u32 kTextCol = 0xE8ECF4FF;

static constexpr float kIcon = 14.0f;

namespace
{
    const char* const kModeLabels[Editor::kNumModes] = {
        "Object", "Edit", "Paint", "Texture"};
    const char* const kShapeLabels[Editor::kNumShapes] = {
        "Cube", "Sphere", "Pyramid", "Cylinder", "Plane"};
    const char* const kFileLabels[Editor::kNumMenu] = {"Save", "Load", "Exit"};
    const char* const kTexModeLabels[TextureEditor::kNumTexModes] = {"Paint", "UV"};
    const char* const kViewLabels[Editor::kNumView] = {"Wireframe", "Faces", "Flip", "Shading"};
    const char* const kWorkspaceLabels[2] = {"3D", "2D"}; // ThreeD, TwoD
    const char* const kTexActionLabels[Editor::kNumTexActions] = {"Texture All", "Untexture All"};
    // glyphs parallel to the label arrays
    const Icon kModeIcons[Editor::kNumModes] = {
        Icon_Box, Icon_Pencil, Icon_Paint, Icon_Texture};
    const Icon kShapeIcons[Editor::kNumShapes] = {
        Icon_Box, Icon_Circle, Icon_Pyramid, Icon_Cylinder, Icon_Square};
    const Icon kFileIcons[Editor::kNumMenu] = {Icon_Save, Icon_Load, Icon_Exit};
    const Icon kTexModeIcons[TextureEditor::kNumTexModes] = {Icon_Paint, Icon_Move};
    const Icon kViewIcons[Editor::kNumView] = {Icon_Box, Icon_Square, Icon_Flip, Icon_Shade};
    const Icon kTexActionIcons[Editor::kNumTexActions] = {Icon_Texture, Icon_Eraser};
    // segmented sub-switch glyphs
    const Icon kTransformIcons[3] = {Icon_Move, Icon_Rotate, Icon_Scale};
    const Icon kSubLevelIcons[3] = {Icon_Vertex, Icon_Edge, Icon_Square};
    const Icon kTexToolIcons[3] = {Icon_Paint, Icon_Bucket, Icon_Pipette};
    const Icon kPaintIcons[2] = {Icon_Paint, Icon_Pipette};      // brush, eyedropper
    const Icon kFaceTexIcons[2] = {Icon_Texture, Icon_Eraser};   // texture, untexture
    constexpr float kTextScale = 0.5f;

    C2D_TextBuf textBuf = nullptr;
    C2D_TextBuf toastBuf = nullptr; // reparsed each frame for the toast
    C2D_TextBuf brushBuf = nullptr; // reparsed each frame for the brush-size number
    C2D_Text modeLabels[Editor::kNumModes];
    float modeLabelH[Editor::kNumModes] = {0};
    C2D_Text shapeLabels[Editor::kNumShapes];
    float shapeLabelH[Editor::kNumShapes] = {0};
    C2D_Text fileLabels[Editor::kNumMenu];
    float fileLabelH[Editor::kNumMenu] = {0};
    C2D_Text texModeLabels[TextureEditor::kNumTexModes];
    float texModeLabelH[TextureEditor::kNumTexModes] = {0};
    C2D_Text viewLabels[Editor::kNumView];
    float viewLabelH[Editor::kNumView] = {0};
    C2D_Text workspaceLabels[2];
    float workspaceLabelH[2] = {0};
    const char* const kAxisLabels[3] = {"X", "Y", "Z"};
    C2D_Text axisText[3];
    C2D_Text texActionLabels[Editor::kNumTexActions];
    float texActionLabelH[Editor::kNumTexActions] = {0};

    inline u32 conv(u32 rgba)
    {
        return C2D_Color32((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                           (rgba >> 8) & 0xFF, rgba & 0xFF);
    }

    void outline(int x, int y, int w, int h, u32 color)
    {
        const u32 c = conv(color);
        C2D_DrawRectSolid(x, y, 0.0f, w, 1, c);
        C2D_DrawRectSolid(x, y + h - 1, 0.0f, w, 1, c);
        C2D_DrawRectSolid(x, y, 0.0f, 1, h, c);
        C2D_DrawRectSolid(x + w - 1, y, 0.0f, 1, h, c);
    }

    void parseAll(C2D_Text* txt, float* h, const char* const* names, int n)
    {
        for (int i = 0; i < n; i++)
        {
            C2D_TextParse(&txt[i], textBuf, names[i]);
            C2D_TextOptimize(&txt[i]);
            float w, hh;
            C2D_TextGetDimensions(&txt[i], kTextScale, kTextScale, &w, &hh);
            h[i] = hh;
        }
    }

    // left-aligned text, vertically centered in [y, y+h]. the +2 fudge accounts
    // for the descender space C2D includes in the measured height, which would
    // otherwise leave the visible glyphs sitting too high
    void textLeft(C2D_Text* t, float th, float x, float y, float h)
    {
        C2D_DrawText(t, C2D_WithColor, x, y + (h - th) / 2.0f + 1.0f, 0.0f,
                     kTextScale, kTextScale, conv(kTextCol));
    }

    // icon centered in a button; active fills the bg, disabled dims the icon
    void iconBtn(const Rect& b, Icon ic, bool active, bool enabled = true)
    {
        if (active)
            C2D_DrawRectSolid(b.x, b.y, 0.0f, b.w, b.h, conv(kActiveBg));
        const u32 col = !enabled ? kIconDim : (active ? kIconActive : kIconIdle);
        icons::draw(ic, b.x + (b.w - kIcon) / 2.0f, b.y + (b.h - kIcon) / 2.0f, kIcon, col);
    }

    // segmented switch: floating inset pill, cells centered in the bar
    void drawSegmented(const Icon* ics, int n, int active)
    {
        for (int i = 0; i < n; i++)
        {
            const Rect r = segCell(i, n);
            const bool on = (i == active);
            C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(on ? kActiveBg : kItemBg));
            outline(r.x, r.y, r.w, r.h, kBorderCol);
            icons::draw(ics[i], r.x + (r.w - kIcon) / 2.0f, r.y + (r.h - kIcon) / 2.0f,
                        kIcon, on ? kIconActive : kIconIdle);
        }
    }

    void drawPopup(const Rect* rects, int count, const Icon* ics, C2D_Text* txt,
                   const float* txtH, int highlight)
    {
        const Rect& first = rects[0];
        const Rect& last = rects[count - 1];
        const int panX = first.x - 2, panY = first.y - 2;
        const int panW = first.w + 4, panH = (last.y + last.h) - first.y + 4;
        C2D_DrawRectSolid(panX, panY, 0.0f, panW, panH, conv(kPanelBg));
        outline(panX, panY, panW, panH, kBorderCol);
        for (int i = 0; i < count; i++)
        {
            const Rect& r = rects[i];
            const bool hi = (i == highlight);
            C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(hi ? kActiveBg : kItemBg));
            outline(r.x, r.y, r.w, r.h, kBorderCol);
            icons::draw(ics[i], r.x + 5, r.y + (r.h - kIcon) / 2.0f, kIcon,
                        hi ? kIconActive : kIconIdle);
            textLeft(&txt[i], txtH[i], r.x + 5 + kIcon + 4, r.y, r.h);
        }
    }
}

namespace ui
{
    void init()
    {
        textBuf = C2D_TextBufNew(512);
        toastBuf = C2D_TextBufNew(64);
        brushBuf = C2D_TextBufNew(16);
        parseAll(modeLabels, modeLabelH, kModeLabels, Editor::kNumModes);
        parseAll(shapeLabels, shapeLabelH, kShapeLabels, Editor::kNumShapes);
        parseAll(fileLabels, fileLabelH, kFileLabels, Editor::kNumMenu);
        parseAll(texModeLabels, texModeLabelH, kTexModeLabels, TextureEditor::kNumTexModes);
        parseAll(viewLabels, viewLabelH, kViewLabels, Editor::kNumView);
        parseAll(workspaceLabels, workspaceLabelH, kWorkspaceLabels, 2);
        for (int i = 0; i < 3; i++)
        {
            C2D_TextParse(&axisText[i], textBuf, kAxisLabels[i]);
            C2D_TextOptimize(&axisText[i]);
        }
        parseAll(texActionLabels, texActionLabelH, kTexActionLabels, Editor::kNumTexActions);
    }

    void exit()
    {
        if (textBuf)
            C2D_TextBufDelete(textBuf);
        if (toastBuf)
            C2D_TextBufDelete(toastBuf);
        if (brushBuf)
            C2D_TextBufDelete(brushBuf);
        textBuf = nullptr;
        toastBuf = nullptr;
        brushBuf = nullptr;
    }

    void draw(Editor& editor, C3D_Tex* texture)
    {
        // painter's order, so kill depth test
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

        const int topH = editor.btnMenu.y + editor.btnMenu.h;
        const Rect& bm = editor.btnMode;
        const int modeIdx = (int)editor.mode;

        // texture canvas first, so the bars/chrome cover any zoom overflow
        if (editor.is2D())
        {
            static Tex3DS_SubTexture sub;
            sub.width = Scene::kTexSize;
            sub.height = Scene::kTexSize;
            sub.left = 0.0f;
            sub.top = 1.0f;
            sub.right = 1.0f;
            sub.bottom = 0.0f;
            C2D_Image img;
            img.tex = texture;
            img.subtex = &sub;
            C2D_DrawImageAt(img, editor.tex.canvasX, editor.tex.canvasY, 0.0f, nullptr,
                            editor.tex.canvasScale, editor.tex.canvasScale);
            const float texPx = Scene::kTexSize * editor.tex.canvasScale;
            outline((int)editor.tex.canvasX, (int)editor.tex.canvasY, (int)texPx, (int)texPx, kBorderCol);

            // pixel grid (only when zoomed in enough to be readable)
            if (editor.tex.canvasScale >= 4.0f)
            {
                const u32 gc = conv(0xFFFFFF28);
                const float s = editor.tex.canvasScale, x0 = editor.tex.canvasX, y0 = editor.tex.canvasY;
                int vi0 = (int)((0 - x0) / s), vi1 = (int)((320 - x0) / s) + 1;
                if (vi0 < 0) vi0 = 0;
                if (vi1 > Scene::kTexSize) vi1 = Scene::kTexSize;
                for (int i = vi0; i <= vi1; i++)
                    C2D_DrawLine(x0 + i * s, y0, gc, x0 + i * s, y0 + texPx, gc, 1.0f, 0.05f);
                int hi0 = (int)((topH - y0) / s), hi1 = (int)((bm.y - y0) / s) + 1;
                if (hi0 < 0) hi0 = 0;
                if (hi1 > Scene::kTexSize) hi1 = Scene::kTexSize;
                for (int i = hi0; i <= hi1; i++)
                    C2D_DrawLine(x0, y0 + i * s, gc, x0 + texPx, y0 + i * s, gc, 1.0f, 0.05f);
            }

            // UV overlay: all textured faces faint always; in uv mode the edit
            // target is drawn bright with handles instead
            {
                auto drawQuad = [&](const Face& f, u32 col, float thick, bool handles) {
                    float cx[4], cy[4];
                    for (int c = 0; c < 4; c++)
                    {
                        cx[c] = editor.tex.canvasX + f.uv[c][0] * texPx;
                        cy[c] = editor.tex.canvasY + (1.0f - f.uv[c][1]) * texPx;
                    }
                    const u32 lc = conv(col);
                    for (int c = 0; c < 4; c++)
                    {
                        const int n = (c + 1) & 3;
                        C2D_DrawLine(cx[c], cy[c], lc, cx[n], cy[n], lc, thick, 0.1f);
                    }
                    if (handles)
                        for (int c = 0; c < 4; c++)
                            C2D_DrawRectSolid(cx[c] - 3, cy[c] - 3, 0.1f, 6, 6, conv(0xFFFFFFFF));
                };

                // every textured face faint; skip the edit target in uv mode
                // (it gets the bright pass below)
                for (int o = 0; o < (int)editor.scene.objects.size(); o++)
                {
                    const Mesh& m = editor.scene.objects[o];
                    for (int fi = 0; fi < (int)m.faces.size(); fi++)
                    {
                        if (!m.faces[fi].textured)
                            continue;
                        if (editor.tex.texMode == TexMode::Uv && o == editor.tex.uvObj &&
                            fi == editor.tex.uvFaceIdx)
                            continue;
                        drawQuad(m.faces[fi], 0x8891A0C0, 1.0f, false);
                    }
                }

                if (editor.tex.texMode == TexMode::Uv &&
                    editor.tex.uvObj >= 0 && editor.tex.uvObj < (int)editor.scene.objects.size() &&
                    editor.tex.uvFaceIdx >= 0 &&
                    editor.tex.uvFaceIdx < (int)editor.scene.objects[editor.tex.uvObj].faces.size())
                    drawQuad(editor.scene.objects[editor.tex.uvObj].faces[editor.tex.uvFaceIdx],
                             0xFFD24AFF, 1.5f, true);
            }
        }

        // bar backgrounds
        C2D_DrawRectSolid(0, 0, 0.0f, 320, topH, conv(kBarBg));
        C2D_DrawRectSolid(0, bm.y, 0.0f, 320, bm.h, conv(kBarBg));

        // top bar. left corner: 3D/2D workspace switch (labeled, shows current
        // workspace, tap flips). right corner: hamburger menu.
        {
            const Rect& b = editor.btnWorkspace;
            const int wi = editor.is2D() ? 1 : 0;
            icons::draw(wi ? Icon_Image : Icon_Box, b.x + 6,
                        b.y + (b.h - kIcon) / 2.0f, kIcon, kIconIdle);
            textLeft(&workspaceLabels[wi], workspaceLabelH[wi], b.x + 6 + kIcon + 4, b.y, b.h);
        }
        if (editor.is2D())
        {
            iconBtn(editor.btnAdd, Icon_Fit, false);   // recenter/fit canvas
            iconBtn(editor.btnDel, Icon_Trash, false); // clear sheet
        }
        else
        {
            iconBtn(editor.btnAdd, Icon_Plus, editor.shapeMenuOpen);
            iconBtn(editor.btnDel, Icon_Trash, false);
        }
        iconBtn(editor.btnUndo, Icon_Undo, false, editor.hasUndo());
        iconBtn(editor.btnRedo, Icon_Redo, false, editor.hasRedo());
        if (editor.is3D())
            iconBtn(editor.btnView, Icon_Eye, editor.viewMenuOpen);
        iconBtn(editor.btnMenu, Icon_Menu, editor.fileMenuOpen);

        // bottom bar: model controls (mode + sub-switch), or texture tools
        if (editor.is3D())
        {
            if (editor.modeMenuOpen)
                C2D_DrawRectSolid(bm.x, bm.y, 0.0f, bm.w, bm.h, conv(kActiveBg));
            const u32 modeCol = editor.modeMenuOpen ? kIconActive : kIconIdle;
            icons::draw(kModeIcons[modeIdx], bm.x + 5,
                        bm.y + (bm.h - kIcon) / 2.0f, kIcon, modeCol);
            textLeft(&modeLabels[modeIdx], modeLabelH[modeIdx], bm.x + 5 + kIcon + 4, bm.y, bm.h);

            // segmented sub-switch for the current mode
            if (editor.mode == EditMode::Object)
                drawSegmented(kTransformIcons, 3, (int)editor.transformTool);
            else if (editor.mode == EditMode::Edit)
                drawSegmented(kSubLevelIcons, 3, (int)editor.subLevel);
            else if (editor.mode == EditMode::Paint)
                drawSegmented(kPaintIcons, 2, (int)editor.paintTool);
            else if (editor.mode == EditMode::Texture)
                drawSegmented(kFaceTexIcons, 2, (int)editor.faceTexTool);

            // Edit/Face verb buttons (right side)
            if (editor.mode == EditMode::Edit && editor.subLevel == SubLevel::Face)
            {
                const bool hasFaces = !editor.scene.selectedFaces.empty();
                iconBtn(editor.btnSubdivide, Icon_Subdivide, false, hasFaces);
                iconBtn(editor.btnExtrude, Icon_Extrude, false, hasFaces);
            }
            // Edit/Edge verb button (right side)
            else if (editor.mode == EditMode::Edit && editor.subLevel == SubLevel::Edge)
                iconBtn(editor.btnSplit, Icon_Split, false, !editor.scene.selectedEdges.empty());
        }
        else
        {
            // mode button (icon + label), bottom-left
            const Rect& tm = editor.tex.btnTexMode;
            const int texIdx = (int)editor.tex.texMode;
            if (editor.tex.texModeMenuOpen)
                C2D_DrawRectSolid(tm.x, tm.y, 0.0f, tm.w, tm.h, conv(kActiveBg));
            const u32 tmCol = editor.tex.texModeMenuOpen ? kIconActive : kIconIdle;
            icons::draw(kTexModeIcons[texIdx], tm.x + 5,
                        tm.y + (tm.h - kIcon) / 2.0f, kIcon, tmCol);
            textLeft(&texModeLabels[texIdx], texModeLabelH[texIdx], tm.x + 5 + kIcon + 4, tm.y, tm.h);

            if (editor.tex.texMode == TexMode::Paint)
            {
                // brush / fill / eyedropper as the centered segmented switch
                drawSegmented(kTexToolIcons, 3, (int)editor.tex.texTool);
                // brush size: a white square sized to the brush (capped to the
                // button), opens the size popup
                const Rect& bs = editor.tex.btnBrushSize;
                if (editor.tex.brushMenuOpen)
                    C2D_DrawRectSolid(bs.x, bs.y, 0.0f, bs.w, bs.h, conv(kActiveBg));
                int side = editor.tex.brushSize * 2;
                const int maxSide = bs.h - 8;
                if (side > maxSide) side = maxSide;
                if (side < 2) side = 2;
                C2D_DrawRectSolid(bs.x + (bs.w - side) / 2, bs.y + (bs.h - side) / 2, 0.0f,
                                  side, side, conv(editor.tex.brushMenuOpen ? kIconActive : kIconIdle));
            }
            else
            {
                iconBtn(editor.tex.btnAutoLayout, Icon_Layout, false);
                iconBtn(editor.tex.btnUvReset, Icon_Fit, false);
            }
        }

        // model viewport chrome (the canvas is drawn earlier, behind the bars)
        if (editor.is3D())
        {
            // gutters in the toolbar color: an outer frame around the ortho area
            // plus the dividers between views. one shared width, centered on each
            // edge (no doubling at seams).
            const u32 bc = conv(kBarBg);
            const int G = 3;
            const int ax0 = 0, ay0 = topH, aw = 320, ah = bm.y - topH;
            C2D_DrawRectSolid(ax0, ay0, 0.0f, aw, G, bc);          // top
            C2D_DrawRectSolid(ax0, ay0 + ah - G, 0.0f, aw, G, bc); // bottom
            C2D_DrawRectSolid(ax0, ay0, 0.0f, G, ah, bc);          // left
            C2D_DrawRectSolid(ax0 + aw - G, ay0, 0.0f, G, ah, bc); // right
            if (editor.maxView < 0)
            {
                const Viewport& v0 = editor.viewports[0];
                const int divY = v0.y + v0.h;                                  // rows
                const int divX = editor.viewports[1].x + editor.viewports[1].w; // cols
                C2D_DrawRectSolid(ax0, divY - G / 2, 0.0f, aw, G, bc);
                C2D_DrawRectSolid(divX - G / 2, divY, 0.0f, G, ay0 + ah - divY, bc);
            }

            for (int i = 0; i < 3; i++)
            {
                if (editor.maxView >= 0 && i != editor.maxView)
                    continue; // a maximized view fills the whole area
                const Viewport& vp = editor.viewports[i];

                // per-axis arrows (the 3D gizmo seen from this ortho angle):
                // X red / Y green / Z blue, horizontal one flips with flip
                static const u32 axisCol[3] = {0xF04747FF, 0x4FD16BFF, 0x4A8CF0FF};
                auto arrow = [&](float x0, float y0, float x1, float y1, u32 rgba) {
                    const u32 c = conv(rgba);
                    C2D_DrawLine(x0, y0, c, x1, y1, c, 1.0f, 0.5f);
                    float dx = x1 - x0, dy = y1 - y0;
                    const float l = sqrtf(dx * dx + dy * dy);
                    if (l < 0.01f) return;
                    dx /= l; dy /= l;
                    const float hl = 3.5f, ca = 0.82f, sa = 0.57f; // ~35 deg barbs
                    const float bx = -dx, by = -dy;
                    C2D_DrawLine(x1, y1, c, x1 + (bx * ca - by * sa) * hl,
                                 y1 + (bx * sa + by * ca) * hl, c, 1.0f, 0.5f);
                    C2D_DrawLine(x1, y1, c, x1 + (bx * ca + by * sa) * hl,
                                 y1 + (-bx * sa + by * ca) * hl, c, 1.0f, 0.5f);
                };
                const float len = 11.0f, hs = editor.flipViews ? -1.0f : 1.0f;
                // anchor bottom-left, or bottom-right when flipped so the
                // horizontal arrow always points inward
                const float ax = editor.flipViews ? (vp.x + vp.w - 9.0f) : (vp.x + 9.0f);
                const float ay = vp.y + vp.h - 9.0f;
                arrow(ax, ay, ax + hs * len, ay, axisCol[vp.axisX]); // horizontal
                arrow(ax, ay, ax, ay - len, axisCol[vp.axisY]);      // vertical

                // axis letters at the tips
                const float sc = 0.42f;
                auto letter = [&](int axis, float lx, float ly) {
                    C2D_DrawText(&axisText[axis], C2D_WithColor, lx, ly, 0.0f, sc, sc,
                                 conv(axisCol[axis]));
                };
                letter(vp.axisX, ax + hs * (len + 3.0f) - (hs < 0 ? 6.0f : 0.0f), ay - 5.0f);
                letter(vp.axisY, ax - 3.0f, ay - len - 12.0f);

                // maximize / restore toggle in the top corner
                const Rect mb = editor.viewMaxBtn(vp);
                C2D_DrawRectSolid(mb.x, mb.y, 0.0f, mb.w, mb.h, conv(0x1B1E26C0));
                outline(mb.x, mb.y, mb.w, mb.h, kBorderCol);
                icons::draw(editor.maxView >= 0 ? Icon_Minimize : Icon_Fit,
                            mb.x + (mb.w - kIcon) / 2.0f, mb.y + (mb.h - kIcon) / 2.0f,
                            kIcon, kIconIdle);
            }
        }

        // active-color button (opens the picker) in paint contexts
        if (editor.colorActive())
        {
            const Rect& b = editor.btnColor;
            C2D_DrawRectSolid(b.x, b.y, 0.0f, b.w, b.h, conv(kBarBg));
            C2D_DrawRectSolid(b.x + 3, b.y + 3, 0.0f, b.w - 6, b.h - 6, conv(editor.paintColor));
            outline(b.x + 3, b.y + 3, b.w - 6, b.h - 6, kBorderCol);
        }
        // texture-mode overflow menu button (texture all / untexture all)
        else if (editor.mode == EditMode::Texture && editor.is3D())
            iconBtn(editor.btnTexMenu, Icon_More, editor.texActionMenuOpen);

        // popups on top
        if (editor.modeMenuOpen)
        {
            drawPopup(editor.modeMenu, Editor::kNumModes, kModeIcons, modeLabels,
                      modeLabelH, modeIdx);
        }
        else if (editor.shapeMenuOpen)
        {
            drawPopup(editor.shapeMenu, Editor::kNumShapes, kShapeIcons, shapeLabels,
                      shapeLabelH, -1);
        }
        else if (editor.fileMenuOpen)
        {
            drawPopup(editor.fileMenu, Editor::kNumMenu, kFileIcons, fileLabels,
                      fileLabelH, -1);
        }
        else if (editor.texActionMenuOpen)
        {
            drawPopup(editor.texActionMenu, Editor::kNumTexActions, kTexActionIcons,
                      texActionLabels, texActionLabelH, -1);
        }
        else if (editor.tex.texModeMenuOpen)
        {
            drawPopup(editor.tex.texModeMenu, TextureEditor::kNumTexModes, kTexModeIcons,
                      texModeLabels, texModeLabelH, (int)editor.tex.texMode);
        }
        else if (editor.viewMenuOpen)
        {
            // toggle menu: each row highlighted by its own on/off state
            const Rect* rects = editor.viewMenu;
            const bool on[Editor::kNumView] = {editor.wireframe, editor.showFaces, editor.flipViews, editor.shading};
            const Rect& first = rects[0];
            const Rect& last = rects[Editor::kNumView - 1];
            const int panX = first.x - 2, panY = first.y - 2;
            const int panW = first.w + 4, panH = (last.y + last.h) - first.y + 4;
            C2D_DrawRectSolid(panX, panY, 0.0f, panW, panH, conv(kPanelBg));
            outline(panX, panY, panW, panH, kBorderCol);
            for (int i = 0; i < Editor::kNumView; i++)
            {
                const Rect& r = rects[i];
                C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(on[i] ? kActiveBg : kItemBg));
                outline(r.x, r.y, r.w, r.h, kBorderCol);
                icons::draw(kViewIcons[i], r.x + 5, r.y + (r.h - kIcon) / 2.0f, kIcon,
                            on[i] ? kIconActive : kIconIdle);
                textLeft(&viewLabels[i], viewLabelH[i], r.x + 5 + kIcon + 4, r.y, r.h);
            }
        }

        // status toast, centered under the top bar
        if (editor.statusTime > 0.0f && editor.statusMsg)
        {
            C2D_TextBufClear(toastBuf);
            C2D_Text t;
            C2D_TextParse(&t, toastBuf, editor.statusMsg);
            C2D_TextOptimize(&t);
            float tw, th;
            C2D_TextGetDimensions(&t, kTextScale, kTextScale, &tw, &th);
            const float pad = 8.0f;
            const float bw = tw + pad * 2, bh = th + 6.0f;
            const float bx = 160.0f - bw / 2.0f;
            const float by = topH + 6.0f;
            C2D_DrawRectSolid(bx, by, 0.0f, bw, bh, conv(0x11141BE0));
            outline(bx, by, bw, bh, kBorderCol);
            C2D_DrawText(&t, C2D_WithColor, bx + pad, by + 3.0f, 0.0f,
                         kTextScale, kTextScale, conv(kTextCol));
        }

        // color picker popup, topmost
        if (editor.colorPickerOpen)
        {
            const Rect panel = editor.pickerPanel();
            C2D_DrawRectSolid(panel.x, panel.y, 0.0f, panel.w, panel.h, conv(kPanelBg));
            outline(panel.x, panel.y, panel.w, panel.h, kBorderCol);

            // palette presets
            for (int i = 0; i < kPaletteCount; i++)
            {
                const Rect s = editor.pickerSwatch(i);
                C2D_DrawRectSolid(s.x, s.y, 0.0f, s.w, s.h, conv(kPalette[i]));
                outline(s.x, s.y, s.w, s.h, kBorderCol);
            }

            // SV square: one bilinear gradient (white / hue / black / black)
            const Rect sv = editor.pickerSv();
            const u32 hueCol = hsv2rgb(editor.pickerH, 1.0f, 1.0f);
            C2D_DrawRectangle(sv.x, sv.y, 0.0f, sv.w, sv.h, conv(0xFFFFFFFF), conv(hueCol),
                              conv(0x000000FF), conv(0x000000FF));
            const int cx = sv.x + (int)(editor.pickerS * sv.w);
            const int cy = sv.y + (int)((1.0f - editor.pickerV) * sv.h);
            outline(cx - 4, cy - 4, 8, 8, 0xFFFFFFFF);
            outline(cx - 3, cy - 3, 6, 6, 0x000000FF);

            // hue bar: 6 vertical gradient segments
            const Rect hue = editor.pickerHue();
            const float segH = hue.h / 6.0f;
            for (int i = 0; i < 6; i++)
            {
                const u32 a = hsv2rgb(i / 6.0f, 1.0f, 1.0f);
                const u32 b = hsv2rgb((i + 1) / 6.0f, 1.0f, 1.0f);
                C2D_DrawRectangle(hue.x, hue.y + i * segH, 0.0f, hue.w, segH, conv(a), conv(a),
                                  conv(b), conv(b));
            }
            outline(hue.x, hue.y, hue.w, hue.h, kBorderCol);
            const int hy = hue.y + (int)(editor.pickerH * hue.h);
            C2D_DrawRectSolid(hue.x - 2, hy - 1, 0.0f, hue.w + 4, 3, conv(0xFFFFFFFF));
        }

        // brush-size popup (2D Paint), topmost: vertical slider, size in px, and
        // a preview circle at the bottom
        if (editor.is2D() && editor.tex.brushMenuOpen)
        {
            const Rect p = editor.tex.brushMenu();
            C2D_DrawRectSolid(p.x, p.y, 0.0f, p.w, p.h, conv(kPanelBg));
            outline(p.x, p.y, p.w, p.h, kBorderCol);

            const Rect tr = editor.tex.brushTrack();
            const int cx = tr.x + tr.w / 2;
            C2D_DrawRectSolid(cx - 1, tr.y, 0.0f, 2, tr.h, conv(kItemBg));
            outline(cx - 1, tr.y, 2, tr.h, kBorderCol);
            const float t = (float)(editor.tex.brushSize - 1) / (TextureEditor::kMaxBrush - 1);
            const int hy2 = tr.y + tr.h - (int)(t * tr.h);
            C2D_DrawRectSolid(tr.x, hy2 - 3, 0.0f, tr.w, 6, conv(kActiveBg));
            outline(tr.x, hy2 - 3, tr.w, 6, kBorderCol);

            // size in px, centered below the track
            char buf[8];
            snprintf(buf, sizeof buf, "%dpx", editor.tex.brushSize);
            C2D_TextBufClear(brushBuf);
            C2D_Text nt;
            C2D_TextParse(&nt, brushBuf, buf);
            C2D_TextOptimize(&nt);
            float tw, th;
            C2D_TextGetDimensions(&nt, kTextScale, kTextScale, &tw, &th);
            C2D_DrawText(&nt, C2D_WithColor, p.x + (p.w - tw) / 2.0f, tr.y + tr.h + 6.0f, 0.0f,
                         kTextScale, kTextScale, conv(kTextCol));
        }
    }
}
