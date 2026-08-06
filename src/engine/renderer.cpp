#include "renderer.h"
#include "main_shbin.h"
#include <math.h>

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define CLEAR_COLOR 0x20242EFF
#define WIRE_COLOR 0xD9D9E0FF

// position(3) + color(4) + uv(2)
static constexpr u32 kVertexFloats = 9;
static constexpr u32 kVertexSize = kVertexFloats * 4;

static void bindVertexBuffer(void* vbo)
{
    C3D_BufInfo* bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    // perm 0x210: slot0->v0, slot1->v1, slot2->v2; must match AttrInfo
    BufInfo_Add(bufInfo, vbo, kVertexSize, 3, 0x210);
}

static void colorToFloats(u32 color, float* out)
{
    out[0] = ((color >> 24) & 0xFF) / 255.0f;
    out[1] = ((color >> 16) & 0xFF) / 255.0f;
    out[2] = ((color >> 8) & 0xFF) / 255.0f;
    out[3] = (color & 0xFF) / 255.0f;
}

// 0xRRGGBBAA -> PICA texture texel order (ABGR bytes, unlike the framebuffer)
static u32 toTexel(u32 rgba)
{
    const u32 r = (rgba >> 24) & 0xFF, g = (rgba >> 16) & 0xFF;
    const u32 b = (rgba >> 8) & 0xFF, a = rgba & 0xFF;
    return a | (b << 8) | (g << 16) | (r << 24);
}

// PICA textures are stored in 8x8 tiles, morton (z-order) within each tile
static int tiledIndex(int x, int y, int w)
{
    const int tx = x >> 3, ty = y >> 3;
    const int ux = x & 7, uy = y & 7;
    const int m = (ux & 1) | ((uy & 1) << 1) | ((ux & 2) << 1) |
                  ((uy & 2) << 2) | ((ux & 4) << 2) | ((uy & 4) << 3);
    return (ty * (w >> 3) + tx) * 64 + m;
}

bool Renderer::init()
{
    topTarget_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (!topTarget_)
        return false;
    C3D_RenderTargetSetOutput(topTarget_, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    bottomTarget_ = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (!bottomTarget_)
        return false;
    C3D_RenderTargetSetOutput(bottomTarget_, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    dvlb = DVLB_ParseFile((u32*)main_shbin, main_shbin_size);
    if (!dvlb)
        return false;

    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
    C3D_BindProgram(&program);
    uLocProj = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    // identity ortho, not tilted: the line pass feeds already-NDC coords so it
    // must not re-apply the screen rotation
    Mtx_Ortho(&ndc, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, true);

    solidVbo = linearAlloc(kMaxSolidVerts * kVertexSize);
    wireVbo = linearAlloc(kMaxWireVerts * kVertexSize);
    if (!solidVbo || !wireVbo)
        return false;

    if (!C3D_TexInit(&tex_, kTexSize, kTexSize, GPU_RGBA8))
        return false;
    C3D_TexSetFilter(&tex_, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&tex_, GPU_REPEAT, GPU_REPEAT);
    // content comes from Scene::texture, uploaded by main

    return true;
}

void Renderer::uploadTexture(const u32* rgba)
{
    u32* dst = (u32*)tex_.data;
    for (int y = 0; y < kTexSize; y++)
        for (int x = 0; x < kTexSize; x++)
            dst[tiledIndex(x, y, kTexSize)] = toTexel(rgba[y * kTexSize + x]);
    C3D_TexFlush(&tex_);
}

void Renderer::exit()
{
    C3D_TexDelete(&tex_);
    if (solidVbo)
        linearFree(solidVbo);
    if (wireVbo)
        linearFree(wireVbo);
    if (dvlb)
    {
        shaderProgramFree(&program);
        DVLB_Free(dvlb);
    }
    if (topTarget_)
        C3D_RenderTargetDelete(topTarget_);
    if (bottomTarget_)
        C3D_RenderTargetDelete(bottomTarget_);
}

void Renderer::beginFrame()
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    // draws are queued until endFrame(), so rewind the shared vbos each frame
    solidVertOffset_ = 0;
    wireVertOffset_ = 0;
}

void Renderer::drawOn(C3D_RenderTarget* target)
{
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(target);
}

void Renderer::endFrame()
{
    C3D_FrameEnd(0);
}

void Renderer::bind3DState()
{
    C3D_BindProgram(&program);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4); // v1 = color
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2); // v2 = uv

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void Renderer::drawPreview(const std::vector<Mesh>& objects, const Camera& camera,
                           bool wireframe, bool shading)
{
    bind3DState();

    const C3D_Mtx vp = camera.viewProj();

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &vp);
    for (const Mesh& m : objects)
        drawSolid(m, shading);

    if (wireframe)
    {
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &ndc);
        for (const Mesh& m : objects)
            drawWire(m, vp);
    }

    drawGizmo(camera);
}

