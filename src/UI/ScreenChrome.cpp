#include "UI/ScreenChrome.h"

#include <switch.h>
#include <SDL2/SDL.h>

namespace UI
{
    // The psm service is opened in main(); if that failed every query fails too and the strip just
    // draws a full battery, which is what PKForge shows when it cannot read one either.
    int statusBatteryPercent()
    {
        static int cached = -1;
        static uint32_t lastQueryMs = 0;
        const uint32_t now = SDL_GetTicks();
        if (lastQueryMs == 0 || now - lastQueryMs > 5000)
        {
            lastQueryMs = now == 0 ? 1 : now;
            u32 percent = 0;
            cached = R_SUCCEEDED(psmGetBatteryChargePercentage(&percent)) ? static_cast<int>(percent) : -1;
        }
        return cached;
    }
}
