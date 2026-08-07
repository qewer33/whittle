#include "ui.h"
#include "icons.h"
#include "palette.h"
#include "uidraw.h"
#include "layout.h"
#include "widgets/bar.h"
#include <citro2d.h>
#include <cstdio>
#include <string>

using namespace uidraw; // theme colors, conv, outline, textLeft, kIcon, kTextScale

namespace
{
    // the migrated menu popups own their own labels/icons (set in the Editor),
    // these are only for the buttons that still live in ui.cpp
    const char* const kModeLabels[Editor::kNumModes] = {
        "Object", "Edit", "Paint", "Texture"};
    const char* const kTexModeLabels[TextureEditor::kNumTexModes] = {"Paint", "UV"};
    const char* const kWorkspaceLabels[2] = {"3D", "2D"}; // ThreeD, TwoD
    const Icon kModeIcons[Editor::kNumModes] = {
        Icon_Box, Icon_Pencil, Icon_Paint, Icon_Texture};
    const Icon kTexModeIcons[TextureEditor::kNumTexModes] = {Icon_Paint, Icon_Move};
    C2D_TextBuf textBuf = nullptr;
    C2D_TextBuf toastBuf = nullptr; // reparsed each frame for the toast
    C2D_TextBuf brushBuf = nullptr; // reparsed each frame for the brush-size number
    C2D_TextBuf topBuf = nullptr;   // reparsed each frame for the top-screen project name
    C2D_Text modeLabels[Editor::kNumModes];
    float modeLabelH[Editor::kNumModes] = {0};
    C2D_Text texModeLabels[TextureEditor::kNumTexModes];
    float texModeLabelH[TextureEditor::kNumTexModes] = {0};
    C2D_Text workspaceLabels[2];
    float workspaceLabelH[2] = {0};
    const char* const kAxisLabels[3] = {"X", "Y", "Z"};
    C2D_Text axisText[3];
    // project browser: Projects, Back, New, Delete, Open, Cancel
    const char* const kBrowserLabels[6] = {"Projects", "Back", "New", "Delete", "Open", "Cancel"};
    C2D_Text browserLabels[6];
    float browserLabelH[6] = {0};
    C2D_TextBuf browserBuf = nullptr; // reparsed each frame for project names

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

