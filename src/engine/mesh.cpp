#include "mesh.h"
#include <algorithm>
#include <math.h>

void MeshTopology::rebuild(const Mesh& mesh)
{
    edges.clear();

    auto addUnique = [](std::vector<int>& values, int value) {
        if (std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(value);
    };

    for (int fi = 0; fi < (int)mesh.faces.size(); fi++)
    {
        const Face& f = mesh.faces[fi];
        for (int k = 0; k < 4; k++)
        {
            const int v = f.indices[k];
            const int next = f.indices[(k + 1) & 3];
            if (v == next || v < 0 || next < 0 || v >= (int)mesh.positions.size() ||
                next >= (int)mesh.positions.size())
                continue;
            const int lo = v < next ? v : next;
            const int hi = v < next ? next : v;
            int ei = findEdge(lo, hi);
            if (ei < 0)
            {
                ei = (int)edges.size();
                edges.push_back({lo, hi, {}});
            }
            addUnique(edges[ei].faces, fi);
        }
    }
}

int MeshTopology::findEdge(int a, int b) const
{
    const int lo = a < b ? a : b, hi = a < b ? b : a;
    for (int i = 0; i < (int)edges.size(); i++)
        if (edges[i].v0 == lo && edges[i].v1 == hi)
            return i;
    return -1;
}

int Mesh::addVertex(Vec3 p)
{
    positions.push_back(p);
    return (int)positions.size() - 1;
}

int Mesh::removeVertex(int idx)
{
    if (idx < 0 || idx >= (int)positions.size())
        return 0;

    std::vector<Face> kept;
    kept.reserve(faces.size());
    for (const Face& f : faces)
    {
        bool uses = false;
        for (int i = 0; i < 4; i++)
            if (f.indices[i] == idx)
            {
                uses = true;
                break;
            }
        if (uses)
            continue;
        Face remapped = f;
        for (int i = 0; i < 4; i++)
            if (remapped.indices[i] > idx)
                remapped.indices[i]--;
        kept.push_back(remapped);
    }
    faces = std::move(kept);
    positions.erase(positions.begin() + idx);
    return 1;
}

void Mesh::moveVertex(int idx, Vec3 p)
{
    if (idx < 0 || idx >= (int)positions.size())
        return;
    positions[idx] = p;
}

Mesh makeCube(float size)
{
    Mesh mesh;
    const float s = size * 0.5f;

    mesh.positions = {
        {-s, -s, -s}, // 0
        { s, -s, -s}, // 1
        { s,  s, -s}, // 2
        {-s,  s, -s}, // 3
        {-s, -s,  s}, // 4
        { s, -s,  s}, // 5
        { s,  s,  s}, // 6
        {-s,  s,  s}, // 7
    };

    mesh.faces = {
        {{4, 5, 6, 7}, 0xFF4040FF}, // +Z red
        {{1, 0, 3, 2}, 0xFFA040FF}, // -Z orange
        {{5, 1, 2, 6}, 0xFFFF40FF}, // +X yellow
        {{0, 4, 7, 3}, 0x40FF40FF}, // -X green
        {{2, 3, 7, 6}, 0x4080FFFF}, // +Y blue
        {{1, 5, 4, 0}, 0xC040FFFF}, // -Y purple
    };

    return mesh;
}

// primitive helpers

static Face quad(const std::vector<Vec3>& P, int a, int b, int c, int d, u32 base)
{
    (void)P;
    Face f;
    f.indices[0] = a;
    f.indices[1] = b;
    f.indices[2] = c;
    f.indices[3] = d;
    f.color = base;
    return f;
}

// triangle as a degenerate quad (last index repeated)
static Face tri(const std::vector<Vec3>& P, int a, int b, int c, u32 base)
{
    return quad(P, a, b, c, c, base);
}

Mesh makePlane(float size)
{
    Mesh m;
    const float s = size * 0.5f;
    const u32 base = 0xB0B0B8FF;
    m.positions = {{-s, 0, -s}, {s, 0, -s}, {s, 0, s}, {-s, 0, s}};
    m.faces.push_back(quad(m.positions, 0, 3, 2, 1, base)); // +Y up
    m.faces.push_back(quad(m.positions, 0, 1, 2, 3, base)); // -Y down (double sided)
    return m;
}

Mesh makePyramid(float size)
{
    Mesh m;
    const float s = size * 0.5f;
    const u32 base = 0xF0A040FF;
    m.positions = {{-s, -s, -s}, {s, -s, -s}, {s, -s, s}, {-s, -s, s}, {0, s, 0}};
    m.faces.push_back(quad(m.positions, 0, 1, 2, 3, base)); // base, -Y
    m.faces.push_back(tri(m.positions, 1, 0, 4, base));
    m.faces.push_back(tri(m.positions, 2, 1, 4, base));
    m.faces.push_back(tri(m.positions, 3, 2, 4, base));
    m.faces.push_back(tri(m.positions, 0, 3, 4, base));
    return m;
}

Mesh makeCylinder(float radius, float height)
{
    Mesh m;
    const int N = 8;
    const u32 base = 0x40C0A0FF;
    const float PI = 3.14159265f;
    const float hy = height * 0.5f;

    for (int i = 0; i < N; i++) // top ring: 0..N-1
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), hy, radius * sinf(th)});
    }
    for (int i = 0; i < N; i++) // bottom ring: N..2N-1
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), -hy, radius * sinf(th)});
    }
    const int tc = (int)m.positions.size();
    m.positions.push_back({0, hy, 0});
    const int bc = (int)m.positions.size();
    m.positions.push_back({0, -hy, 0});

    auto T = [&](int i) { return i % N; };
    auto B = [&](int i) { return N + i % N; };
    for (int i = 0; i < N; i++)
        m.faces.push_back(quad(m.positions, T(i), T(i + 1), B(i + 1), B(i), base));
    for (int i = 0; i < N; i++)
        m.faces.push_back(tri(m.positions, tc, T(i + 1), T(i), base));
    for (int i = 0; i < N; i++)
        m.faces.push_back(tri(m.positions, bc, B(i), B(i + 1), base));
    return m;
}

