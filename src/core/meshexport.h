#pragma once

#include <string>

struct Scene;

namespace meshexport
{
    // write <name>.obj + <name>.mtl (+ <name>.tga if any face is textured) under
    // sdmc:/whittle/exports/<name>/. returns false on any file error.
    bool exportObj(const Scene& scene, const std::string& name);

    // write <name>.stl (binary, triangulated, geometry only) in the same dir.
    bool exportStl(const Scene& scene, const std::string& name);
}
