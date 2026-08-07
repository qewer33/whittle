#pragma once

#include <3ds.h>
#include <citro3d.h>
#include "camera.h"
#include "mesh.h"

struct Line
{
    Vec3 a, b;
};

// Owns the GPU pipeline. The PICA200 has no line primitive, so lines are
// CPU-projected and drawn as screen-space quads for pixel-exact width.
struct Renderer
{
    bool init();
    void exit();

    void beginFrame();
    void drawOn(C3D_RenderTarget* target);
    void endFrame();
    C3D_RenderTarget* topTarget() const { return topTarget_; }
    C3D_RenderTarget* bottomTarget() const { return bottomTarget_; }

    // top-screen preview of every object, plus optional wireframe overlay.
    // shading applies soft flat lighting to the filled faces.
    void drawPreview(const std::vector<Mesh>& objects, const Camera& camera,
                     bool wireframe, bool shading);

    // citro2d clobbers the C3D state, so re-bind ours before drawing
    void bind3DState();

    // line pass. mvp maps world to ndc, bufW/bufH are the target's framebuffer
    // dims (240x400 top, 240x320 bottom), used to scale line width into ndc
    void drawLineSet(const Line* lines, int count, const C3D_Mtx& mvp,
                     u32 color, int bufW, int bufH);

    // clip to a bottom-screen display rect (320x240, y-down). call
    // clearScissor() before any full-screen draw (the citro2d pass, next frame)
    void setBottomScissor(int x, int y, int w, int h);
    void clearScissor();

    // opaque filled faces for a viewport. cull drops the away-facing side so
    // the outer surface shows without depth sorting, caller picks the winding
    void drawFaces(const Mesh& mesh, const C3D_Mtx& mvp, GPU_CULLMODE cull);

    static constexpr int kTexSize = 128; // must match Scene::kTexSize
    // replace the whole texture from a linear RGBA (0xRRGGBBAA) buffer
    void uploadTexture(const u32* rgba);
    C3D_Tex* texture() { return &tex_; }

private:
    static constexpr int kBottomBufW = 240;
    static constexpr int kBottomBufH = 320;
    static constexpr u32 kInitVerts = 16384; // starting vbo size, grows to fit

    C3D_RenderTarget* topTarget_ = nullptr;
    C3D_RenderTarget* bottomTarget_ = nullptr;
    DVLB_s* dvlb = nullptr;
    shaderProgram_s program;
    int uLocProj = -1;

    C3D_Mtx ndc; // identity ortho for the line pass
    C3D_Tex tex_; // shared model texture
    void* solidVbo = nullptr;
    void* wireVbo = nullptr;
    u32 solidCap_ = 0, wireCap_ = 0; // current vbo capacities in verts
    // append cursors into the shared vbos: passes queue until endFrame(), so each
    // appends to fresh space. reset per frame. solid=preview+ortho, wire=lines.
    u32 solidVertOffset_ = 0;
    u32 wireVertOffset_ = 0;
    // verts requested this frame (counts past capacity), drives the grow at the
    // next frame boundary
    u32 solidNeeded_ = 0;
    u32 wireNeeded_ = 0;

    void drawSolid(const Mesh& mesh, bool shade);
    void drawFaceSubset(const Mesh& mesh, bool textured, GPU_CULLMODE cull,
                        bool depthTest, bool shade);
    void drawWire(const Mesh& mesh, const C3D_Mtx& vp);
    void drawGizmo(const Camera& camera); // XYZ orientation axes, bottom-right
    void useColorEnv();
    void useTextureEnv();
    void useTextureEnvModulate(); // texture * vertex color, for shaded textures
};
