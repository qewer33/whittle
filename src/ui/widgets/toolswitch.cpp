#include "toolswitch.h"
#include "uidraw.h"
#include "layout.h"

using namespace uidraw;

namespace widgets
{
    void ToolSwitch::draw(int active) const
    {
        const int n = count();
        for (int i = 0; i < n; i++)
        {
            const Rect r = layout::segCell(i, n);
            const bool on = i == active;
            C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(on ? kActiveBg : kItemBg));
            outline(r.x, r.y, r.w, r.h, kBorderCol);
            icons::draw(icons_[i], r.x + (r.w - kIcon) / 2.0f, r.y + (r.h - kIcon) / 2.0f, kIcon,
                        on ? kIconActive : kIconIdle);
        }
    }

    int ToolSwitch::handle(int px, int py) const
    {
        const int n = count();
        for (int i = 0; i < n; i++)
            if (layout::segCell(i, n).contains(px, py))
                return i;
        return -1;
    }
}
