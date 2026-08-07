#pragma once

#include <initializer_list>
#include <vector>
#include "icons.h"

namespace widgets
{
    // a centered segmented switch in the bottom bar. owns its draw and hit-test.
    // the caller passes the active cell, and acts on the picked one.
    struct ToolSwitch
    {
        void setup(std::initializer_list<Icon> icons) { icons_ = icons; }
        int count() const { return (int)icons_.size(); }

        void draw(int active) const;      // active cell highlighted
        int handle(int px, int py) const; // hit cell, or -1

    private:
        std::vector<Icon> icons_;
    };
}
