#pragma once

#include <3ds.h>
#include "scene.h"
#include "rect.h"
#include "textureeditor.h"
#include "projectbrowser.h"
#include "palette.h"
#include "renderer.h"
#include "viewport.h"

// Paint-mode tool (segmented switch)
enum class PaintTool
{
    Brush,      // tap a face to paint the current color
    Eyedropper, // tap a face to pick its color into the palette
};

// Texture-mode tool (segmented switch)
enum class FaceTexTool
{
    Texture,   // tap a face to mark it textured
    Untexture, // tap a face to revert it to a flat color
};

// Which workspace is active: 3D modelling (ortho viewports) or 2D texture + UV
// editing (the paint canvas)
enum class Workspace
{
    ThreeD,
    TwoD,
};

// deferred file-menu action, run by main between frames (save prompts the
// keyboard, a system applet that can't run mid-render)
enum class FileOp
{
    None,
    Save,
    Load,
};

// interaction layer over a Scene: input, picking, transforms, toolbar, popups.
// the texture workspace lives in a separate TextureEditor (member `tex`).
struct Editor
{
    static constexpr int kNumShapes = 5; // cube, sphere, pyramid, cylinder, plane
    static constexpr int kNumModes = 4;  // object, model, paint, texture
    static constexpr int kNumMenu = 3;   // save, load, exit
    static constexpr int kNumView = 4;   // wireframe, faces, flip, shading
    static constexpr int kNumTexActions = 2; // texture all, untexture all

    Editor();

    Scene scene;
    EditMode mode = EditMode::Object;
    TransformTool transformTool = TransformTool::Move; // Object mode sub-switch
    SubLevel subLevel = SubLevel::Vertex;              // Edit mode sub-switch
    PaintTool paintTool = PaintTool::Brush;            // Paint mode sub-switch
    FaceTexTool faceTexTool = FaceTexTool::Texture;    // Texture mode sub-switch

    // Active workspace: 3D shows the ortho viewports; 2D shows the paint canvas.
    Workspace workspace = Workspace::ThreeD;
    bool is3D() const { return workspace == Workspace::ThreeD; }
    bool is2D() const { return workspace == Workspace::TwoD; }

    // the active paint color (arbitrary RGBA), shared by the model paint mode
    // and the texture workspace (tex reads it by reference)
    u32 paintColor = kPalette[2];

    // color picker popup: presets + an HSV square/hue-bar. working h/s/v is
    // seeded from paintColor on open so dragging doesn't accumulate error.
    bool colorPickerOpen = false;
    float pickerH = 0.0f, pickerS = 0.0f, pickerV = 1.0f;

    // shown in paint contexts (model Paint mode, or the texture-workspace brush)
    bool colorActive() const
    {
        return (is3D() && mode == EditMode::Paint) ||
               (is2D() && tex.texMode == TexMode::Paint);
    }

    // picker popup geometry (shared by draw + hit-test): HSV on top, palette
    // presets on the bottom
    static constexpr int kPickX = 54, kPickY = 28, kPickW = 212, kPickH = 185;
    Rect pickerPanel() const { return {kPickX, kPickY, kPickW, kPickH}; }
    Rect pickerSv() const { return {kPickX + 10, kPickY + 10, 160, 125}; }
    Rect pickerHue() const { const Rect s = pickerSv(); return {s.x + s.w + 10, s.y, 22, s.h}; }
    Rect pickerSwatch(int i) const
    {
        const int sw = 22, sh = 14, gap = 2, y0 = kPickY + 145; // below the SV square
        return {kPickX + 12 + (i % 8) * (sw + gap), y0 + (i / 8) * (sh + gap), sw, sh};
    }

    // texture workspace interaction (canvas, pixel tools, UV editing)
    TextureEditor tex{scene, paintColor};

    // top-level screen: the editor, or the full-screen project browser
    AppScreen screen = AppScreen::Editor;
    ProjectBrowser browser{scene, screen};

    bool wireframe = true; // wireframe overlay on the preview
    bool showFaces = true; // filled faces in the ortho views
    bool flipViews = false; // view ortho projections from the far side
    bool shading = true;   // soft flat shading on the preview

    // only one popup open at a time (texModeMenu lives in `tex`)
    bool modeMenuOpen = false;
    bool shapeMenuOpen = false;
    bool fileMenuOpen = false;
    bool viewMenuOpen = false;
    bool texActionMenuOpen = false; // texture-mode overflow (texture/untexture all)
    Rect modeMenu[kNumModes];
    Rect shapeMenu[kNumShapes];
    Rect fileMenu[kNumMenu];
    Rect viewMenu[kNumView];
    Rect texActionMenu[kNumTexActions];

    bool wantQuit = false; // set by Exit, main loop breaks on it

    // status toast, statusTime counts down
    const char* statusMsg = nullptr;
    float statusTime = 0.0f;
    void tickStatus(float dt) { if (statusTime > 0.0f) statusTime -= dt; }

    Viewport viewports[3]; // top, front, side
    Rect baseView[3];      // three-up layout rects, restored on un-maximize
    int maxView = -1;      // which ortho view fills the area (-1 = three-up)

