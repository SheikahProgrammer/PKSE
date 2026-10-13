#include "UI/TouchInput.h"

#include <cmath>

#include "UI/PKSEFramebuffer.h"

namespace UI
{
    void TouchInput::update()
    {
        prevDown = curDown;

        HidTouchScreenState touchState{};
        if (hidGetTouchScreenStates(&touchState, 1) > 0 && touchState.count > 0)
        {
            curX = static_cast<int>(touchState.touches[0].x);
            curY = static_cast<int>(touchState.touches[0].y);
            // A popup still growing in is drawn smaller than the buttons it registered: map the
            // finger back through that scale so the tap lands on the button under it.
            // The mapping is latched when the finger goes down and kept until it lifts, so a touch
            // that outlasts the animation does not jump (and read as a drag) halfway through.
            if (!curDown)
                latched = PKSEFramebuffer::popInScale(latchX, latchY, latchScale) && latchScale > 0.0f;
            if (latched)
            {
                curX = static_cast<int>(std::lround(latchX + (curX - latchX) / latchScale));
                curY = static_cast<int>(std::lround(latchY + (curY - latchY) / latchScale));
            }
            // Record the start position on the press edge (curDown still holds the previous frame here).
            if (!curDown)
            {
                begX = curX;
                begY = curY;
            }
            curDown = true;
        }
        else
        {
            curDown = false;
        }
    }

    bool TouchInput::dragged() const
    {
        const int deltaX = curX - begX, deltaY = curY - begY;
        return (deltaX * deltaX + deltaY * deltaY) > (20 * 20); // ~20px movement threshold
    }
}
