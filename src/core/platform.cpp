#include "platform.h"
#include <3ds.h>

namespace platform
{
    bool inputText(const char* hint, const char* initial, std::string& out)
    {
        SwkbdState swkbd;
        swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 32);
        swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
        if (hint)
            swkbdSetHintText(&swkbd, hint);
        if (initial && initial[0])
            swkbdSetInitialText(&swkbd, initial);

        char buf[256] = {0}; // 32 chars, UTF-8 worst case
        const SwkbdButton btn = swkbdInputText(&swkbd, buf, sizeof(buf));
        if (btn != SWKBD_BUTTON_RIGHT)
            return false;
        out = buf;
        return true;
    }
}