void Renderer::useColorEnv()
{
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void Renderer::useTextureEnv()
{
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
}

void Renderer::useTextureEnvModulate()
{
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

// soft flat shade for a face from its first three verts: a hemisphere term (up
// brighter) plus a gentle directional, kept well away from black
static float faceShade(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
    const Vec3 e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    const Vec3 e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
    Vec3 n = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
    const float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len < 1e-6f)
        return 1.0f;
    n = {n.x / len, n.y / len, n.z / len};
    // asymmetric direction (distinct x/z) so no two face orientations tie
    static const float lx = 0.72f, ly = 0.62f, lz = 0.31f;
    // full-range dot (not clamped at 0) so front/side/back faces all differ
    const float dir = (n.x * lx + n.y * ly + n.z * lz) * 0.5f + 0.5f; // 0..1
    const float hemi = n.y * 0.5f + 0.5f;                             // 0 down .. 1 up
    float s = 0.60f + 0.28f * dir + 0.12f * hemi;
    return s > 1.0f ? 1.0f : s;
}

// draws the flat or the textured faces of a mesh; caller sets the projection
// uniform. shared by the preview and the ortho views. shade bakes flat lighting
// into the vertex color.
void Renderer::drawFaceSubset(const Mesh& mesh, bool textured, GPU_CULLMODE cull,
                              bool depthTest, bool shade)
{
    const u32 base = solidVertOffset_;
    float* dst = (float*)solidVbo + base * kVertexFloats;
    u32 vertCount = 0;
    const int corner[6] = {0, 1, 2, 0, 2, 3};

    for (const Face& face : mesh.faces)
    {
        if (face.textured != textured)
            continue;
        if (base + vertCount + 6 > kMaxSolidVerts)
            break;

        float col[4];
        colorToFloats(face.color, col);

        const float s = shade ? faceShade(mesh.positions[face.indices[0]],
                                          mesh.positions[face.indices[1]],
                                          mesh.positions[face.indices[2]])
                              : 1.0f;
        // shaded textures modulate texture*color, so put the shade in the color;
        // otherwise bake it into the flat color directly
        float vc[4];
        if (textured && shade)
        {
            vc[0] = vc[1] = vc[2] = s;
            vc[3] = 1.0f;
        }
        else
        {
            vc[0] = col[0] * s;
            vc[1] = col[1] * s;
            vc[2] = col[2] * s;
            vc[3] = col[3];
        }

        for (int i = 0; i < 6; i++)
        {
            const int c = corner[i];
            const Vec3& p = mesh.positions[face.indices[c]];
            dst[0] = p.x;
            dst[1] = p.y;
            dst[2] = p.z;
            dst[3] = vc[0];
            dst[4] = vc[1];
            dst[5] = vc[2];
            dst[6] = vc[3];
            dst[7] = face.uv[c][0];
            dst[8] = face.uv[c][1];
            dst += kVertexFloats;
            vertCount++;
        }
    }

    if (vertCount == 0)
        return;

    bindVertexBuffer(solidVbo);
    if (textured)
    {
        if (shade)
            useTextureEnvModulate();
        else
            useTextureEnv();
        C3D_TexBind(0, &tex_);
    }
    else
        useColorEnv();
    C3D_CullFace(cull);
    if (depthTest)
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    else
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_DrawArrays(GPU_TRIANGLES, base, vertCount);
    solidVertOffset_ += vertCount;
}

void Renderer::drawSolid(const Mesh& mesh, bool shade)
{
    // double-sided: the depth buffer sorts overlap, so open shells (a deleted
    // face) show their inner walls in-color instead of culling to background.
    // closed solids look identical since the near face wins the depth test.
    drawFaceSubset(mesh, false, GPU_CULL_NONE, true, shade);
    drawFaceSubset(mesh, true, GPU_CULL_NONE, true, shade);
}

void Renderer::drawFaces(const Mesh& mesh, const C3D_Mtx& mvp, GPU_CULLMODE cull)
{
    // ortho matrix now carries real depth (see Viewport::matrix), so the depth
    // buffer sorts faces like the preview. callers pass GPU_CULL_NONE to render
    // both sides (open shells show their far walls through the opening).
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &mvp);
    drawFaceSubset(mesh, false, cull, true, false); // ortho stays flat full-color
    drawFaceSubset(mesh, true, cull, true, false);
}