    // maximize toggle button in a viewport's bottom corner (opposite the axis
    // arrows): bottom-right normally, bottom-left when flipped
    Rect viewMaxBtn(const Viewport& vp) const
    {
        const int s = 18, m = 4;
        const int x = flipViews ? (vp.x + m) : (vp.x + vp.w - s - m);
        return {x, vp.y + vp.h - s - m, s, s};
    }
    void toggleMax(int i);

    // toolbar hit rects, set in the ctor
    Rect btnMenu, btnAdd, btnDel, btnUndo, btnRedo; // top bar
    Rect btnView, btnWorkspace;                     // top bar, right
    Rect btnMode;                                   // bottom bar, left (model)
    Rect btnExtrude, btnSubdivide;                  // bottom bar, right (edit/face)
    Rect btnSplit;                                  // bottom bar, right (edit/edge)
    Rect btnColor;                                  // bottom bar, right (paint contexts)
    Rect btnTexMenu;                                // bottom bar, right (texture mode overflow)

    bool hasUndo() const { return scene.hasUndo(); }
    bool hasRedo() const { return scene.hasRedo(); }

    // segmented sub-switch: number of cells for the current mode, and the rect
    // for cell i (shared centered-pill geometry, see segCell)
    int subSegCount() const
    {
        switch (mode)
        {
        case EditMode::Object: return 3; // move / rotate / scale
        case EditMode::Edit: return 3;   // vertex / edge / face
        case EditMode::Paint: return 2;  // brush / eyedropper
        case EditMode::Texture: return 2; // texture / untexture
        }
        return 0;
    }
    Rect subSegRect(int i) const { return segCell(i, subSegCount()); }

    void handleKeys(u32 kDown);
    void handleTouchDown(int px, int py);
    void handleTouchMove(int px, int py);
    void handleTouchUp(int px, int py);

    // pending file-menu action; serviceFileOps runs it (call between frames)
    FileOp pendingFileOp = FileOp::None;
    void serviceFileOps();

    // tie the ortho scale to the preview's camera distance (single zoom)
    void syncZoom(float cameraDistance);

    void renderViewports(Renderer& r);

private:
    static constexpr float kSnap = 0.5f;
    static constexpr float kSelectRadiusPx = 14.0f;
    static constexpr float kDragThresholdPx = 4.0f;
    static constexpr float kZoomScale = 112.5f; // scale = kZoomScale / distance
    static constexpr float kRotSnap = 0.2617993878f; // 15 deg
    static constexpr float kScaleSnap = 0.25f;

    bool touching = false;
    bool pressInViewport = false;
    int pressPx = 0, pressPy = 0;
    bool dragMoved = false;
    bool dragSnapshotted = false;

    // finger world pos at drag start
    float dragStartWx = 0.0f, dragStartWy = 0.0f;

    // object drag: move/rotate/scale around the centroid
    int pressedObject = -1;
    bool pressedObjWasSelected = false;
    bool draggingObjects = false;
    int objDragViewport = -1;
    Vec3 objDragPivot = {0.0f, 0.0f, 0.0f};
    std::vector<std::vector<Vec3>> objDragOrig; // per selected object

    // Edit-mode sub-object press: greedy select + deselect-on-tap, per level
    int pressedVertObj = -1, pressedVertIdx = -1;
    bool pressedVertWasSelected = false;
    int pressedFaceObj = -1, pressedFaceIdx = -1;
    bool pressedFaceWasSelected = false;
    int pressedEdgeObj = -1, pressedEdgeV0 = -1, pressedEdgeV1 = -1;
    bool pressedEdgeWasSelected = false;

    // sub-object drag: moves selected verts, or the verts of selected edges/faces
    bool pressedSub = false;      // a sub-object was pressed (drag candidate)
    bool grabSelection = false;   // extrude just ran; next viewport drag moves it
    bool extruding = false;       // extrude operation active (shows normal arrows)
    bool draggingSub = false;
    int subDragViewport = -1;
    std::vector<VertRef> dragVerts; // verts moved this drag (deduped union)
    std::vector<Vec3> dragOrig;     // their positions at drag start

    bool panning = false;
    int panViewport = -1;
    int lastPanPx = 0, lastPanPy = 0;

    bool pickingSv = false, pickingHue = false; // dragging in the color picker

    Viewport* viewportAt(int px, int py);
    bool pickVertexAny(const Viewport& vp, int px, int py, int& outObj, int& outVert);
    bool pickEdge(const Viewport& vp, int px, int py, int& outObj, int& outV0, int& outV1);
    bool pickFace(const Viewport& vp, int px, int py, int& outObj, int& outFace);
    int pickObject(const Viewport& vp, int px, int py);
    void deleteSelected(); // dispatches to the scene by mode
    void applyFlip();      // push flipViews to the viewports
    void setStatus(const char* m);
    void beginObjectDrag(const Viewport& vp);
    void applyObjectDrag(const Viewport& vp, float wx, float wy);
    void beginSubDrag(const Viewport& vp);
    void applySubDrag(const Viewport& vp, float wx, float wy);
};
