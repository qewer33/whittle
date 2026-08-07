#include "meshexport.h"
#include "scene.h"
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#include <vector>

namespace
{
    // display name to a safe filename stem (matches Scene::slug)
    std::string slug(const std::string& name)
    {
        std::string s;
        for (char c : name)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_')
                s += c;
            else if (c == ' ')
                s += '-';
        }
        if (s.empty())
            s = "untitled";
        return s;
    }

    // uncompressed 32-bit TGA written bottom-up with a bottom-left origin.
    bool writeTga(const std::string& path, const std::vector<u32>& tex, int size)
    {
        FILE* f = fopen(path.c_str(), "wb");
        if (!f)
            return false;
        unsigned char hdr[18] = {0};
        hdr[2] = 2;                              // uncompressed true-color
        hdr[12] = size & 0xFF; hdr[13] = size >> 8; // width
        hdr[14] = size & 0xFF; hdr[15] = size >> 8; // height
        hdr[16] = 32;                            // bits per pixel
        hdr[17] = 0x08;                          // 8 alpha bits, bottom-left origin
        fwrite(hdr, 1, 18, f);
        for (int y = size - 1; y >= 0; y--)
            for (int x = 0; x < size; x++)
            {
                const u32 c = tex[y * size + x]; // 0xRRGGBBAA
                const unsigned char px[4] = {
                    (unsigned char)((c >> 8) & 0xFF),  // B
                    (unsigned char)((c >> 16) & 0xFF), // G
                    (unsigned char)((c >> 24) & 0xFF), // R
                    (unsigned char)(c & 0xFF),         // A
                };
                fwrite(px, 1, 4, f);
            }
        fclose(f);
        return true;
    }
}

bool meshexport::exportObj(const Scene& scene, const std::string& name)
{
    const std::string stem = slug(name);
    mkdir("sdmc:/whittle", 0777);
    mkdir("sdmc:/whittle/exports", 0777);
    const std::string dir = std::string("sdmc:/whittle/exports/") + stem;
    mkdir(dir.c_str(), 0777);
    const std::string objPath = dir + "/" + stem + ".obj";
    const std::string mtlPath = dir + "/" + stem + ".mtl";
    const std::string tgaName = stem + ".tga";

    // one material per unique flat color, plus one shared textured material
    std::vector<u32> colors;
    bool hasTex = false;
    for (const Mesh& m : scene.objects)
        for (const Face& fa : m.faces)
        {
            if (fa.textured)
            {
                hasTex = true;
                continue;
            }
            bool found = false;
            for (u32 c : colors)
                if (c == fa.color) { found = true; break; }
            if (!found)
                colors.push_back(fa.color);
        }

    // .mtl
    FILE* mf = fopen(mtlPath.c_str(), "w");
    if (!mf)
        return false;
    for (u32 c : colors)
        fprintf(mf, "newmtl mat_%06x\nKd %.4f %.4f %.4f\n\n", (unsigned)((c >> 8) & 0xFFFFFF),
                ((c >> 24) & 0xFF) / 255.0f, ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f);
    if (hasTex)
        fprintf(mf, "newmtl mat_textured\nKd 1 1 1\nmap_Kd %s\n", tgaName.c_str());
    fclose(mf);

    if (hasTex && !writeTga(dir + "/" + tgaName, scene.texture, Scene::kTexSize))
        return false;

    // .obj
    FILE* of = fopen(objPath.c_str(), "w");
    if (!of)
        return false;
    fprintf(of, "# exported from whittle\nmtllib %s.mtl\n", stem.c_str());

    int vBase = 0, vtCount = 0; // running global (1-based) vertex / texcoord counts
    for (size_t oi = 0; oi < scene.objects.size(); oi++)
    {
        const Mesh& m = scene.objects[oi];
        fprintf(of, "o Object_%u\n", (unsigned)oi);
        for (const Vec3& p : m.positions)
            fprintf(of, "v %.6f %.6f %.6f\n", p.x, p.y, p.z);

        std::string curMat;
        for (const Face& fa : m.faces)
        {
            char mat[24];
            if (fa.textured)
                snprintf(mat, sizeof(mat), "mat_textured");
            else
                snprintf(mat, sizeof(mat), "mat_%06x", (unsigned)((fa.color >> 8) & 0xFFFFFF));
            if (curMat != mat)
            {
                fprintf(of, "usemtl %s\n", mat);
                curMat = mat;
            }

            const int n = fa.indices[3] == fa.indices[2] ? 3 : 4; // degenerate quad = tri
            if (fa.textured)
            {
                for (int c = 0; c < n; c++)
                    fprintf(of, "vt %.6f %.6f\n", fa.uv[c][0], fa.uv[c][1]);
                fprintf(of, "f");
                for (int c = 0; c < n; c++)
                    fprintf(of, " %d/%d", vBase + fa.indices[c] + 1, vtCount + c + 1);
                fprintf(of, "\n");
                vtCount += n;
            }
            else
            {
                fprintf(of, "f");
                for (int c = 0; c < n; c++)
                    fprintf(of, " %d", vBase + fa.indices[c] + 1);
                fprintf(of, "\n");
            }
        }
        vBase += (int)m.positions.size();
    }
    fclose(of);
    return true;
}

bool meshexport::exportStl(const Scene& scene, const std::string& name)
{
    const std::string stem = slug(name);
    mkdir("sdmc:/whittle", 0777);
    mkdir("sdmc:/whittle/exports", 0777);
    const std::string dir = std::string("sdmc:/whittle/exports/") + stem;
    mkdir(dir.c_str(), 0777);
    const std::string path = dir + "/" + stem + ".stl";

    u32 triCount = 0;
    for (const Mesh& m : scene.objects)
        for (const Face& fa : m.faces)
            triCount += fa.indices[3] == fa.indices[2] ? 1 : 2;

    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
    char header[80] = {0};
    snprintf(header, sizeof(header), "whittle STL export");
    fwrite(header, 1, 80, f);
    fwrite(&triCount, sizeof(u32), 1, f);

    // STL convention is Z-up but our world is Y-up, so rotate
    auto zup = [](const Vec3& p) { return Vec3{p.x, -p.z, p.y}; };

    // binary STL triangle: normal(3f) + 3 verts(9f) + attr(u16). LE matches ARM.
    auto writeTri = [&](const Vec3& A, const Vec3& B, const Vec3& C) {
        const Vec3 a = zup(A), b = zup(B), c = zup(C);
        const Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
        const Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 nrm = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        const float len = sqrtf(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
        if (len > 1e-8f) { nrm.x /= len; nrm.y /= len; nrm.z /= len; }
        else { nrm = {0.0f, 0.0f, 0.0f}; }
        const float buf[12] = {nrm.x, nrm.y, nrm.z, a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z};
        fwrite(buf, sizeof(float), 12, f);
        const u16 attr = 0;
        fwrite(&attr, sizeof(u16), 1, f);
    };

    for (const Mesh& m : scene.objects)
        for (const Face& fa : m.faces)
        {
            const Vec3& p0 = m.positions[fa.indices[0]];
            const Vec3& p1 = m.positions[fa.indices[1]];
            const Vec3& p2 = m.positions[fa.indices[2]];
            writeTri(p0, p1, p2);
            if (fa.indices[3] != fa.indices[2])
                writeTri(p0, p2, m.positions[fa.indices[3]]);
        }
    fclose(f);
    return true;
}
