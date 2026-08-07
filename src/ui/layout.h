#pragma once

#include "rect.h"

// single source of truth for bottom-screen UI geometry
namespace layout
{
    constexpr int kScreenW = 320;
    constexpr int kScreenH = 240;
    constexpr int kBarH = 22;                      // top and bottom bar height
    constexpr int kBottomBarY = kScreenH - kBarH;
    constexpr int kContentY = kBarH;               // area between the two bars
    constexpr int kContentH = kScreenH - 2 * kBarH;

    inline Rect topBar() { return {0, 0, kScreenW, kBarH}; }
    inline Rect bottomBar() { return {0, kBottomBarY, kScreenW, kBarH}; }
    inline Rect content() { return {0, kContentY, kScreenW, kContentH}; }

    // cell i of a centered segmented switch (count cells), floating inset in the
    // bottom bar. shared by draw and hit-test so they agree.
    inline Rect segCell(int i, int count)
    {
        constexpr int segW = 44, segPad = 3;
        const int x0 = kScreenW / 2 - count * segW / 2;
        return {x0 + i * segW, kBottomBarY + segPad, segW, kBarH - 2 * segPad};
    }
}
