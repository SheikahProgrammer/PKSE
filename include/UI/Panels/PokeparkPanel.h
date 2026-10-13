#ifndef UI_PANELS_POKEPARK_PANEL_H
#define UI_PANELS_POKEPARK_PANEL_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI
{
    namespace Panels
    {
        void drawPokepark(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif // UI_PANELS_POKEPARK_PANEL_H
