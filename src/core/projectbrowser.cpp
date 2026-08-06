#include "projectbrowser.h"
#include <algorithm>

void ProjectBrowser::refresh()
{
    entries = scene.listProjects();
    std::sort(entries.begin(), entries.end(),
              [](const ProjectInfo& a, const ProjectInfo& b) { return a.mtime > b.mtime; });
}

void ProjectBrowser::open()
{
    refresh();
    selected = entries.empty() ? -1 : 0;
    scroll = 0.0f;
    confirmingDelete = false;
    loadPreview();
}

void ProjectBrowser::update(float dt)
{
    camera.yaw += dt * 0.6f; // slow spin
}

void ProjectBrowser::loadPreview()
{
    preview.clear();
    if (selected < 0 || selected >= (int)entries.size())
        return;
    if (!scene.peekMeshes(entries[selected].path, preview))
    {
        preview.clear();
        return;
    }

    // frame the camera on the mesh bounds so any project fits
    Vec3 lo = {1e9f, 1e9f, 1e9f}, hi = {-1e9f, -1e9f, -1e9f};
    bool any = false;
    for (const Mesh& m : preview)
        for (const Vec3& p : m.positions)
        {
            lo.x = p.x < lo.x ? p.x : lo.x;
            lo.y = p.y < lo.y ? p.y : lo.y;
            lo.z = p.z < lo.z ? p.z : lo.z;
            hi.x = p.x > hi.x ? p.x : hi.x;
            hi.y = p.y > hi.y ? p.y : hi.y;
            hi.z = p.z > hi.z ? p.z : hi.z;
            any = true;
        }
    if (!any)
        return;
    camera.target = {(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    float r = hi.x - lo.x;
    r = (hi.y - lo.y) > r ? (hi.y - lo.y) : r;
    r = (hi.z - lo.z) > r ? (hi.z - lo.z) : r;
    camera.distance = r * 2.0f + 1.5f;
    camera.yaw = 0.75f;
    camera.pitch = 0.5f;
}

float ProjectBrowser::maxScroll() const
{
    const float total = (float)entries.size() * kRowH;
    const float view = (float)(kListBottom - kListTop);
    return total > view ? total - view : 0.0f;
}

int ProjectBrowser::rowAt(int py) const
{
    if (py < kListTop || py >= kListBottom)
        return -1;
    const int i = (int)(((float)(py - kListTop) + scroll) / kRowH);
    return (i >= 0 && i < (int)entries.size()) ? i : -1;
}

void ProjectBrowser::close()
{
    screenRef = AppScreen::Editor;
    preview.clear();
}

void ProjectBrowser::handleTouchDown(int px, int py)
{
    pressedList = false;
    dragging = false;

    if (confirmingDelete)
    {
        if (confirmCancel().contains(px, py))
            confirmingDelete = false;
        else if (confirmOk().contains(px, py))
        {
            if (selected >= 0 && selected < (int)entries.size())
            {
                scene.deleteProject(entries[selected].path);
                refresh();
                if (selected >= (int)entries.size())
                    selected = (int)entries.size() - 1;
                loadPreview();
            }
            confirmingDelete = false;
        }
        return;
    }

    if (btnBack().contains(px, py)) { close(); return; }
    if (btnNew().contains(px, py)) { scene.newProject(); close(); return; }
    if (selected >= 0 && btnOpen().contains(px, py)) { scene.load(entries[selected].path); close(); return; }
    if (selected >= 0 && btnDelete().contains(px, py)) { confirmingDelete = true; return; }

    // list press: candidate for tap-select or scroll-drag
    if (py >= kListTop && py < kListBottom)
    {
        pressedList = true;
        pressY = py;
        pressScroll = scroll;
        pressedRow = rowAt(py); // touch-up reports (0,0), so capture it now
    }
}

void ProjectBrowser::handleTouchMove(int px, int py)
{
    (void)px;
    if (!pressedList)
        return;
    const int d = py - pressY;
    if (!dragging && (d > 4 || d < -4))
        dragging = true;
    if (dragging)
    {
        scroll = pressScroll - (float)d;
        const float m = maxScroll();
        if (scroll < 0.0f)
            scroll = 0.0f;
        if (scroll > m)
            scroll = m;
    }
}

void ProjectBrowser::handleTouchUp(int px, int py)
{
    (void)px;
    (void)py;
    if (pressedList && !dragging && pressedRow >= 0)
    {
        selected = pressedRow;
        loadPreview();
    }
    pressedList = false;
    dragging = false;
}
