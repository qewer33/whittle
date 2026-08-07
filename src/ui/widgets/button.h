#pragma once

#include <citro2d.h>
#include "rect.h"
#include "icons.h"

namespace widgets
{
    // a toolbar button: its hit rect (from Rect) plus its own drawing. active
    // fills the background, disabled dims the icon.
    struct Button : Rect
    {
        void draw(Icon ic, bool active = false, bool enabled = true) const;
        void drawLabeled(Icon ic, const C2D_Text* label, float labelH, bool active = false) const;
    };
}
