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
                   std::initializer_list<MenuItem> items, bool closeOnPick = true);

        int count() const { return (int)icons_.size(); }
        Rect itemRect(int i) const;

        void draw(const bool* on = nullptr) const; // on[i] highlights row i

        // returns the picked row, or -1 on a miss. closes on dismiss, and on pick
        // unless closeOnPick is false.
        int handle(int px, int py);

    private:
        Rect anchor_{};
        Placement place_ = Placement::Below;
        Align align_ = Align::Start;
        int width_ = 90;
        bool closeOnPick_ = true;
        std::vector<Icon> icons_;
        std::vector<C2D_Text> labels_;
        std::vector<float> labelH_;
        static constexpr int kItemH = 30;
    };
}
