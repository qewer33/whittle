#include "scene.h"
#include <algorithm>
#include <array>
#include <stdio.h>

// scene save file on the SD card
static const char* const kSavePath = "sdmc:/m1model.dat";
static const u32 kSaveMagic = 0x314D444D; // "MDM1"
static const u32 kSaveVersion = 3;

Scene::Scene()
{
    texture.resize(kTexSize * kTexSize);
    for (int y = 0; y < kTexSize; y++)
        for (int x = 0; x < kTexSize; x++)
            texture[y * kTexSize + x] = (((x >> 4) + (y >> 4)) & 1) ? 0xef7d57FF : 0x257179FF;
}

void Scene::clampActive()
{
    if (activeObject >= (int)objects.size())
        activeObject = (int)objects.size() - 1;
    if (activeObject < 0)
        activeObject = 0;
}

void Scene::snapshot()
{
    undoStack.push_back({objects, texture});
    if ((int)undoStack.size() > kMaxUndo)
        undoStack.erase(undoStack.begin());
    redoStack.clear(); // a new edit kills the redo history
}

void Scene::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back({objects, texture});
    objects = undoStack.back().objects;
    texture = undoStack.back().texture;
    undoStack.pop_back();
    textureDirty = true;
    clearSelection();
    clampActive();
}

void Scene::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back({objects, texture});
    objects = redoStack.back().objects;
    texture = redoStack.back().texture;
    redoStack.pop_back();
    textureDirty = true;
    clearSelection();
    clampActive();
}

void Scene::clearSelection()
{
    selectedVerts.clear();
    selectedEdges.clear();
    selectedFaces.clear();
    selectedObjects.clear();
}

bool Scene::isObjectSelected(int obj) const
{
    return std::find(selectedObjects.begin(), selectedObjects.end(), obj) !=
           selectedObjects.end();
}

bool Scene::isVertSelected(int obj, int vert) const
{
    for (const VertRef& vr : selectedVerts)
        if (vr.obj == obj && vr.vert == vert)
            return true;
    return false;
}

bool Scene::isEdgeSelected(int obj, int a, int b) const
{
    for (const EdgeRef& er : selectedEdges)
        if (er.obj == obj && ((er.v0 == a && er.v1 == b) || (er.v0 == b && er.v1 == a)))
            return true;
    return false;
}

bool Scene::isFaceSelected(int obj, int face) const
{
    for (const FaceRef& fr : selectedFaces)
        if (fr.obj == obj && fr.face == face)
            return true;
    return false;
}

Vec3 Scene::selectionCentroid() const
{
    Vec3 c = {0.0f, 0.0f, 0.0f};
    int n = 0;
    for (int o : selectedObjects)
    {
        if (o < 0 || o >= (int)objects.size())
            continue;
        for (const Vec3& p : objects[o].positions)
        {
            c.x += p.x;
            c.y += p.y;
            c.z += p.z;
            n++;
        }
    }
    if (n > 0)
    {
        c.x /= n;
        c.y /= n;
        c.z /= n;
    }
    return c;
}

int Scene::addShape(int kind)
{
    Mesh m;
    switch (kind)
    {
    case 0: m = makeCube(1.0f); break;
    case 1: m = makeSphere(0.5f); break;
    case 2: m = makePyramid(1.0f); break;
    case 3: m = makeCylinder(0.5f, 1.0f); break;
    case 4: m = makePlane(1.0f); break;
    default: return -1;
    }

    snapshot();
    objects.push_back(m);
    activeObject = (int)objects.size() - 1;
    clearSelection();
    return activeObject;
}

void Scene::deleteSelectedObjects()
{
    if (selectedObjects.empty())
        return;
    snapshot();
    std::vector<int> idx = selectedObjects;
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int o : idx)
        if (o >= 0 && o < (int)objects.size())
            objects.erase(objects.begin() + o);
    clearSelection();
    clampActive();
}

void Scene::deleteSelectedVerts()
{
    if (selectedVerts.empty())
        return;
    snapshot();
    // remove high indices first; removeVertex shifts the rest down
    for (int o = 0; o < (int)objects.size(); o++)
    {
        std::vector<int> vs;
        for (const VertRef& vr : selectedVerts)
            if (vr.obj == o)
                vs.push_back(vr.vert);
        std::sort(vs.begin(), vs.end(), std::greater<int>());
        for (int v : vs)
            objects[o].removeVertex(v);
    }
    clearSelection();
}

