#pragma once

#include <string>

namespace platform
{
    // native software keyboard. returns true and fills `out` on confirm, false
    // on cancel. `initial` prefills the field. must be called between frames
    // (the keyboard is a system applet that takes over both screens).
    bool inputText(const char* hint, const char* initial, std::string& out);
}
