#ifndef UI_PANELS_ENCOUNTER_LOOKUP_PANEL_H
#define UI_PANELS_ENCOUNTER_LOOKUP_PANEL_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Panels
    {
        /**
         * Offline Route & Encounter Database Lookup Panel.
         * Answers "Where can I catch this Pokémon?" across all 36 mainline games.
         * Displays exact routes, encounter methods, level ranges, shiny locks,
         * and supports 1-tap catch/materialize into the active save's box.
         */
        void drawEncounterLookup(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif // UI_PANELS_ENCOUNTER_LOOKUP_PANEL_H
