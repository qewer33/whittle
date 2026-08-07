#pragma once

#include <string>

namespace platform
{
    // native software keyboard. returns true + fills `out` on confirm, false on
    // cancel. must be called between frames (it's a system applet).
    bool inputText(const char* hint, const char* initial, std::string& out);
}
