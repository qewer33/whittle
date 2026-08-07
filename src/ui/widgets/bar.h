#pragma once

#include <citro2d.h>
#include "rect.h"
#include "uidraw.h"

namespace widgets
{
    // a full-width toolbar: a rect that fills itself with the bar color
    struct Bar : Rect
    {
        void draw() const { C2D_DrawRectSolid(x, y, 0.0f, w, h, uidraw::conv(uidraw::kBarBg)); }
    };
}
