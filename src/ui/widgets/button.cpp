#include "button.h"
#include "uidraw.h"

using namespace uidraw;

namespace widgets
{
    void Button::draw(Icon ic, bool active, bool enabled) const
    {
        if (active)
            C2D_DrawRectSolid(x, y, 0.0f, w, h, conv(kActiveBg));
        const u32 col = !enabled ? kIconDim : active ? kIconActive : kIconIdle;
        icons::draw(ic, x + (w - kIcon) / 2.0f, y + (h - kIcon) / 2.0f, kIcon, col);
    }

    void Button::drawLabeled(Icon ic, const C2D_Text* label, float labelH, bool active) const
    {
        if (active)
            C2D_DrawRectSolid(x, y, 0.0f, w, h, conv(kActiveBg));
        icons::draw(ic, x + 5, y + (h - kIcon) / 2.0f, kIcon, active ? kIconActive : kIconIdle);
        textLeft(label, labelH, x + 5 + kIcon + 4, y, h);
    }
}
