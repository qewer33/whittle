#include "viewrender.h"
#include <math.h>

static const u32 kGridCol = 0x2E3440FF;
static const u32 kEdge = 0xB8C0D0FF;
static const u32 kEdgeSel = 0xFFD24AFF;
static const u32 kMarker = 0xFFD24AFF;
static const u32 kNormal = 0x5BD98BFF; // face-normal arrow (extrude direction)

static float snapToGrid(float v, float grid)
{
    return roundf(v / grid) * grid;
}

static Vec3 vsub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 vcross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static Vec3 vnorm(const Vec3& a)
{
    const float l = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 1e-6f ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0, 0, 0};
}

static void pushGrid(const Viewport& vp, float gridSpacing, Renderer& r)
{
    if (gridSpacing <= 0.0f)
        return;
    const float minX = vp.centerX - vp.spanX() / 2.0f;
    const float maxX = vp.centerX + vp.spanX() / 2.0f;
    const float minY = vp.centerY - vp.spanY() / 2.0f;
    const float maxY = vp.centerY + vp.spanY() / 2.0f;

    const float startX = floorf(minX / gridSpacing) * gridSpacing;
    const float startY = floorf(minY / gridSpacing) * gridSpacing;

    Line lines[128];
    int n = 0;

    for (float x = startX; x <= maxX + 0.001f && n < 120; x += gridSpacing)
    {
        const float xp = snapToGrid(x, gridSpacing);
        if (xp < minX - 0.001f)
            continue;
        Vec3 a = {0, 0, 0}, b = {0, 0, 0};
        setAxis(a, vp.axisX, xp);
        setAxis(b, vp.axisX, xp);
        setAxis(a, vp.axisY, minY);
        setAxis(b, vp.axisY, maxY);
        lines[n++] = {a, b};
    }
    for (float y = startY; y <= maxY + 0.001f && n < 120; y += gridSpacing)
    {
        const float yp = snapToGrid(y, gridSpacing);
        if (yp < minY - 0.001f)
            continue;
        Vec3 a = {0, 0, 0}, b = {0, 0, 0};
        setAxis(a, vp.axisX, minX);
        setAxis(b, vp.axisX, maxX);
        setAxis(a, vp.axisY, yp);
        setAxis(b, vp.axisY, yp);
        lines[n++] = {a, b};
    }

    const C3D_Mtx m = vp.matrix();
    r.drawLineSet(lines, n, m, kGridCol, 240, 320);
}

static void pushMeshLines(const Viewport& vp, const Mesh& mesh, Renderer& r, u32 color)
{
    Line lines[256];
    int n = 0;
    const C3D_Mtx m = vp.matrix();
    auto flush = [&]() {
        if (n > 0)
        {
            r.drawLineSet(lines, n, m, color, 240, 320);
            n = 0;
        }
    };
    for (const Face& face : mesh.faces)
    {
        for (int e = 0; e < 4; e++)
        {
            lines[n].a = mesh.positions[face.indices[e]];
            lines[n].b = mesh.positions[face.indices[(e + 1) % 4]];
            n++;
            if (n == 256)
                flush();
        }
    }
    flush();
}

