#ifndef UI_PANELS_TRAINER_CARD_PANEL_H
#define UI_PANELS_TRAINER_CARD_PANEL_H

#include <cstdint>

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Panels
    {
        // Elegant in-game styled Pokémon Trainer Card view:
        // - Authentic card chassis with dynamic regional themes & colors
        // - Trainer avatar, name [edit], gender badge [♂/♀ toggle], TID16, SID16, Gen 7+ Display ID
        // - Wealth & Pokedex completion meters
        // - 6-slot signature party showcase (pixel sprites, levels, shiny stars, items)
        // - Velvet regional gym badges case (8 badges with vector jewel shapes & toggle)
        void drawTrainerCardView(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);

        uint8_t readSaveBadges(TrainerViewScreen &screen);
        void writeSaveBadges(TrainerViewScreen &screen, uint8_t badgeMask);
    }
}

#endif // UI_PANELS_TRAINER_CARD_PANEL_H