void Renderer::drawLineSet(const Line* lines, int count, const C3D_Mtx& mvp,
                           u32 color, int bufW, int bufH)
{
    if (count <= 0)
        return;

    const float col[4] = {
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f,
    };

    const float halfW = 1.0f; // 2px total width
    // clamp to room left in the shared vbo this frame
    const u32 roomQuads = (kMaxWireVerts - wireVertOffset_) / 6;
    if (count > (int)roomQuads)
        count = (int)roomQuads;
    if (count <= 0)
        return;

    float* dst = (float*)wireVbo + wireVertOffset_ * kVertexFloats;
    u32 vertCount = 0;

    for (int li = 0; li < count; li++)
    {
        const Vec3& pa = lines[li].a;
        const Vec3& pb = lines[li].b;

        // project both endpoints to ndc
        float ax, ay, bx, by;
        bool aok, bok;
        {
            const C3D_FVec& r0 = mvp.r[0];
            const C3D_FVec& r1 = mvp.r[1];
            const C3D_FVec& r3 = mvp.r[3];
            float w = r3.x * pa.x + r3.y * pa.y + r3.z * pa.z + r3.w;
            if (w < 0.01f)
                aok = false;
            else
            {
                ax = (r0.x * pa.x + r0.y * pa.y + r0.z * pa.z + r0.w) / w;
                ay = (r1.x * pa.x + r1.y * pa.y + r1.z * pa.z + r1.w) / w;
                aok = true;
            }
            w = r3.x * pb.x + r3.y * pb.y + r3.z * pb.z + r3.w;
            if (w < 0.01f)
                bok = false;
            else
            {
                bx = (r0.x * pb.x + r0.y * pb.y + r0.z * pb.z + r0.w) / w;
                by = (r1.x * pb.x + r1.y * pb.y + r1.z * pb.z + r1.w) / w;
                bok = true;
            }
        }
        if (!aok || !bok)
            continue;

        const float dx = bx - ax;
        const float dy = by - ay;
        const float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f)
            continue;

        // screen-space normal, in ndc units
        const float nx = (-dy / len) * halfW * 2.0f / bufW;
        const float ny = (dx / len) * halfW * 2.0f / bufH;

        const float quad[4][2] = {
            {ax - nx, ay - ny},
            {bx - nx, by - ny},
            {bx + nx, by + ny},
            {ax + nx, ay + ny},
        };
        const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; i++)
        {
            dst[0] = quad[tri[i]][0];
            dst[1] = quad[tri[i]][1];
            dst[2] = 0.5f;
            dst[3] = col[0];
            dst[4] = col[1];
            dst[5] = col[2];
            dst[6] = col[3];
            dst[7] = 0.0f;
            dst[8] = 0.0f;
            dst += kVertexFloats;
            vertCount++;
        }
    }

    if (vertCount == 0)
        return;

    bindVertexBuffer(wireVbo);
    useColorEnv();
    // coords are already ndc, so draw through the identity ortho
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &ndc);
    C3D_CullFace(GPU_CULL_NONE);
    // color only, never depth: lines are pure overlays and must not disturb the
    // depth buffer the ortho faces now sort against
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_DrawArrays(GPU_TRIANGLES, wireVertOffset_, vertCount);
    wireVertOffset_ += vertCount;
}

void Renderer::setBottomScissor(int x, int y, int w, int h)
{
    // display (320x240, y-down) to framebuffer (240x320, rotated 90 CW):
    // fb left/right = 240 - display_y, fb top/bottom = 320 - display_x
    const int left = kBottomBufW - (y + h);
    const int right = kBottomBufW - y;
    const int top = kBottomBufH - (x + w);
    const int bottom = kBottomBufH - x;
    C3D_SetScissor(GPU_SCISSOR_NORMAL, left, top, right, bottom);
}

