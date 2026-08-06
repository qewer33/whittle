#pragma once

#include <vector>
#include "scene.h"
#include "rect.h"
#include "camera.h"
#include "mesh.h"

// which top-level screen is active
enum class AppScreen
{
    Editor,
    Browser,
};

// full-screen project picker, opened from the file menu's Load. lists saved
// projects, previews the selected one on the top screen, and opens/deletes/news.
struct ProjectBrowser
{
    ProjectBrowser(Scene& s, AppScreen& screen) : scene(s), screenRef(screen) {}

    Scene& scene;
    AppScreen& screenRef;

    std::vector<ProjectInfo> entries;
    int selected = -1;
    float scroll = 0.0f;           // list scroll offset in px
    bool confirmingDelete = false; // delete-confirm modal up

    std::vector<Mesh> preview; // top-screen preview of the selected project
    Camera camera;

    void open();           // refresh the list, select newest, load its preview
    void update(float dt); // spin the preview
    void handleTouchDown(int px, int py);
    void handleTouchMove(int px, int py);
    void handleTouchUp(int px, int py);

    // layout, shared by draw + hit-test
    static constexpr int kHeaderH = 26, kFooterH = 26, kRowH = 30;
    static constexpr int kListTop = kHeaderH;
    static constexpr int kListBottom = 240 - kFooterH;
    Rect btnBack() const { return {320 - 6 - 56, 3, 56, 20}; }
    Rect btnNew() const { return {6, 240 - kFooterH + 3, 60, 20}; }
    Rect btnDelete() const { return {320 - 6 - 62 - 6 - 64, 240 - kFooterH + 3, 64, 20}; }
    Rect btnOpen() const { return {320 - 6 - 62, 240 - kFooterH + 3, 62, 20}; }
    Rect rowRect(int i) const { return {0, kListTop + (int)(i * kRowH - scroll), 320, kRowH}; }
    Rect confirmBox() const { return {50, 88, 220, 64}; }
    Rect confirmCancel() const { return {60, 122, 90, 22}; }
    Rect confirmOk() const { return {170, 122, 90, 22}; }

private:
    bool pressedList = false; // a list press in progress (tap vs scroll-drag)
    int pressY = 0;
    int pressedRow = -1; // row under the press (touch-up coords are unreliable)
    float pressScroll = 0.0f;
    bool dragging = false;

    int rowAt(int py) const;  // row index under a y, or -1
    float maxScroll() const;
    void refresh();           // re-list + re-sort (newest first)
    void loadPreview();       // load `selected` into preview + frame the camera
    void close();
};
