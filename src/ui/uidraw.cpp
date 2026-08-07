#include "uidraw.h"

namespace uidraw
{
    static C2D_TextBuf buf = nullptr;

    void init() { buf = C2D_TextBufNew(1024); }

    void exit()
    {
        if (buf)
            C2D_TextBufDelete(buf);
        buf = nullptr;
    }

    C2D_TextBuf labelBuf() { return buf; }
}
