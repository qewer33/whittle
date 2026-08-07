#include "menu.h"
#include "uidraw.h"

using namespace uidraw;

namespace widgets
{
    void Menu::setup(Rect anchor, Placement place, Align align, int width,
                     std::initializer_list<MenuItem> items, bool closeOnPick)
    {
        anchor_ = anchor;
        place_ = place;
        align_ = align;
        width_ = width;
        closeOnPick_ = closeOnPick;
        icons_.clear();
        labels_.clear();
        labelH_.clear();
        for (const MenuItem& it : items)
        {
            icons_.push_back(it.icon);
            C2D_Text t;
            C2D_TextParse(&t, labelBuf(), it.label);
            C2D_TextOptimize(&t);
            float w, h;
            C2D_TextGetDimensions(&t, kTextScale, kTextScale, &w, &h);
            labels_.push_back(t);
            labelH_.push_back(h);
        }
    }

    Rect Menu::itemRect(int i) const
    {
        int x;
        if (place_ == Placement::LeftOf)
            x = anchor_.x - width_;
        else
            x = align_ == Align::Start ? anchor_.x : anchor_.x + anchor_.w - width_;

        int y;
        if (place_ == Placement::Above)
            y = anchor_.y - count() * kItemH + i * kItemH;
        else if (place_ == Placement::LeftOf)
            y = anchor_.y + i * kItemH;
        else
            y = anchor_.y + anchor_.h + i * kItemH;

        return {x, y, width_, kItemH};
    }

    void Menu::draw(const bool* on) const
    {
        const int n = count();
        const Rect first = itemRect(0);
        const Rect last = itemRect(n - 1);
        const int panX = first.x - 2, panY = first.y - 2;
        const int panW = first.w + 4, panH = last.y + last.h - first.y + 4;
        C2D_DrawRectSolid(panX, panY, 0.0f, panW, panH, conv(kPanelBg));
        outline(panX, panY, panW, panH, kBorderCol);
        for (int i = 0; i < n; i++)
        {
            const Rect r = itemRect(i);
            const bool hi = on && on[i];
            C2D_DrawRectSolid(r.x, r.y, 0.0f, r.w, r.h, conv(hi ? kActiveBg : kItemBg));
            outline(r.x, r.y, r.w, r.h, kBorderCol);
            icons::draw(icons_[i], r.x + 5, r.y + (r.h - kIcon) / 2.0f, kIcon,
                        hi ? kIconActive : kIconIdle);
            textLeft(&labels_[i], labelH_[i], r.x + 5 + kIcon + 4, r.y, r.h);
        }
    }

    int Menu::handle(int px, int py)
    {
        for (int i = 0; i < count(); i++)
            if (itemRect(i).contains(px, py))
            {
                if (closeOnPick_)
                    open = false;
                return i;
            }
        open = false; // tapped outside
        return -1;
    }
}
