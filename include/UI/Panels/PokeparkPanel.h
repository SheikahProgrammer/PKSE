#ifndef UI_PANELS_POKEPARK_PANEL_H
#define UI_PANELS_POKEPARK_PANEL_H

#include <string>
#include <vector>

#include <switch.h>

#include "UI/Common.h"

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;
    class TouchInput;

    /**
     * The Poképark as a place you visit: everything its screen shows and remembers between frames.
     *
     * Not part of any one game. The park is a hangout for Pokémon from every save -- each game
     * only decides who of ITS Pokémon live there (see Panels::drawParkRoster) -- so it is opened
     * from the top-level menu and runs without a save loaded.
     */
    struct PokeparkView
    {
        int pokeparkSelectedResident = 0;
        bool pokeparkCardActive = false;
        bool pokeparkJournalActive = false;
        int pokeparkJournalScroll = 0;
        int pokeparkCardActionSelected = 0;
        std::string pokeparkDialogueText;
        float pokeparkDialogueTimer = 0.0f;
        int pokeparkBerryFlavor = 2;       // the berry the card's Snack (and ZR) offers: 0 spicy .. 4 sour
        bool pokeparkPhotoMode = false;    // UI hidden, the park held still
        bool pokeparkPhotoCapture = false; // this frame is the one being saved
        bool pokeparkPhotoPending = false; // saved last frame; waiting to hear whether it worked
        std::string pokeparkPhotoName;
        std::string pokeparkToast;
        float pokeparkToastTimer = 0.0f;
        std::vector<TouchButton> touchButtons;
        bool exitRequested = false;

        // Quick menu (Down): call / tuck in / berries for everyone, the activity filter, follow camera,
        // park speed and effects.
        bool quickMenuActive = false;
        int quickMenuRow = 0;
        int activityFilter = 0; // 0 all, 1 sleeping, 2 playing, 3 lonely, 4 quarrelling
        bool followCamera = false;
        // Journal: X cycles the kind shown (0 = all).
        int journalFilter = 0;
        // "While you were away", shown once on entering when the park had news.
        std::vector<std::string> awayLines;

        int touchedButtonId(const TouchInput &touch) const;
        void handleInput(u64 buttonsDown, const TouchInput &touch);
    };

    namespace Panels
    {
        void drawPokepark(PokeparkView &view, PKSEFramebuffer &framebuffer);
        /// Inside a game: choose which of this save's Pokémon live in the shared park.
        void drawParkRoster(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif // UI_PANELS_POKEPARK_PANEL_H