static void pushVertexMarkers(const Viewport& vp, const Scene& scene, Renderer& r)
{
    const float len = 0.3f;
    const C3D_Mtx m = vp.matrix();
    for (const VertRef& vr : scene.selectedVerts)
    {
        if (vr.obj < 0 || vr.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[vr.obj];
        if (vr.vert < 0 || vr.vert >= (int)mesh.positions.size())
            continue;
        const Vec3& p = mesh.positions[vr.vert];
        Line lines[2] = {{p, p}, {p, p}};
        setAxis(lines[0].a, vp.axisX, getAxis(p, vp.axisX) - len);
        setAxis(lines[0].b, vp.axisX, getAxis(p, vp.axisX) + len);
        setAxis(lines[1].a, vp.axisY, getAxis(p, vp.axisY) - len);
        setAxis(lines[1].b, vp.axisY, getAxis(p, vp.axisY) + len);
        r.drawLineSet(lines, 2, m, kMarker, 240, 320);
    }
}

static void appendCrossMarker(const Viewport& vp, const Vec3& p, float len,
                              Line* lines, int& n)
{
    Vec3 a = p, b = p, c = p, d = p;
    setAxis(a, vp.axisX, getAxis(p, vp.axisX) - len);
    setAxis(b, vp.axisX, getAxis(p, vp.axisX) + len);
    setAxis(c, vp.axisY, getAxis(p, vp.axisY) - len);
    setAxis(d, vp.axisY, getAxis(p, vp.axisY) + len);
    lines[n++] = {a, b};
    lines[n++] = {c, d};
}

static void pushSelectedEdgeMarkers(const Viewport& vp, const Scene& scene, Renderer& r)
{
    Line lines[256];
    int n = 0;
    const C3D_Mtx m = vp.matrix();
    auto flush = [&]() {
        if (n > 0)
        {
            r.drawLineSet(lines, n, m, kMarker, 240, 320);
            n = 0;
        }
    };
    auto emit = [&](const Vec3& p) {
        if (n > 254)
            flush();
        appendCrossMarker(vp, p, 0.3f, lines, n);
    };
    for (const EdgeRef& er : scene.selectedEdges)
    {
        if (er.obj < 0 || er.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[er.obj];
        if (er.v0 < 0 || er.v0 >= (int)mesh.positions.size() || er.v1 < 0 ||
            er.v1 >= (int)mesh.positions.size())
            continue;
        emit(mesh.positions[er.v0]);
        emit(mesh.positions[er.v1]);
    }
    flush();
}

// selected edges, drawn in the highlight color over the grey wireframe
static void pushSelectedEdges(const Viewport& vp, const Scene& scene, Renderer& r)
{
    Line lines[256];
    int n = 0;
    const C3D_Mtx m = vp.matrix();
    auto flush = [&]() {
        if (n > 0)
        {
            r.drawLineSet(lines, n, m, kEdgeSel, 240, 320);
            n = 0;
        }
    };
    auto add = [&](const Line& line) {
        if (n == 256)
            flush();
        lines[n++] = line;
    };
    for (const EdgeRef& er : scene.selectedEdges)
    {
        if (er.obj < 0 || er.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[er.obj];
        if (er.v0 < 0 || er.v0 >= (int)mesh.positions.size())
            continue;
        if (er.v1 < 0 || er.v1 >= (int)mesh.positions.size())
            continue;
        add({mesh.positions[er.v0], mesh.positions[er.v1]});
    }
    flush();
}

// selected faces: highlight the border plus the two diagonals so they read as a
// filled selection rather than four separate edges
static void pushSelectedFaces(const Viewport& vp, const Scene& scene, Renderer& r)
{
    Line lines[256];
    int n = 0;
    const C3D_Mtx m = vp.matrix();
    auto flush = [&]() {
        if (n > 0)
        {
            r.drawLineSet(lines, n, m, kEdgeSel, 240, 320);
            n = 0;
        }
    };
    auto add = [&](const Line& line) {
        if (n == 256)
            flush();
        lines[n++] = line;
    };
    for (const FaceRef& fr : scene.selectedFaces)
    {
        if (fr.obj < 0 || fr.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[fr.obj];
        if (fr.face < 0 || fr.face >= (int)mesh.faces.size())
            continue;
        const Face& f = mesh.faces[fr.face];
        for (int e = 0; e < 4; e++)
            add({mesh.positions[f.indices[e]], mesh.positions[f.indices[(e + 1) % 4]]});
        add({mesh.positions[f.indices[0]], mesh.positions[f.indices[2]]});
        add({mesh.positions[f.indices[1]], mesh.positions[f.indices[3]]});
    }
    flush();
}

static void pushSelectedFaceMarkers(const Viewport& vp, const Scene& scene, Renderer& r)
{
    Line lines[256];
    int n = 0;
    const C3D_Mtx m = vp.matrix();
    auto flush = [&]() {
        if (n > 0)
        {
            r.drawLineSet(lines, n, m, kMarker, 240, 320);
            n = 0;
        }
    };
    auto emit = [&](const Vec3& p, float len) {
        if (n > 254)
            flush();
        appendCrossMarker(vp, p, len, lines, n);
    };
    for (const FaceRef& fr : scene.selectedFaces)
    {
        if (fr.obj < 0 || fr.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[fr.obj];
        if (fr.face < 0 || fr.face >= (int)mesh.faces.size())
            continue;
        const Face& f = mesh.faces[fr.face];
        Vec3 center = {0.0f, 0.0f, 0.0f};
        for (int k = 0; k < 4; k++)
        {
            const Vec3& p = mesh.positions[f.indices[k]];
            emit(p, 0.22f);
            center.x += p.x * 0.25f;
            center.y += p.y * 0.25f;
            center.z += p.z * 0.25f;
        }
        emit(center, 0.34f); // remains available when the face is edge-on
    }
    flush();
}

// outward-normal arrow at each selected face centroid (the extrude direction).
// the head spreads in the viewport plane, edge-on views collapse it to a dot.
static void pushFaceNormalArrows(const Viewport& vp, const Scene& scene, Renderer& r)
{
    const float shaft = 0.9f, head = 0.3f;
    const float ca = 0.906f, sa = 0.423f; // cos/sin 25 deg, arrowhead spread
    Line lines[256];
    int n = 0;
    for (const FaceRef& fr : scene.selectedFaces)
    {
        if (fr.obj < 0 || fr.obj >= (int)scene.objects.size())
            continue;
        const Mesh& mesh = scene.objects[fr.obj];
        if (fr.face < 0 || fr.face >= (int)mesh.faces.size())
            continue;
        const Face& f = mesh.faces[fr.face];
        const Vec3& p0 = mesh.positions[f.indices[0]];
        const Vec3& p1 = mesh.positions[f.indices[1]];
        const Vec3& p2 = mesh.positions[f.indices[2]];
        Vec3 c = {0, 0, 0};
        for (int k = 0; k < 4; k++)
        {
            const Vec3& p = mesh.positions[f.indices[k]];
            c.x += p.x * 0.25f;
            c.y += p.y * 0.25f;
            c.z += p.z * 0.25f;
        }
        const Vec3 nrm = vnorm(vcross(vsub(p1, p0), vsub(p2, p0))); // outward (CCW)
        if (nrm.x == 0 && nrm.y == 0 && nrm.z == 0)
            continue;
        const Vec3 tip = {c.x + nrm.x * shaft, c.y + nrm.y * shaft, c.z + nrm.z * shaft};
        if (n < 256)
            lines[n++] = {c, tip};

        // arrowhead barbs, spread in this viewport's plane (skip if edge-on)
        float nx = getAxis(nrm, vp.axisX), ny = getAxis(nrm, vp.axisY);
        const float il = sqrtf(nx * nx + ny * ny);
        if (il > 1e-3f)
        {
            nx /= il;
            ny /= il;
            const float bx = -nx, by = -ny; // back toward the base
            const float a1x = bx * ca - by * sa, a1y = bx * sa + by * ca;
            const float a2x = bx * ca + by * sa, a2y = -bx * sa + by * ca;
            Vec3 b1 = tip, b2 = tip;
            setAxis(b1, vp.axisX, getAxis(tip, vp.axisX) + a1x * head);
            setAxis(b1, vp.axisY, getAxis(tip, vp.axisY) + a1y * head);
            setAxis(b2, vp.axisX, getAxis(tip, vp.axisX) + a2x * head);
            setAxis(b2, vp.axisY, getAxis(tip, vp.axisY) + a2y * head);
            if (n < 256)
                lines[n++] = {tip, b1};
            if (n < 256)
                lines[n++] = {tip, b2};
        }
    }
    if (n)
        r.drawLineSet(lines, n, vp.matrix(), kNormal, 240, 320);
}

void viewrender::render(const Scene& scene, const Viewport viewports[3],
                        EditMode mode, SubLevel subLevel, bool extruding,
                        bool showFaces, int onlyView, float gridSpacing, Renderer& r)
{
    for (int i = 0; i < 3; i++)
    {
        if (onlyView >= 0 && i != onlyView) // a maximized view fills the area
            continue;
        const Viewport& vp = viewports[i];
        r.setBottomScissor(vp.x, vp.y, vp.w, vp.h);

        pushGrid(vp, gridSpacing, r);
        if (showFaces)
            for (const Mesh& m : scene.objects)
                r.drawFaces(m, vp.matrix(), GPU_CULL_NONE); // depth-sorted, double-sided
        const bool objMode = mode == EditMode::Object;
        for (int o = 0; o < (int)scene.objects.size(); o++)
        {
            const bool hi = objMode && scene.isObjectSelected(o);
            pushMeshLines(vp, scene.objects[o], r, hi ? kEdgeSel : kEdge);
        }
        if (mode == EditMode::Edit)
        {
            if (subLevel == SubLevel::Vertex)
                pushVertexMarkers(vp, scene, r);
            else if (subLevel == SubLevel::Edge)
            {
                pushSelectedEdges(vp, scene, r);
                pushSelectedEdgeMarkers(vp, scene, r);
            }
            else
            {
                pushSelectedFaces(vp, scene, r);
                pushSelectedFaceMarkers(vp, scene, r);
                if (extruding)
                    pushFaceNormalArrows(vp, scene, r);
            }
        }
    }
    r.clearScissor();
}