Mesh makeSphere(float radius)
{
    Mesh m;
    const int stacks = 5; // latitude segments
    const int slices = 8; // longitude segments
    const u32 base = 0x6098FFFF;
    const float PI = 3.14159265f;

    m.positions.push_back({0, radius, 0}); // top pole = index 0
    for (int j = 1; j < stacks; j++)       // rings j=1..stacks-1
    {
        const float phi = PI * (float)j / (float)stacks;
        const float y = radius * cosf(phi);
        const float rr = radius * sinf(phi);
        for (int i = 0; i < slices; i++)
        {
            const float th = 2 * PI * (float)i / (float)slices;
            m.positions.push_back({rr * cosf(th), y, rr * sinf(th)});
        }
    }
    m.positions.push_back({0, -radius, 0});
    const int bottom = (int)m.positions.size() - 1;

    auto R = [&](int j, int i) { return 1 + (j - 1) * slices + (i % slices); };
    for (int i = 0; i < slices; i++) // top cap
        m.faces.push_back(tri(m.positions, 0, R(1, i + 1), R(1, i), base));
    for (int j = 1; j <= stacks - 2; j++) // bands
        for (int i = 0; i < slices; i++)
            m.faces.push_back(quad(m.positions, R(j, i), R(j, i + 1), R(j + 1, i + 1), R(j + 1, i), base));
    for (int i = 0; i < slices; i++) // bottom cap
        m.faces.push_back(tri(m.positions, bottom, R(stacks - 1, i), R(stacks - 1, i + 1), base));
    return m;
}