    // a button with centered text, dimmed when disabled, filled when active
    void textBtn(const Rect& r, C2D_Text* t, float th, bool enabled, bool active = false)
    {
        C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(active ? kActiveBg : kItemBg));
        outline(r.x, r.y, r.w, r.h, kBorderCol);
        float tw, hh;
        C2D_TextGetDimensions(t, kTextScale, kTextScale, &tw, &hh);
        C2D_DrawText(t, C2D_WithColor, r.x + (r.w - tw) / 2.0f, r.y + (r.h - th) / 2.0f, 0.0f,
                     kTextScale, kTextScale, conv(enabled ? kTextCol : kIconDim));
    }

    // a button with an icon + label, the pair centered, dimmed when disabled
    void iconTextBtn(const Rect& r, Icon ic, C2D_Text* t, float th, bool enabled)
    {
        C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(kItemBg));
        outline(r.x, r.y, r.w, r.h, kBorderCol);
        float tw, hh;
        C2D_TextGetDimensions(t, kTextScale, kTextScale, &tw, &hh);
        float sx = r.x + (r.w - (kIcon + 4 + tw)) / 2.0f;
        if (sx < r.x + 3)
            sx = r.x + 3;
        icons::draw(ic, sx, r.y + (r.h - kIcon) / 2.0f, kIcon, enabled ? kIconIdle : kIconDim);
        C2D_DrawText(t, C2D_WithColor, sx + kIcon + 4, r.y + (r.h - th) / 2.0f, 0.0f,
                     kTextScale, kTextScale, conv(enabled ? kTextCol : kIconDim));
    }

    // parse a transient string into browserBuf and draw it (buffer is cleared
    // once per browser frame, so all its texts stay valid until the flush)
    void browserText(const char* s, float x, float y, u32 col)
    {
        C2D_Text t;
        C2D_TextParse(&t, browserBuf, s);
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, kTextScale, kTextScale, conv(col));
    }

    void drawBrowser(Editor& editor)
    {
        ProjectBrowser& b = editor.browser;
        C2D_TextBufClear(browserBuf);
        C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, conv(kPanelBg));

        // rows (header/footer are drawn after, to cover any overflow)
        for (int i = 0; i < (int)b.entries.size(); i++)
        {
            const Rect r = b.rowRect(i);
            if (r.y + r.h <= ProjectBrowser::kListTop || r.y >= ProjectBrowser::kListBottom)
                continue;
            const bool sel = i == b.selected;
            if (sel)
                C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(kActiveBg));
            browserText(b.entries[i].name.c_str(), r.x + 10, r.y + (r.h - browserLabelH[0]) / 2.0f,
                        sel ? kIconActive : kTextCol);
            C2D_DrawRectSolid(r.x, r.y + r.h - 1, 0.0f, r.w, 1, conv(kBarBg));
        }

        if (b.entries.empty())
            browserText("No projects", 132.0f, 108.0f, kIconDim);

        // header
        widgets::Bar{{0, 0, 320, ProjectBrowser::kHeaderH}}.draw();
        icons::draw(Icon_Load, 8, (ProjectBrowser::kHeaderH - kIcon) / 2.0f, kIcon, kIconIdle);
        textLeft(&browserLabels[0], browserLabelH[0], 8 + kIcon + 4, 0, ProjectBrowser::kHeaderH);
        iconTextBtn(b.btnBack(), Icon_Back, &browserLabels[1], browserLabelH[1], true);

        // footer actions
        const int fy = 240 - ProjectBrowser::kFooterH;
        widgets::Bar{{0, fy, 320, ProjectBrowser::kFooterH}}.draw();
        const bool hasSel = b.selected >= 0;
        iconTextBtn(b.btnNew(), Icon_Plus, &browserLabels[2], browserLabelH[2], true);
        iconTextBtn(b.btnDelete(), Icon_Trash, &browserLabels[3], browserLabelH[3], hasSel);
        iconTextBtn(b.btnOpen(), Icon_Load, &browserLabels[4], browserLabelH[4], hasSel);

        // delete confirmation
        if (b.confirmingDelete)
        {
            C2D_DrawRectSolid(0, 0, 0.0f, 320, 240, conv(0x00000099));
            const Rect box = b.confirmBox();
            C2D_DrawRectSolid(box.x, box.y, 0.0f, box.w, box.h, conv(kPanelBg));
            outline(box.x, box.y, box.w, box.h, kBorderCol);
            const std::string name = hasSel ? b.entries[b.selected].name : std::string();
            const std::string msg = "Delete " + name + "?";
            C2D_Text t;
            C2D_TextParse(&t, browserBuf, msg.c_str());
            C2D_TextOptimize(&t);
            float tw, th;
            C2D_TextGetDimensions(&t, kTextScale, kTextScale, &tw, &th);
            C2D_DrawText(&t, C2D_WithColor, box.x + (box.w - tw) / 2.0f, box.y + 12.0f, 0.0f,
                         kTextScale, kTextScale, conv(kTextCol));
            textBtn(b.confirmCancel(), &browserLabels[5], browserLabelH[5], true);
            textBtn(b.confirmOk(), &browserLabels[3], browserLabelH[3], true, true);
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
        browserBuf = C2D_TextBufNew(1024);
        topBuf = C2D_TextBufNew(64);
        parseAll(modeLabels, modeLabelH, kModeLabels, Editor::kNumModes);
        parseAll(texModeLabels, texModeLabelH, kTexModeLabels, TextureEditor::kNumTexModes);
        parseAll(workspaceLabels, workspaceLabelH, kWorkspaceLabels, 2);
        for (int i = 0; i < 3; i++)
        {
            C2D_TextParse(&axisText[i], textBuf, kAxisLabels[i]);
            C2D_TextOptimize(&axisText[i]);
        }
        parseAll(browserLabels, browserLabelH, kBrowserLabels, 6);
    }

    void exit()
    {
        if (textBuf)
            C2D_TextBufDelete(textBuf);
        if (toastBuf)
            C2D_TextBufDelete(toastBuf);
        if (brushBuf)
            C2D_TextBufDelete(brushBuf);
        if (browserBuf)
            C2D_TextBufDelete(browserBuf);
        if (topBuf)
            C2D_TextBufDelete(topBuf);
        textBuf = nullptr;
        toastBuf = nullptr;
        brushBuf = nullptr;
        browserBuf = nullptr;
        topBuf = nullptr;
    }

    void draw(Editor& editor, C3D_Tex* texture)
    {
        // painter's order, so kill depth test
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

        if (editor.screen == AppScreen::Browser)
        {
            drawBrowser(editor);
            return;
        }

        const int topH = layout::kBarH;
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

            // UV overlay: all textured faces faint always, in uv mode the edit
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

                // every textured face faint, skip the edit target in uv mode
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
        widgets::Bar{layout::topBar()}.draw();
        widgets::Bar{layout::bottomBar()}.draw();

        // top bar. left corner: 3D/2D workspace switch (labeled, shows current
        // workspace, tap flips). right corner: hamburger menu.
        const int wi = editor.is2D() ? 1 : 0;
        editor.btnWorkspace.drawLabeled(wi ? Icon_Image : Icon_Box, &workspaceLabels[wi],
                                        workspaceLabelH[wi]);
        if (editor.is2D())
        {
            editor.btnAdd.draw(Icon_Fit);   // recenter/fit canvas
            editor.btnDel.draw(Icon_Trash); // clear sheet
        }
        else
        {
            editor.btnAdd.draw(Icon_Plus, editor.shapeMenu.open);
            editor.btnDel.draw(Icon_Trash);
        }
        editor.btnUndo.draw(Icon_Undo, false, editor.hasUndo());
        editor.btnRedo.draw(Icon_Redo, false, editor.hasRedo());
        if (editor.is3D())
            editor.btnView.draw(Icon_Eye, editor.viewMenu.open);
        editor.btnMenu.draw(Icon_Menu, editor.fileMenu.open);

        // bottom bar: model controls (mode + sub-switch), or texture tools
        if (editor.is3D())
        {
            editor.btnMode.drawLabeled(kModeIcons[modeIdx], &modeLabels[modeIdx],
                                       modeLabelH[modeIdx], editor.modeMenu.open);

            // segmented tool switch for the current mode
            if (editor.mode == EditMode::Object)
                editor.transformSwitch.draw((int)editor.transformTool);
            else if (editor.mode == EditMode::Edit)
                editor.subLevelSwitch.draw((int)editor.subLevel);
            else if (editor.mode == EditMode::Paint)
                editor.paintSwitch.draw((int)editor.paintTool);
            else if (editor.mode == EditMode::Texture)
                editor.texSwitch.draw((int)editor.faceTexTool);

            // Edit/Face verb buttons (right side)
            if (editor.mode == EditMode::Edit && editor.subLevel == SubLevel::Face)
            {
                const bool hasFaces = !editor.scene.selectedFaces.empty();
                editor.btnSubdivide.draw(Icon_Subdivide, false, hasFaces);
                editor.btnExtrude.draw(Icon_Extrude, false, hasFaces);
            }
            // Edit/Edge verb button (right side)
            else if (editor.mode == EditMode::Edit && editor.subLevel == SubLevel::Edge)
                editor.btnSplit.draw(Icon_Split, false, !editor.scene.selectedEdges.empty());
        }
        else
        {
            // mode button (icon + label), bottom-left
            const int texIdx = (int)editor.tex.texMode;
            editor.tex.btnTexMode.drawLabeled(kTexModeIcons[texIdx], &texModeLabels[texIdx],
                                              texModeLabelH[texIdx], editor.tex.texModeMenu.open);

            if (editor.tex.texMode == TexMode::Paint)
            {
                // brush / fill / eyedropper as the centered segmented switch
                editor.tex.toolSwitch.draw((int)editor.tex.texTool);
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
                editor.tex.btnAutoLayout.draw(Icon_Layout);
                editor.tex.btnUvReset.draw(Icon_Fit);
            }
        }

        // model viewport chrome (the canvas is drawn earlier, behind the bars)
        if (editor.is3D())
        {
            // ortho view dividers
            if (editor.maxView < 0)
            {
                const u32 bc = conv(kBarBg);
                const int G = 3;
                const Rect area = layout::content();
                const Viewport& v0 = editor.viewports[0];
                const int divY = v0.y + v0.h;
                const int divX = editor.viewports[1].x + editor.viewports[1].w;
                C2D_DrawRectSolid(area.x, divY - G / 2, 0.0f, area.w, G, bc);
                C2D_DrawRectSolid(divX - G / 2, divY, 0.0f, G, area.y + area.h - divY, bc);
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
            editor.btnTexMenu.draw(Icon_More, editor.texActionMenu.open);

        // toolbar popups on top: each widget draws itself when open (exportMenu
        // is a flyout, so file + export can both be open)
        if (editor.modeMenu.open)
        {
            bool on[Editor::kNumModes] = {};
            on[modeIdx] = true;
            editor.modeMenu.draw(on);
        }
        if (editor.shapeMenu.open)
            editor.shapeMenu.draw();
        if (editor.fileMenu.open)
            editor.fileMenu.draw();
        if (editor.exportMenu.open)
            editor.exportMenu.draw();
        if (editor.viewMenu.open)
        {
            const bool on[Editor::kNumView] = {editor.wireframe, editor.showFaces,
                                               editor.flipViews, editor.shading};
            editor.viewMenu.draw(on);
        }
        if (editor.texActionMenu.open)
            editor.texActionMenu.draw();
        if (editor.tex.texModeMenu.open)
        {
            bool on[TextureEditor::kNumTexModes] = {};
            on[(int)editor.tex.texMode] = true;
            editor.tex.texModeMenu.draw(on);
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

    void drawTop(Editor& editor)
    {
        if (editor.screen != AppScreen::Editor)
            return;
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);

        const char* name = editor.scene.projectName.empty() ? "Unnamed Model"
                                                            : editor.scene.projectName.c_str();
        C2D_TextBufClear(topBuf);
        C2D_Text t;
        C2D_TextParse(&t, topBuf, name);
        C2D_TextOptimize(&t);
        const float sc = 0.43f;         // a couple px smaller than the toolbar text
        const u32 nameCol = 0xA8AEBAFF; // grayer than the bright UI text
        float tw, th;
        C2D_TextGetDimensions(&t, sc, sc, &tw, &th);
        const float x = 8.0f, y = 6.0f;
        C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, sc, sc, conv(nameCol));
        // unsaved-changes dot trailing the name
        if (editor.scene.dirty)
            C2D_DrawCircleSolid(x + tw + 6.0f, y + th / 2.0f, 0.0f, 2.0f, conv(0xF0A030FF));
    }
}
