#include "menu.h"
#include "uidraw.h"

using namespace uidraw;

namespace widgets
{
    void Menu::setup(Rect anchor, Placement place, Align align, int width,
                     std::initializer_list<MenuItem> items, bool closeOnPick,
                     const char* title)
    {
        anchor_ = anchor;
        place_ = place;
        align_ = align;
        width_ = width;
        closeOnPick_ = closeOnPick;
        hasTitle_ = title != nullptr;
        if (hasTitle_)
        {
            C2D_TextParse(&title_, labelBuf(), title);
            C2D_TextOptimize(&title_);
            float w;
            C2D_TextGetDimensions(&title_, kTextScale, kTextScale, &w, &titleH_);
        }
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

    int Menu::leftX() const
    {
        if (place_ == Placement::LeftOf)
            return anchor_.x - width_;
        return align_ == Align::Start ? anchor_.x : anchor_.x + anchor_.w - width_;
    }

    int Menu::topY() const
    {
        if (place_ == Placement::Above)
            return anchor_.y - (headerH() + count() * kItemH);
        if (place_ == Placement::LeftOf)
            return anchor_.y;
        return anchor_.y + anchor_.h;
    }

    Rect Menu::itemRect(int i) const
    {
        return {leftX(), topY() + headerH() + i * kItemH, width_, kItemH};
    }

    void Menu::draw(const bool* on) const
    {
        const int n = count();
        if (n == 0)
            return;

        raisedButton(leftX(), topY() - 1, width_, headerH() + n * kItemH + 2, false);

        if (hasTitle_)
        {
            const int hx = leftX(), hy = topY();
            C2D_DrawText(&title_, C2D_WithColor, hx + 7,
                         hy + (kHeaderH - titleH_) / 2.0f, 0.0f, kTextScale, kTextScale,
                         conv(kIconIdle));
            C2D_DrawRectSolid(hx, hy + kHeaderH - 1, 0.0f, width_, 1, conv(kBarHighlight));
        }
        for (int i = 0; i < n; i++)
        {
            const Rect r = itemRect(i);
            const bool hi = (on && on[i]) || i == pressedItem_;
            const Rect body = {r.x + 1, r.y, r.w - 2, r.h};
            if (hi)
                raisedButton(body.x, body.y, body.w, body.h, true);
            icons::draw(icons_[i], body.x + 5, body.y + (body.h - kIcon) / 2.0f, kIcon,
                        hi ? kIconActive : kIconIdle);
            C2D_DrawText(&labels_[i], C2D_WithColor, body.x + 5 + kIcon + 4,
                         body.y + (body.h - labelH_[i]) / 2.0f, 0.0f, kTextScale,
                         kTextScale, conv(hi ? kIconActive : kTextCol));
        }
    }

    int Menu::handle(int px, int py)
    {
        for (int i = 0; i < count(); i++)
            if (itemRect(i).contains(px, py))
            {
                pressedItem_ = i;
                if (closeOnPick_)
                    closePending_ = true;
                return i;
            }
        pressedItem_ = -1;
        closePending_ = true; // tapped outside; dismiss on touch-up
        return -1;
    }

    void Menu::finishTouch()
    {
        if (closePending_)
            open = false;
        closePending_ = false;
        pressedItem_ = -1;
    }
}
