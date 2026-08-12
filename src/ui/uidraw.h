#pragma once

#include <3ds.h>
#include <citro2d.h>

// shared UI theme + draw primitives, used by ui.cpp and the widgets
namespace uidraw
{
    inline constexpr u32 kBarBg = 0x1B1E26FF;
    inline constexpr u32 kBarTop = 0x242B35FF;
    inline constexpr u32 kBarBottom = 0x11161DFF;
    inline constexpr u32 kBarHighlight = 0x59636FFF;
    inline constexpr u32 kActiveBg = 0x4E7BCCFF;
    inline constexpr u32 kItemBg = 0x333A4AFF;
    inline constexpr u32 kPanelBg = 0x272B36FF;
    inline constexpr u32 kBorderCol = 0x66707EFF;
    inline constexpr u32 kIconIdle = 0xC2C8D4FF;
    inline constexpr u32 kIconActive = 0xFFFFFFFF;
    inline constexpr u32 kIconDim = 0x565D6EFF;
    inline constexpr u32 kTextCol = 0xE8ECF4FF;
    inline constexpr u32 kRaisedTop = 0x303946FF;
    inline constexpr u32 kRaisedBottom = 0x161B23FF;
    inline constexpr u32 kRaisedActiveTop = 0x5F91D9FF;
    inline constexpr u32 kRaisedActiveBottom = 0x2F559FFF;
    inline constexpr u32 kRaisedDisabledTop = 0x292F39FF;
    inline constexpr u32 kRaisedDisabledBottom = 0x171B23FF;
    inline constexpr u32 kRaisedHighlight = 0x6E7885FF;
    inline constexpr u32 kRaisedBottomHighlight = 0x3A444FFF;
    inline constexpr u32 kRaisedShadow = 0x0F141BFF;
    inline constexpr float kIcon = 14.0f;
    inline constexpr float kTextScale = 0.5f;

    // 0xRRGGBBAA to a citro2d color
    inline u32 conv(u32 rgba)
    {
        return C2D_Color32((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF);
    }

    inline void outline(int x, int y, int w, int h, u32 color)
    {
        const u32 c = conv(color);
        C2D_DrawRectSolid(x, y, 0.0f, w, 1, c);
        C2D_DrawRectSolid(x, y + h - 1, 0.0f, w, 1, c);
        C2D_DrawRectSolid(x, y, 0.0f, 1, h, c);
        C2D_DrawRectSolid(x + w - 1, y, 0.0f, 1, h, c);
    }

    // Small clipped corners keep the compact toolbar controls rounded without
    // requiring a separate rounded-rectangle texture.
    inline void raisedButton(int x, int y, int w, int h, bool active, bool enabled = true,
                             bool bottomShine = false)
    {
        const u32 top = !enabled ? kRaisedDisabledTop : active ? kRaisedActiveTop : kRaisedTop;
        const u32 bottom = !enabled ? kRaisedDisabledBottom : active ? kRaisedActiveBottom : kRaisedBottom;
        const u32 bodyTop = bottomShine ? bottom : top;
        const u32 bodyBottom = bottomShine ? top : bottom;
        if (enabled)
            C2D_DrawRectSolid(x + 1, y + 1, 0.0f, w - 1, h - 1, conv(kRaisedShadow));
        C2D_DrawRectangle(x + 1, y, 0.0f, w - 2, h, conv(bodyTop), conv(bodyTop), conv(bodyBottom), conv(bodyBottom));
        C2D_DrawRectangle(x, y + 1, 0.0f, w, h - 2, conv(bodyTop), conv(bodyTop), conv(bodyBottom), conv(bodyBottom));

        if (!enabled)
            return;

        const u32 upper = conv(bottomShine ? kRaisedBottomHighlight : kRaisedHighlight);
        const u32 lower = conv(bottomShine ? kRaisedHighlight : kRaisedBottomHighlight);
        const u32 side = conv(bottomShine ? kRaisedHighlight : kRaisedBottomHighlight);
        C2D_DrawRectSolid(x + 2, y, 0.0f, w - 4, 1, upper);
        C2D_DrawRectSolid(x, y + 2, 0.0f, 1, h - 4, upper);
        C2D_DrawRectSolid(x + 2, y + h - 1, 0.0f, w - 4, 1, lower);
        C2D_DrawRectSolid(x + w - 1, y + 2, 0.0f, 1, h - 4, side);
        C2D_DrawRectSolid(x + 1, y, 0.0f, 1, 1, upper);
        C2D_DrawRectSolid(x + w - 2, y, 0.0f, 1, 1, upper);
        C2D_DrawRectSolid(x, y + 1, 0.0f, 1, 1, upper);
        C2D_DrawRectSolid(x + w - 1, y + 1, 0.0f, 1, 1, side);
        C2D_DrawRectSolid(x + 1, y + h - 1, 0.0f, 1, 1, lower);
        C2D_DrawRectSolid(x + w - 2, y + h - 1, 0.0f, 1, 1, lower);
        C2D_DrawRectSolid(x, y + h - 2, 0.0f, 1, 1, lower);
        C2D_DrawRectSolid(x + w - 1, y + h - 2, 0.0f, 1, 1, lower);
    }

    // left-aligned text, vertically centered in [y, y+h]
    inline void textLeft(const C2D_Text* t, float th, float x, float y, float h)
    {
        C2D_DrawText(t, C2D_WithColor, x, y + (h - th) / 2.0f, 0.0f, kTextScale, kTextScale,
                     conv(kTextCol));
    }

    // shared text buffer for widget labels, lifetime bracketed by init/exit
    void init();
    void exit();
    C2D_TextBuf labelBuf();
}
