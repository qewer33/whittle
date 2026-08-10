#pragma once

#include <3ds.h>
#include <vector>

struct Vec3
{
    float x, y, z;
};

// component access by axis index (0=x, 1=y, 2=z)
inline float getAxis(const Vec3& v, int axis)
{
    return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
}
inline void setAxis(Vec3& v, int axis, float value)
{
    if (axis == 0)
        v.x = value;
    else if (axis == 1)
        v.y = value;
    else
        v.z = value;
}

struct Face
{
    int indices[4];
    u32 color;            // 0xRRGGBBAA, used when not textured
    bool textured = false;
    float uv[4][2] = {};  // per-vertex uv into the shared texture
};

struct Mesh;

// Derived connectivity for editing operations. This is rebuilt from the face
// indices and is intentionally not serialized with a project.
struct MeshTopology
{
    struct Edge
    {
        int v0, v1;             // sorted vertex pair
        std::vector<int> faces; // faces incident to this edge
    };

    std::vector<Edge> edges;

    void rebuild(const Mesh& mesh);
    int findEdge(int a, int b) const;
};

struct Mesh
{
    std::vector<Vec3> positions;
    std::vector<Face> faces;
    mutable MeshTopology topology;

    // Topology is derived working data, never part of the project file.
    void rebuildTopology() const { topology.rebuild(*this); }

    int addVertex(Vec3 p);
    // drops the vertex and any face using it, returns count removed (0 if bad idx)
    int removeVertex(int idx);
    void moveVertex(int idx, Vec3 p);
};

// primitives centered at origin, faces wound CCW from outside. makeCube is
// per-face colored, the rest use one base color shaded by normal
Mesh makeCube(float size);
Mesh makeSphere(float radius);
Mesh makePyramid(float size);
Mesh makeCylinder(float radius, float height);
Mesh makePlane(float size);
