#pragma once

#include <3ds.h>
#include <citro2d.h>

// shared UI theme + draw primitives, used by ui.cpp and the widgets
namespace uidraw
{
    inline constexpr u32 kBarBg = 0x1B1E26FF;
    inline constexpr u32 kActiveBg = 0x4E7BCCFF;
    inline constexpr u32 kItemBg = 0x333A4AFF;
    inline constexpr u32 kPanelBg = 0x272B36FF;
    inline constexpr u32 kBorderCol = 0x66707EFF;
    inline constexpr u32 kIconIdle = 0xC2C8D4FF;
    inline constexpr u32 kIconActive = 0xFFFFFFFF;
    inline constexpr u32 kIconDim = 0x565D6EFF;
    inline constexpr u32 kTextCol = 0xE8ECF4FF;
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
