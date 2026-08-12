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

    void Button::drawRaised(Icon ic, bool active, bool enabled) const
    {
        raisedButton(x, y, w, h, active, enabled);
        const u32 col = !enabled ? kIconDim : active ? kIconActive : kIconIdle;
        icons::draw(ic, x + (w - kIcon) / 2.0f, y + (h - kIcon) / 2.0f, kIcon, col);
    }

    void Button::drawRaisedLabeled(Icon ic, const C2D_Text* label, float labelH,
                                   bool active) const
    {
        raisedButton(x, y, w, h, active);
        icons::draw(ic, x + 5, y + (h - kIcon) / 2.0f, kIcon,
                    active ? kIconActive : kIconIdle);
        C2D_DrawText(label, C2D_WithColor, x + 5 + kIcon + 4,
                     y + (h - labelH) / 2.0f, 0.0f, kTextScale, kTextScale,
                     conv(active ? kIconActive : kTextCol));
    }

    void Button::drawRaisedTop(Icon ic, bool active, bool enabled) const
    {
        raisedButton(x, y, w, h, active, enabled, true);
        const u32 col = !enabled ? kIconDim : active ? kIconActive : kIconIdle;
        icons::draw(ic, x + (w - kIcon) / 2.0f, y + (h - kIcon) / 2.0f, kIcon, col);
    }

    void Button::drawRaisedTopLabeled(Icon ic, const C2D_Text* label, float labelH,
                                      bool active) const
    {
        raisedButton(x, y, w, h, active, true, true);
        icons::draw(ic, x + 5, y + (h - kIcon) / 2.0f, kIcon,
                    active ? kIconActive : kIconIdle);
        C2D_DrawText(label, C2D_WithColor, x + 5 + kIcon + 4,
                     y + (h - labelH) / 2.0f, 0.0f, kTextScale, kTextScale,
                     conv(active ? kIconActive : kTextCol));
    }
}
