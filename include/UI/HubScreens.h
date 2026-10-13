#ifndef UI_HUB_SCREENS_H
#define UI_HUB_SCREENS_H

#include <string>
#include <vector>

#include "UI/UIScreen.h"
#include "UI/Panels/PokeparkPanel.h"

namespace UI
{
    /**
     * The places on the top-level menu that belong to no one game (PKForge's home cards): the
     * Poképark, where Pokémon from every save hang out, and the app's Settings. Each runs as its own
     * screen between visits to the save picker.
     */
    class PokeparkScreen : public UIScreen
    {
    public:
        PokeparkScreen();
        void update(const PadState &pad, const TouchInput &touch) override;
        void draw(PKSEFramebuffer &framebuffer) override;
        bool shouldExit() const override { return view.exitRequested; }

    private:
        PokeparkView view;
    };

    class SettingsScreen : public UIScreen
    {
    public:
        void update(const PadState &pad, const TouchInput &touch) override;
        void draw(PKSEFramebuffer &framebuffer) override;
        bool shouldExit() const override { return exitRequested; }

    private:
        int selectedRow = 0;
        bool exitRequested = false;
        std::vector<TouchButton> touchButtons;
        std::string status;
        float statusTimer = 0.0f;
    };
}

#endif