void Renderer::clearScissor()
{
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

void Renderer::drawWire(const Mesh& mesh, const C3D_Mtx& vp)
{
    if (mesh.faces.size() * 4 > 256)
        return;
    Line edges[256];
    int n = 0;
    for (const Face& face : mesh.faces)
    {
        for (int e = 0; e < 4; e++)
        {
            edges[n].a = mesh.positions[face.indices[e]];
            edges[n].b = mesh.positions[face.indices[(e + 1) % 4]];
            n++;
        }
    }
    drawLineSet(edges, n, vp, WIRE_COLOR, 240, 400);
}

static Vec3 gsub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 gcross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static Vec3 gnorm(Vec3 v)
{
    const float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return l < 1e-6f ? Vec3{0, 0, 0} : Vec3{v.x / l, v.y / l, v.z / l};
}

// letter strokes in a [-0.5,0.5] box: {x0,y0, x1,y1}, drawn billboarded
static const float kLetX[2][4] = {{-0.4f, -0.5f, 0.4f, 0.5f}, {-0.4f, 0.5f, 0.4f, -0.5f}};
static const float kLetY[3][4] = {
    {-0.4f, 0.5f, 0.0f, 0.05f}, {0.4f, 0.5f, 0.0f, 0.05f}, {0.0f, 0.05f, 0.0f, -0.5f}};
static const float kLetZ[3][4] = {
    {-0.4f, 0.5f, 0.4f, 0.5f}, {0.4f, 0.5f, -0.4f, -0.5f}, {-0.4f, -0.5f, 0.4f, -0.5f}};

// A small XYZ axis indicator pinned to the bottom-right of the preview. It's
// placed in view space (fixed distance + offset from the camera) so it stays a
// constant screen size and position, and drawn through the normal projection so
// the 3DS screen tilt is handled for free. Lines are depth-off, so it's on top.
void Renderer::drawGizmo(const Camera& camera)
{
    const Vec3 e = camera.eye();
    const Vec3 t = camera.target;
    const Vec3 fwd = gnorm(gsub(t, e));
    const Vec3 right = gnorm(gcross(fwd, {0.0f, 1.0f, 0.0f}));
    const Vec3 up = gcross(right, fwd);

    // draw the gizmo in orthographic
    const float halfH = 1.0f;
    const float halfW = halfH * (400.0f / 240.0f);
    const float len = halfH * 0.09f;    // axis length (ortho units)
    const float margin = len * 1.9f;    // room for the axis + its letter
    const float ox = halfW - margin;    // toward the right edge
    const float oy = -(halfH - margin); // toward the bottom edge

    C3D_Mtx view, proj, vp;
    Mtx_LookAt(&view, FVec4_New(e.x, e.y, e.z, 1.0f), FVec4_New(t.x, t.y, t.z, 1.0f),
               FVec4_New(0.0f, 1.0f, 0.0f, 1.0f), true);
    Mtx_OrthoTilt(&proj, -halfW, halfW, -halfH, halfH, 0.1f, 100.0f, true);
    Mtx_Multiply(&vp, &proj, &view);

    // anchor a fixed offset from the view center (target); in ortho this lands
    // at a stable screen corner with no positional skew
    const Vec3 o = {
        t.x + right.x * ox + up.x * oy,
        t.y + right.y * ox + up.y * oy,
        t.z + right.z * ox + up.z * oy,
    };
    static const u32 col[3] = {0xF04747FF, 0x4FD16BFF, 0x4A8CF0FF}; // X red, Y green, Z blue
    static const Vec3 dir[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float ls = len * 0.5f; // letter size

    for (int a = 0; a < 3; a++)
    {
        const Vec3 tip = {o.x + dir[a].x * len, o.y + dir[a].y * len, o.z + dir[a].z * len};
        // letter center just past the tip, billboarded in the camera plane
        const Vec3 lc = {tip.x + dir[a].x * len * 0.4f, tip.y + dir[a].y * len * 0.4f,
                         tip.z + dir[a].z * len * 0.4f};
        const float(*st)[4] = a == 0 ? kLetX : a == 1 ? kLetY : kLetZ;
        const int ns = a == 0 ? 2 : 3;

        Line ls_[4];
        int n = 0;
        ls_[n++] = {o, tip}; // the axis
        for (int s = 0; s < ns; s++)
        {
            // negate x: the screen billboard is mirrored horizontally
            const float x0 = -st[s][0], y0 = st[s][1], x1 = -st[s][2], y1 = st[s][3];
            ls_[n].a = {lc.x + right.x * x0 * ls + up.x * y0 * ls,
                        lc.y + right.y * x0 * ls + up.y * y0 * ls,
                        lc.z + right.z * x0 * ls + up.z * y0 * ls};
            ls_[n].b = {lc.x + right.x * x1 * ls + up.x * y1 * ls,
                        lc.y + right.y * x1 * ls + up.y * y1 * ls,
                        lc.z + right.z * x1 * ls + up.z * y1 * ls};
            n++;
        }
        drawLineSet(ls_, n, vp, col[a], 240, 400);
    }
}