// subdivide each selected face into 4
void Scene::subdivideSelectedFaces()
{
    if (selectedFaces.empty())
        return;
    snapshot();

    for (int o = 0; o < (int)objects.size(); o++)
    {
        std::vector<int> sel;
        for (const FaceRef& fr : selectedFaces)
            if (fr.obj == o && fr.face >= 0 && fr.face < (int)objects[o].faces.size())
                sel.push_back(fr.face);
        if (sel.empty())
            continue;

        Mesh& m = objects[o];
        std::vector<std::array<int, 3>> midCache; // {loVert, hiVert, newVert}
        auto edgeMid = [&](int a, int b) {
            const int lo = a < b ? a : b, hi = a < b ? b : a;
            for (const auto& e : midCache)
                if (e[0] == lo && e[1] == hi)
                    return e[2];
            const Vec3 pa = m.positions[a], pb = m.positions[b];
            const int idx = (int)m.positions.size();
            m.positions.push_back({(pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f, (pa.z + pb.z) * 0.5f});
            midCache.push_back({lo, hi, idx});
            return idx;
        };

        std::vector<Face> extra; // sub faces past the first (appended after)
        for (int fi : sel)
        {
            const Face f = m.faces[fi]; // copy: the slot gets overwritten below
            const int i0 = f.indices[0], i1 = f.indices[1], i2 = f.indices[2], i3 = f.indices[3];
            const bool tri = (i3 == i2);

            auto uvMid = [](const float a[2], const float b[2], float out[2]) {
                out[0] = (a[0] + b[0]) * 0.5f;
                out[1] = (a[1] + b[1]) * 0.5f;
            };
            auto mk = [&](int a, int b, int c, int d, const float ua[2],
                          const float ub[2], const float uc[2], const float ud[2]) {
                Face nf;
                nf.indices[0] = a; nf.indices[1] = b; nf.indices[2] = c; nf.indices[3] = d;
                nf.color = f.color;
                nf.textured = f.textured;
                nf.uv[0][0] = ua[0]; nf.uv[0][1] = ua[1];
                nf.uv[1][0] = ub[0]; nf.uv[1][1] = ub[1];
                nf.uv[2][0] = uc[0]; nf.uv[2][1] = uc[1];
                nf.uv[3][0] = ud[0]; nf.uv[3][1] = ud[1];
                return nf;
            };

            Face sub[4];
            if (!tri)
            {
                const int a01 = edgeMid(i0, i1), a12 = edgeMid(i1, i2),
                          a23 = edgeMid(i2, i3), a30 = edgeMid(i3, i0);
                const Vec3 p0 = m.positions[i0], p1 = m.positions[i1],
                           p2 = m.positions[i2], p3 = m.positions[i3];
                const int cc = (int)m.positions.size();
                m.positions.push_back({(p0.x + p1.x + p2.x + p3.x) * 0.25f,
                                       (p0.y + p1.y + p2.y + p3.y) * 0.25f,
                                       (p0.z + p1.z + p2.z + p3.z) * 0.25f});
                float u01[2], u12[2], u23[2], u30[2], uc[2];
                uvMid(f.uv[0], f.uv[1], u01);
                uvMid(f.uv[1], f.uv[2], u12);
                uvMid(f.uv[2], f.uv[3], u23);
                uvMid(f.uv[3], f.uv[0], u30);
                uc[0] = (f.uv[0][0] + f.uv[1][0] + f.uv[2][0] + f.uv[3][0]) * 0.25f;
                uc[1] = (f.uv[0][1] + f.uv[1][1] + f.uv[2][1] + f.uv[3][1]) * 0.25f;
                sub[0] = mk(i0, a01, cc, a30, f.uv[0], u01, uc, u30);
                sub[1] = mk(a01, i1, a12, cc, u01, f.uv[1], u12, uc);
                sub[2] = mk(cc, a12, i2, a23, uc, u12, f.uv[2], u23);
                sub[3] = mk(a30, cc, a23, i3, u30, uc, u23, f.uv[3]);
            }
            else
            {
                const int a01 = edgeMid(i0, i1), a12 = edgeMid(i1, i2), a20 = edgeMid(i2, i0);
                float u01[2], u12[2], u20[2];
                uvMid(f.uv[0], f.uv[1], u01);
                uvMid(f.uv[1], f.uv[2], u12);
                uvMid(f.uv[2], f.uv[0], u20);
                sub[0] = mk(i0, a01, a20, a20, f.uv[0], u01, u20, u20);
                sub[1] = mk(a01, i1, a12, a12, u01, f.uv[1], u12, u12);
                sub[2] = mk(a20, a12, i2, i2, u20, u12, f.uv[2], f.uv[2]);
                sub[3] = mk(a01, a12, a20, a20, u01, u12, u20, u20); // center
            }

            m.faces[fi] = sub[0];
            extra.push_back(sub[1]);
            extra.push_back(sub[2]);
            extra.push_back(sub[3]);
        }
        for (const Face& e : extra)
            m.faces.push_back(e);
    }
    clearSelection();
}

// loop cut from each selected edge
void Scene::splitSelectedEdges()
{
    if (selectedEdges.empty())
        return;
    snapshot();

    for (int o = 0; o < (int)objects.size(); o++)
    {
        Mesh& m = objects[o];
        auto isTri = [](const Face& f) { return f.indices[3] == f.indices[2]; };

        // ring = the set of edges the cut passes through (sorted vertex pairs)
        std::vector<std::array<int, 2>> ring;
        auto key = [](int a, int b) { return std::array<int, 2>{a < b ? a : b, a < b ? b : a}; };
        auto inRing = [&](int a, int b) {
            const auto k = key(a, b);
            for (const auto& e : ring)
                if (e == k)
                    return true;
            return false;
        };

        std::vector<std::array<int, 2>> frontier;
        for (const EdgeRef& er : selectedEdges)
            if (er.obj == o && !inRing(er.v0, er.v1))
            {
                ring.push_back(key(er.v0, er.v1));
                frontier.push_back({er.v0, er.v1});
            }
        if (ring.empty())
            continue;

        // grow the ring: in each quad, an edge's opposite edge joins the loop
        while (!frontier.empty())
        {
            const auto cur = frontier.back();
            frontier.pop_back();
            for (const Face& f : m.faces)
            {
                if (isTri(f))
                    continue;
                int pos = -1;
                for (int k = 0; k < 4; k++)
                {
                    const int x = f.indices[k], y = f.indices[(k + 1) % 4];
                    if ((x == cur[0] && y == cur[1]) || (x == cur[1] && y == cur[0])) { pos = k; break; }
                }
                if (pos < 0)
                    continue;
                const int ox = f.indices[(pos + 2) % 4], oy = f.indices[(pos + 3) % 4];
                if (!inRing(ox, oy))
                {
                    ring.push_back(key(ox, oy));
                    frontier.push_back({ox, oy});
                }
            }
        }

        std::vector<std::array<int, 3>> midCache; // {loVert, hiVert, newVert}
        auto edgeMid = [&](int a, int b) {
            const int lo = a < b ? a : b, hi = a < b ? b : a;
            for (const auto& e : midCache)
                if (e[0] == lo && e[1] == hi)
                    return e[2];
            const Vec3 pa = m.positions[a], pb = m.positions[b];
            const int idx = (int)m.positions.size();
            m.positions.push_back({(pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f, (pa.z + pb.z) * 0.5f});
            midCache.push_back({lo, hi, idx});
            return idx;
        };

        const int origCount = (int)m.faces.size();
        std::vector<Face> extra;
        for (int fi = 0; fi < origCount; fi++)
        {
            const Face f = m.faces[fi];
            if (isTri(f))
                continue;
            // a cut quad has two opposite edges in the ring: (0,1)+(2,3) or (1,2)+(3,0)
            int p = -1;
            if (inRing(f.indices[0], f.indices[1]) && inRing(f.indices[2], f.indices[3]))
                p = 0;
            else if (inRing(f.indices[1], f.indices[2]) && inRing(f.indices[3], f.indices[0]))
                p = 1;
            if (p < 0)
                continue;

            const int A = f.indices[p], B = f.indices[(p + 1) % 4],
                      C = f.indices[(p + 2) % 4], D = f.indices[(p + 3) % 4];
            const float *uA = f.uv[p], *uB = f.uv[(p + 1) % 4],
                        *uC = f.uv[(p + 2) % 4], *uD = f.uv[(p + 3) % 4];
            const int mAB = edgeMid(A, B), mCD = edgeMid(C, D);
            float uAB[2], uCD[2];
            uAB[0] = (uA[0] + uB[0]) * 0.5f; uAB[1] = (uA[1] + uB[1]) * 0.5f;
            uCD[0] = (uC[0] + uD[0]) * 0.5f; uCD[1] = (uC[1] + uD[1]) * 0.5f;
            auto mk = [&](int a, int b, int c, int d, const float ua[2],
                          const float ub[2], const float uc[2], const float ud[2]) {
                Face nf;
                nf.indices[0] = a; nf.indices[1] = b; nf.indices[2] = c; nf.indices[3] = d;
                nf.color = f.color;
                nf.textured = f.textured;
                nf.uv[0][0] = ua[0]; nf.uv[0][1] = ua[1];
                nf.uv[1][0] = ub[0]; nf.uv[1][1] = ub[1];
                nf.uv[2][0] = uc[0]; nf.uv[2][1] = uc[1];
                nf.uv[3][0] = ud[0]; nf.uv[3][1] = ud[1];
                return nf;
            };
            m.faces[fi] = mk(A, mAB, mCD, D, uA, uAB, uCD, uD);
            extra.push_back(mk(mAB, B, C, mCD, uAB, uB, uC, uCD));
        }
        for (const Face& e : extra)
            m.faces.push_back(e);
    }
    clearSelection();
}

void Scene::deleteSelectedFaces()
{
    if (selectedFaces.empty())
        return;
    snapshot();
    for (int o = 0; o < (int)objects.size(); o++)
    {
        std::vector<int> fs;
        for (const FaceRef& fr : selectedFaces)
            if (fr.obj == o && fr.face >= 0 && fr.face < (int)objects[o].faces.size())
                fs.push_back(fr.face);
        // remove high indices first so the rest don't shift
        std::sort(fs.begin(), fs.end(), std::greater<int>());
        fs.erase(std::unique(fs.begin(), fs.end()), fs.end());
        for (int f : fs)
            objects[o].faces.erase(objects[o].faces.begin() + f);
    }
    clearSelection();
}

void Scene::deleteSelectedEdges()
{
    if (selectedEdges.empty())
        return;
    snapshot();
    for (int o = 0; o < (int)objects.size(); o++)
    {
        Mesh& m = objects[o];
        std::vector<int> fs; // faces using any selected edge in this object
        for (int fi = 0; fi < (int)m.faces.size(); fi++)
        {
            const Face& f = m.faces[fi];
            bool hit = false;
            for (const EdgeRef& er : selectedEdges)
            {
                if (er.obj != o)
                    continue;
                for (int k = 0; k < 4; k++)
                {
                    const int a = f.indices[k], b = f.indices[(k + 1) % 4];
                    if (a == b)
                        continue;
                    if ((a == er.v0 && b == er.v1) || (a == er.v1 && b == er.v0))
                    {
                        hit = true;
                        break;
                    }
                }
                if (hit)
                    break;
            }
            if (hit)
                fs.push_back(fi);
        }
        std::sort(fs.begin(), fs.end(), std::greater<int>());
        for (int f : fs)
            m.faces.erase(m.faces.begin() + f);
    }
    clearSelection();
}

void Scene::textureAllFaces()
{
    for (Mesh& m : objects)
        for (Face& f : m.faces)
            f.textured = true;
}

void Scene::untextureAllFaces()
{
    for (Mesh& m : objects)
        for (Face& f : m.faces)
            f.textured = false;
}

// extrude every selected face
void Scene::extrudeSelectedFaces()
{
    if (selectedFaces.empty())
        return;
    snapshot();

    for (int o = 0; o < (int)objects.size(); o++)
    {
        std::vector<int> sel; // selected face indices in this object
        for (const FaceRef& fr : selectedFaces)
            if (fr.obj == o && fr.face >= 0 && fr.face < (int)objects[o].faces.size())
                sel.push_back(fr.face);
        if (sel.empty())
            continue;

        Mesh& m = objects[o];

        // capture originals up front (indices get remapped below)
        std::vector<std::array<int, 4>> orig(sel.size());
        for (size_t i = 0; i < sel.size(); i++)
            for (int k = 0; k < 4; k++)
                orig[i][k] = m.faces[sel[i]].indices[k];

        // how many selected faces touch an edge: 1 = border (wall it), 2 = interior
        auto edgeCount = [&](int a, int b) {
            int c = 0;
            for (const auto& q : orig)
                for (int k = 0; k < 4; k++)
                {
                    const int x = q[k], y = q[(k + 1) % 4];
                    if (x == y)
                        continue;
                    if ((x == a && y == b) || (x == b && y == a)) { c++; break; }
                }
            return c;
        };

        // one new vertex per original (shared verts stay welded across the cap)
        std::vector<int> toNew(m.positions.size(), -1);
        auto mapVert = [&](int vi) {
            if (toNew[vi] < 0)
            {
                const Vec3 p = m.positions[vi];
                toNew[vi] = (int)m.positions.size();
                m.positions.push_back(p);
            }
            return toNew[vi];
        };

        std::vector<Face> walls;
        for (size_t i = 0; i < sel.size(); i++)
        {
            const std::array<int, 4>& q = orig[i];
            Face& f = m.faces[sel[i]];
            for (int k = 0; k < 4; k++)
            {
                const int v0 = q[k], v1 = q[(k + 1) % 4];
                if (v0 == v1 || edgeCount(v0, v1) != 1)
                    continue; // degenerate, or interior shared edge
                const int n0 = mapVert(v0), n1 = mapVert(v1);
                Face w;
                // standard extrude side winding: base edge in face order, then
                // the cap edge reversed, so the wall faces outward
                w.indices[0] = v0;
                w.indices[1] = v1;
                w.indices[2] = n1;
                w.indices[3] = n0;
                w.color = f.color;
                w.textured = false;
                w.uv[0][0] = 0; w.uv[0][1] = 1;
                w.uv[1][0] = 1; w.uv[1][1] = 1;
                w.uv[2][0] = 1; w.uv[2][1] = 0;
                w.uv[3][0] = 0; w.uv[3][1] = 0;
                walls.push_back(w);
            }
            // the face itself becomes the cap, on the new verts
            for (int k = 0; k < 4; k++)
                f.indices[k] = mapVert(q[k]);
        }
        for (const Face& w : walls)
            m.faces.push_back(w);
    }
}

bool Scene::save() const
{
    FILE* f = fopen(kSavePath, "wb");
    if (!f)
        return false;
    fwrite(&kSaveMagic, sizeof(u32), 1, f);
    fwrite(&kSaveVersion, sizeof(u32), 1, f);
    const u32 nobj = (u32)objects.size();
    fwrite(&nobj, sizeof(u32), 1, f);
    for (const Mesh& m : objects)
    {
        const u32 pc = (u32)m.positions.size();
        fwrite(&pc, sizeof(u32), 1, f);
        if (pc)
            fwrite(m.positions.data(), sizeof(Vec3), pc, f);
        const u32 fc = (u32)m.faces.size();
        fwrite(&fc, sizeof(u32), 1, f);
        // field-by-field so the file is independent of struct padding
        for (const Face& fa : m.faces)
        {
            fwrite(fa.indices, sizeof(int), 4, f);
            fwrite(&fa.color, sizeof(u32), 1, f);
            const u8 tx = fa.textured ? 1 : 0;
            fwrite(&tx, 1, 1, f);
            fwrite(fa.uv, sizeof(float), 8, f);
        }
    }
    fwrite(texture.data(), sizeof(u32), texture.size(), f);
    fclose(f);
    return true;
}

bool Scene::load()
{
    FILE* f = fopen(kSavePath, "rb");
    if (!f)
        return false;
    u32 magic = 0, version = 0, nobj = 0;
    if (fread(&magic, sizeof(u32), 1, f) != 1 || magic != kSaveMagic ||
        fread(&version, sizeof(u32), 1, f) != 1 || version < 1 || version > kSaveVersion ||
        fread(&nobj, sizeof(u32), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    std::vector<Mesh> loaded;
    loaded.reserve(nobj);
    for (u32 o = 0; o < nobj; o++)
    {
        Mesh m;
        u32 pc = 0, fc = 0;
        if (fread(&pc, sizeof(u32), 1, f) != 1)
        {
            fclose(f);
            return false;
        }
        m.positions.resize(pc);
        if (pc && fread(m.positions.data(), sizeof(Vec3), pc, f) != pc)
        {
            fclose(f);
            return false;
        }
        if (fread(&fc, sizeof(u32), 1, f) != 1)
        {
            fclose(f);
            return false;
        }
        for (u32 fi = 0; fi < fc; fi++)
        {
            Face fa; // defaults: textured=false, uv=0
            if (fread(fa.indices, sizeof(int), 4, f) != 4 ||
                fread(&fa.color, sizeof(u32), 1, f) != 1)
            {
                fclose(f);
                return false;
            }
            if (version >= 2)
            {
                u8 tx = 0;
                if (fread(&tx, 1, 1, f) != 1 || fread(fa.uv, sizeof(float), 8, f) != 8)
                {
                    fclose(f);
                    return false;
                }
                fa.textured = tx != 0;
            }
            m.faces.push_back(fa);
        }
        loaded.push_back(std::move(m));
    }

    std::vector<u32> loadedTex;
    if (version >= 3)
    {
        loadedTex.resize(kTexSize * kTexSize);
        if (fread(loadedTex.data(), sizeof(u32), loadedTex.size(), f) != loadedTex.size())
        {
            fclose(f);
            return false;
        }
    }
    fclose(f);

    snapshot(); // load is undoable
    objects = std::move(loaded);
    if (!loadedTex.empty())
        texture = std::move(loadedTex);
    textureDirty = true;
    clearSelection();
    activeObject = 0;
    return true;
}
