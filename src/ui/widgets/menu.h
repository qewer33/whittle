#pragma once

#include <initializer_list>
#include <vector>
#include <citro2d.h>
#include "rect.h"
#include "icons.h"

namespace widgets
{
    enum class Placement { Below, Above, LeftOf };
    enum class Align { Start, End }; // anchor's left / right edge (Below/Above)

    struct MenuItem
    {
        Icon icon;
        const char* label;
    };

    // popup list of icon+label rows. owns its layout, draw, and hit-test
    struct Menu
    {
        bool open = false;

        void setup(Rect anchor, Placement place, Align align, int width,
                   std::initializer_list<MenuItem> items, bool closeOnPick = true,
                   const char* title = nullptr, int cols = 1);

        int count() const { return (int)icons_.size(); }
        Rect itemRect(int i) const;

        void draw(const bool* on = nullptr) const; // on[i] highlights row i

        // returns the picked row, or -1 on a miss. dismissal is applied on
        // finishTouch(), and picks stay open when closeOnPick is false.
        int handle(int px, int py);
        void finishTouch();
        void cancelClose() { closePending_ = false; }

    private:
        Rect anchor_{};
        Placement place_ = Placement::Below;
        Align align_ = Align::Start;
        int width_ = 90;
        bool closeOnPick_ = true;
        std::vector<Icon> icons_;
        std::vector<C2D_Text> labels_;
        std::vector<float> labelH_;
        bool hasTitle_ = false;
        C2D_Text title_{};
        float titleH_ = 0.0f;
        int cols_ = 1;
        static constexpr int kItemH = 24;
        static constexpr int kHeaderH = 22;
        bool closePending_ = false;
        int pressedItem_ = -1;

        int headerH() const { return hasTitle_ ? kHeaderH : 0; }
        int rows() const { return (count() + cols_ - 1) / cols_; }
        int leftX() const;
        int topY() const;
    };
}