Mesh makeCone(float radius, float height)
{
    Mesh m;
    const int N = 8;
    const u32 base = 0xE08040FF;
    const float PI = 3.14159265f;
    const float hy = height * 0.5f;

    for (int i = 0; i < N; i++) // bottom ring: 0..N-1
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), -hy, radius * sinf(th)});
    }
    const int apex = (int)m.positions.size();
    m.positions.push_back({0, hy, 0});
    const int bc = (int)m.positions.size();
    m.positions.push_back({0, -hy, 0});

    auto B = [&](int i) { return i % N; };
    for (int i = 0; i < N; i++) // slope
        m.faces.push_back(tri(m.positions, apex, B(i + 1), B(i), base));
    for (int i = 0; i < N; i++) // bottom cap
        m.faces.push_back(tri(m.positions, bc, B(i), B(i + 1), base));
    return m;
}

Mesh makeTorus(float radius, float tube)
{
    Mesh m;
    const int S = 8; // segments around the main ring
    const int T = 6; // segments around the tube
    const u32 base = 0xB060E0FF;
    const float PI = 3.14159265f;

    for (int i = 0; i < S; i++)
    {
        const float a = 2 * PI * i / S;
        const float ca = cosf(a), sa = sinf(a);
        for (int j = 0; j < T; j++)
        {
            const float b = 2 * PI * j / T;
            const float rr = radius + tube * cosf(b);
            m.positions.push_back({rr * ca, tube * sinf(b), rr * sa});
        }
    }
    auto V = [&](int i, int j) { return (i % S) * T + (j % T); };
    for (int i = 0; i < S; i++)
        for (int j = 0; j < T; j++)
            m.faces.push_back(
                quad(m.positions, V(i, j), V(i, j + 1), V(i + 1, j + 1), V(i + 1, j), base));
    return m;
}

Mesh makeWedge(float size)
{
    Mesh m;
    const float s = size * 0.5f;
    const u32 base = 0xE0C040FF;
    // ramp: high edge along +X (y=+s), sloping down to the -X bottom edge
    m.positions = {
        {-s, -s, -s}, {s, -s, -s}, {s, -s, s}, {-s, -s, s}, // 0..3 bottom
        {s, s, -s},   {s, s, s},                            // 4,5 top edge at +X
    };
    m.faces.push_back(quad(m.positions, 0, 1, 2, 3, base)); // bottom -Y
    m.faces.push_back(quad(m.positions, 1, 4, 5, 2, base)); // back +X
    m.faces.push_back(quad(m.positions, 4, 0, 3, 5, base)); // slope
    m.faces.push_back(tri(m.positions, 1, 0, 4, base));     // -Z side
    m.faces.push_back(tri(m.positions, 3, 2, 5, base));     // +Z side
    return m;
}

Mesh makePrism(float radius, float height)
{
    Mesh m;
    const int N = 3;
    const u32 base = 0x50C878FF;
    const float PI = 3.14159265f;
    const float hy = height * 0.5f;

    for (int i = 0; i < N; i++) // top ring: 0..2
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), hy, radius * sinf(th)});
    }
    for (int i = 0; i < N; i++) // bottom ring: 3..5
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), -hy, radius * sinf(th)});
    }
    auto T = [&](int i) { return i % N; };
    auto B = [&](int i) { return N + i % N; };
    for (int i = 0; i < N; i++)
        m.faces.push_back(quad(m.positions, T(i), T(i + 1), B(i + 1), B(i), base));
    m.faces.push_back(tri(m.positions, T(0), T(2), T(1), base)); // top +Y
    m.faces.push_back(tri(m.positions, B(0), B(1), B(2), base)); // bottom -Y
    return m;
}

Mesh makeCircle(float radius)
{
    Mesh m;
    const int N = 12;
    const u32 base = 0xB0B0B8FF;
    const float PI = 3.14159265f;

    m.positions.push_back({0, 0, 0}); // center = 0
    for (int i = 0; i < N; i++)
    {
        const float th = 2 * PI * i / N;
        m.positions.push_back({radius * cosf(th), 0, radius * sinf(th)});
    }
    auto R = [&](int i) { return 1 + i % N; };
    for (int i = 0; i < N; i++) // +Y up
        m.faces.push_back(tri(m.positions, 0, R(i + 1), R(i), base));
    for (int i = 0; i < N; i++) // -Y down (double sided)
        m.faces.push_back(tri(m.positions, 0, R(i), R(i + 1), base));
    return m;
}
