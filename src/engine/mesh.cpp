#include "mesh.h"
#include <math.h>

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

// quad (a,b,c,d), flat single color. the renderer's runtime shading gives the
// preview its 3D look; the ortho views stay flat.
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
    m.faces.push_back(quad(m.positions, 0, 1, 2, 3, base)); // -Y down (double-sided)
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
