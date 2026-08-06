#include "icons.h"
#include <citro2d.h>
#include "icons_t3x.h"

static C2D_SpriteSheet sheet = nullptr;

namespace icons
{
    bool init()
    {
        sheet = C2D_SpriteSheetLoadFromMem(icons_t3x, icons_t3x_size);
        return sheet != nullptr;
    }

    void exit()
    {
        if (sheet)
            C2D_SpriteSheetFree(sheet);
        sheet = nullptr;
    }

    void draw(Icon ic, float x, float y, float size, u32 color)
    {
        if (!sheet || (size_t)ic >= C2D_SpriteSheetCount(sheet))
            return;
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, (size_t)ic);
        const float base = (img.subtex && img.subtex->width) ? img.subtex->width : 24.0f;
        const float sc = size / base;

        C2D_ImageTint tint;
        const u32 c = C2D_Color32((color >> 24) & 0xFF, (color >> 16) & 0xFF,
                                  (color >> 8) & 0xFF, color & 0xFF);
        C2D_PlainImageTint(&tint, c, 1.0f); // blend=1 recolors the white icon
        C2D_DrawImageAt(img, x, y, 0.5f, &tint, sc, sc);
    }
}
