#ifndef UI_PANELS_LIVING_DEX_PANEL_H
#define UI_PANELS_LIVING_DEX_PANEL_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Panels
    {
        /**
         * 1,025-Pokémon HOME-style Living Dex and Collection Tracker Panel.
         *
         * Features:
         * - 30-slot grid (6 columns x 5 rows) matching classic Pokémon box & HOME layouts.
         * - Comprehensive collection scanning across active save (party + boxes) and unified Bank.
         * - Visual states: full-color sprite + Poké Ball badge for owned, silhouette/dimmed for missing.
         * - Sparkling shiny star badge for shiny owned species.
         * - Filter scope tabs: ALL, This Game, Gen I through Gen IX.
         * - Toggle filters: Missing Only [Y], Shiny Living Dex [-].
         * - Cursor preview banner with species name, dex #, and dual type pills.
         * - Interactive Location Inspector Modal [A] showing exact box/slot coordinates for every copy.
         * - Hardware-accurate Retro DS chrome support under ThemeMode::RetroDS.
         */
        void drawLivingDexView(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif // UI_PANELS_LIVING_DEX_PANEL_H
