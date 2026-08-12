#pragma once

#include <citro2d.h>
#include "rect.h"
#include "uidraw.h"

namespace widgets
{
    // a full-width toolbar: a rect that fills itself with the bar color
    struct Bar : Rect
    {
        void draw(bool bottomShine = false) const
        {
            const u32 top = bottomShine ? uidraw::kBarBottom : uidraw::kBarTop;
            const u32 bottom = bottomShine ? uidraw::kBarTop : uidraw::kBarBottom;
            C2D_DrawRectangle(x, y, 0.0f, w, h, uidraw::conv(top), uidraw::conv(top),
                              uidraw::conv(bottom), uidraw::conv(bottom));
            C2D_DrawRectSolid(x, y, 0.0f, w, 1,
                              uidraw::conv(bottomShine ? uidraw::kRaisedShadow : uidraw::kBorderCol));
            C2D_DrawRectSolid(x, y + h - 1, 0.0f, w, 1,
                              uidraw::conv(bottomShine ? uidraw::kBorderCol : uidraw::kRaisedShadow));
            C2D_DrawRectSolid(x, y, 0.0f, 1, h, uidraw::conv(uidraw::kRaisedShadow));
            C2D_DrawRectSolid(x + w - 1, y, 0.0f, 1, h, uidraw::conv(uidraw::kRaisedShadow));
        }

        void drawHighlight(bool bottom) const
        {
            const int edgeY = bottom ? y + h - 1 : y;
            C2D_DrawRectSolid(x, edgeY, 0.0f, w, 1, uidraw::conv(uidraw::kBarHighlight));
        }
    };
}
