#include <algorithm>
#include <ctime>
#include <sys/stat.h>

#include "UI/HubScreens.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/Pokepark.h"
#include "UI/ScreenChrome.h"
#include "UI/SettingsView.h"
#include "UI/TouchInput.h"

namespace UI
{
    // ---- Poképark -----------------------------------------------------------------------------

    int PokeparkView::touchedButtonId(const TouchInput &touch) const
    {
        if (!touch.justPressed())
            return -1;
        for (const auto &b : touchButtons)
            if (touch.x() >= b.hitX && touch.x() < b.hitX + b.hitWidth && touch.y() >= b.hitY &&
                touch.y() < b.hitY + b.hitHeight)
                return b.buttonId;
        return -1;
    }

    void PokeparkView::handleInput(u64 buttonsDown, const TouchInput &touch)
    {
        auto &mgr = UI::PokeparkManager::instance();
        auto visibleIndices = mgr.getVisibleResidentIndices(mgr.getCurrentHabitat());
        int touchedId = touchedButtonId(touch);
        const bool haveSelection = !visibleIndices.empty() && pokeparkSelectedResident >= 0 &&
                                   pokeparkSelectedResident < static_cast<int>(visibleIndices.size());
        const int selectedIndex = haveSelection ? visibleIndices[pokeparkSelectedResident] : -1;

        // News from while you were away: any button (or a tap) dismisses it.
        if (!awayLines.empty())
        {
            if (buttonsDown || touch.justPressed())
                awayLines.clear();
            return;
        }

        // Quick menu.
        if (quickMenuActive)
        {
            constexpr int rows = 7;
            if (buttonsDown & HidNpadButton_B)
            {
                quickMenuActive = false;
                return;
            }
            if (touchedId >= 500 && touchedId < 500 + rows)
            {
                quickMenuRow = touchedId - 500;
                buttonsDown |= HidNpadButton_A;
            }
            if (buttonsDown & HidNpadButton_Up)
                quickMenuRow = (quickMenuRow + rows - 1) % rows;
            if (buttonsDown & HidNpadButton_Down)
                quickMenuRow = (quickMenuRow + 1) % rows;
            if (buttonsDown & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right))
            {
                switch (quickMenuRow)
                {
                case 0: mgr.callEveryone(); quickMenuActive = false; break;
                case 1: mgr.tuckInEveryone(); quickMenuActive = false; break;
                case 2: mgr.berryForAll(); quickMenuActive = false; break;
                case 3:
                    activityFilter = (activityFilter + ((buttonsDown & HidNpadButton_Left) ? 4 : 1)) % 5;
                    break;
                case 4: followCamera = !followCamera; break;
                case 5: mgr.cycleSpeed(); break;
                case 6: mgr.toggleEffects(); break;
                }
            }
            return;
        }

        // Photo mode: the UI is hidden and the park holds still; A saves exactly what is on screen.
        if (pokeparkPhotoMode)
        {
            if ((buttonsDown & HidNpadButton_A) && !pokeparkPhotoCapture && !pokeparkPhotoPending)
            {
                mkdir("sdmc:/PKSE", 0777);
                mkdir("sdmc:/PKSE/photos", 0777);
                char name[64];
                const std::time_t now = std::time(nullptr);
                std::strftime(name, sizeof(name), "park_%Y%m%d_%H%M%S.png", std::localtime(&now));
                pokeparkPhotoName = name;
                pokeparkPhotoCapture = true;
            }
            if (buttonsDown & (HidNpadButton_B | HidNpadButton_Minus))
                pokeparkPhotoMode = false;
            return;
        }

        // 1. Dialogue overlay active
        if (!pokeparkDialogueText.empty())
        {
            if ((buttonsDown & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y)) || touchedId == 350)
            {
                pokeparkDialogueText.clear();
                pokeparkDialogueTimer = 0.0f;
            }
            return;
        }

        // 2. Journal
        if (pokeparkJournalActive)
        {
            if (buttonsDown & (HidNpadButton_B | HidNpadButton_Y))
            {
                pokeparkJournalActive = false;
                return;
            }
            if (buttonsDown & HidNpadButton_X)
            {
                journalFilter = (journalFilter + 1) % 7;
                pokeparkJournalScroll = 0;
                return;
            }
            const int maxScroll = std::max(0, static_cast<int>(mgr.getJournal().size()) - 1);
            if (buttonsDown & HidNpadButton_Up)
                pokeparkJournalScroll = std::max(0, pokeparkJournalScroll - 1);
            if (buttonsDown & HidNpadButton_Down)
                pokeparkJournalScroll = std::min(maxScroll, pokeparkJournalScroll + 1);
            if (buttonsDown & HidNpadButton_Left)
                pokeparkJournalScroll = std::max(0, pokeparkJournalScroll - 5);
            if (buttonsDown & HidNpadButton_Right)
                pokeparkJournalScroll = std::min(maxScroll, pokeparkJournalScroll + 5);
            return;
        }

        // 3. Resident profile card: Talk / Play / Snack / Relax / Call, and the berry for Snack.
        if (pokeparkCardActive)
        {
            if (buttonsDown & HidNpadButton_B)
            {
                pokeparkCardActive = false;
                return;
            }
            int actionTrigger = -1;
            if (buttonsDown & HidNpadButton_Left)
                pokeparkCardActionSelected = (pokeparkCardActionSelected - 1 + 5) % 5;
            if (buttonsDown & HidNpadButton_Right)
                pokeparkCardActionSelected = (pokeparkCardActionSelected + 1) % 5;
            if (buttonsDown & HidNpadButton_Up)
                pokeparkBerryFlavor = (pokeparkBerryFlavor + 4) % 5;
            if (buttonsDown & HidNpadButton_Down)
                pokeparkBerryFlavor = (pokeparkBerryFlavor + 1) % 5;
            if (buttonsDown & HidNpadButton_A)
                actionTrigger = pokeparkCardActionSelected;
            if (buttonsDown & HidNpadButton_X)
                actionTrigger = 1;
            if (buttonsDown & HidNpadButton_Y)
                actionTrigger = 2;
            if (buttonsDown & (HidNpadButton_R | HidNpadButton_ZR))
                actionTrigger = 3;
            if (buttonsDown & HidNpadButton_ZL)
                actionTrigger = 4;
            if (touchedId >= 300 && touchedId <= 304)
                actionTrigger = touchedId - 300;

            if (actionTrigger >= 0 && selectedIndex >= 0)
            {
                pokeparkCardActionSelected = actionTrigger;
                switch (actionTrigger)
                {
                case 0: pokeparkDialogueText = mgr.interact(selectedIndex, "talk"); break;
                case 1: pokeparkDialogueText = mgr.interact(selectedIndex, "play"); break;
                case 2:
                    pokeparkDialogueText =
                        mgr.feedBerry(selectedIndex, static_cast<UI::ParkFlavor>(std::clamp(pokeparkBerryFlavor, 0, 4)));
                    break;
                case 3: pokeparkDialogueText = mgr.interact(selectedIndex, "relax"); break;
                case 4:
                    pokeparkDialogueText = mgr.callResident(selectedIndex);
                    pokeparkCardActive = false; // watch it come over
                    break;
                }
                pokeparkDialogueTimer = 5.0f;
            }
            return;
        }

        // 4. The habitat.
        if (touchedId >= 200 && touchedId < 250)
        {
            const int vi = touchedId - 200;
            if (vi >= 0 && vi < static_cast<int>(visibleIndices.size()))
            {
                if (pokeparkSelectedResident == vi)
                {
                    pokeparkCardActive = true;
                    pokeparkCardActionSelected = 0;
                }
                else
                    pokeparkSelectedResident = vi;
            }
            return;
        }
        if (touchedId == 270)
        {
            // Open ground: throw a ball there (scene coordinates are the panel's).
            mgr.throwToy(UI::ParkToyKind::Ball, static_cast<float>(touch.x() - 32), static_cast<float>(touch.y() - 96));
            return;
        }
        if (touchedId == 250 || (buttonsDown & HidNpadButton_L))
        {
            mgr.cycleHabitat(-1);
            pokeparkSelectedResident = 0;
            return;
        }
        if (touchedId == 251 || (buttonsDown & HidNpadButton_R))
        {
            mgr.cycleHabitat(1);
            pokeparkSelectedResident = 0;
            return;
        }
        if (touchedId == 260 || (buttonsDown & HidNpadButton_Y))
        {
            pokeparkJournalActive = true;
            pokeparkJournalScroll = 0;
            return;
        }
        if (buttonsDown & HidNpadButton_Minus)
        {
            pokeparkPhotoMode = true;
            return;
        }
        if (buttonsDown & HidNpadButton_Down)
        {
            quickMenuActive = true;
            quickMenuRow = 0;
            return;
        }
        if (!visibleIndices.empty())
        {
            if (buttonsDown & HidNpadButton_Left)
                pokeparkSelectedResident = (pokeparkSelectedResident - 1 + static_cast<int>(visibleIndices.size())) %
                                           static_cast<int>(visibleIndices.size());
            if (buttonsDown & HidNpadButton_Right)
                pokeparkSelectedResident = (pokeparkSelectedResident + 1) % static_cast<int>(visibleIndices.size());
        }
        if (selectedIndex >= 0 && (buttonsDown & (HidNpadButton_ZL | HidNpadButton_ZR)))
        {
            // Throw near the selected resident, a little in front of it.
            const auto &res = mgr.getResidents()[selectedIndex];
            const float x = res.x + (res.facingLeft ? -1.0f : 1.0f) * 140.0f, y = res.y + 20.0f;
            if (buttonsDown & HidNpadButton_ZL)
                mgr.throwToy(UI::ParkToyKind::Ball, x, y);
            else
                mgr.throwToy(UI::ParkToyKind::Berry, x, y, static_cast<UI::ParkFlavor>(std::clamp(pokeparkBerryFlavor, 0, 4)));
            return;
        }
        if (selectedIndex >= 0 && (buttonsDown & HidNpadButton_Up))
        {
            pokeparkDialogueText = mgr.callResident(selectedIndex);
            return;
        }

        if (buttonsDown & HidNpadButton_A)
        {
            if (selectedIndex >= 0)
            {
                pokeparkCardActive = true;
                pokeparkCardActionSelected = 0;
            }
        }
        else if (buttonsDown & HidNpadButton_X)
        {
            if (selectedIndex >= 0)
            {
                pokeparkDialogueText = mgr.interact(selectedIndex, "talk");
                pokeparkDialogueTimer = 5.0f;
            }
        }
        else if (buttonsDown & HidNpadButton_B)
        {
            mgr.markVisit(); // also saves needs and bonds as they are right now
            exitRequested = true;
        }
    }

    PokeparkScreen::PokeparkScreen()
    {
        // The park loads on its own: it needs no save, only the residents each game sent over.
        PokeparkManager &mgr = PokeparkManager::instance();
        mgr.initialize(nullptr, nullptr);
        // What happened since the park was last left, newest first (nothing on the very first visit).
        if (mgr.lastVisitTime() > 0)
        {
            const auto news = mgr.journalSince(mgr.lastVisitTime());
            for (auto it = news.rbegin(); it != news.rend() && view.awayLines.size() < 6; ++it)
                view.awayLines.push_back((*it)->timeStr + "  " + (*it)->text);
            if (news.size() > view.awayLines.size())
                view.awayLines.push_back("... and " + std::to_string(news.size() - view.awayLines.size()) +
                                         " more in the journal (Y).");
        }
    }

    void PokeparkScreen::update(const PadState &pad, const TouchInput &touch)
    {
        const u64 buttonsDown = padGetButtonsDown(&pad) | navTouchButton(touch);
        if (PKSEFramebuffer::typingInProgress() && ((buttonsDown & HidNpadButton_A) || touch.justPressed()))
        {
            PKSEFramebuffer::skipTyping();
            return;
        }
        view.handleInput(buttonsDown, touch);
    }

    void PokeparkScreen::draw(PKSEFramebuffer &framebuffer)
    {
        framebuffer.clear(Colors::Background);
        drawTitleBar(framebuffer, "Poképark", "Pokémon from all your games");
        Panels::drawPokepark(view, framebuffer);
    }

    // ---- Settings -----------------------------------------------------------------------------

    void SettingsScreen::update(const PadState &pad, const TouchInput &touch)
    {
        u64 buttonsDown = padGetButtonsDown(&pad) | navTouchButton(touch);
        const int rows = SettingsView::visibleRowCount();
        if (buttonsDown & HidNpadButton_Up)
            selectedRow = (selectedRow - 1 + rows) % rows;
        if (buttonsDown & HidNpadButton_Down)
            selectedRow = (selectedRow + 1) % rows;
        if (touch.justPressed())
            for (const auto &b : touchButtons)
                if (touch.x() >= b.hitX && touch.x() < b.hitX + b.hitWidth && touch.y() >= b.hitY &&
                    touch.y() < b.hitY + b.hitHeight && b.buttonId < rows)
                {
                    selectedRow = b.buttonId;
                    buttonsDown |= HidNpadButton_A;
                }
        if (buttonsDown & HidNpadButton_A)
        {
            const std::string line = SettingsView::activate(selectedRow);
            if (!line.empty())
            {
                status = line;
                statusTimer = 4.0f;
            }
        }
        if (buttonsDown & (HidNpadButton_B | HidNpadButton_Plus))
            exitRequested = true;
    }

    void SettingsScreen::draw(PKSEFramebuffer &framebuffer)
    {
        framebuffer.clear(Colors::Background);
        drawTitleBar(framebuffer, "Settings", "For every game");
        const int top = HEADER_HEIGHT + 12, bottom = framebuffer.getHeight() - NAV_BAR_HEIGHT - 12;
        {
            PopIn pop(framebuffer, "hub.settings");
            SettingsView::draw(framebuffer, 140, top, framebuffer.getWidth() - 280, bottom - top, selectedRow, touchButtons);
        }
        drawNavBar(framebuffer, "Up/Down: Select  |  A: Change  |  B: Back");
        if (statusTimer > 0.0f && !status.empty())
        {
            statusTimer -= framebuffer.getDeltaSeconds();
            int tw, th;
            framebuffer.measureText(status, tw, th);
            const int bw = tw + 36, bh = th + 14, bx = (framebuffer.getWidth() - bw) / 2, by = bottom - bh - 6;
            framebuffer.drawFilledRoundedRect(bx, by, bw, bh, 8, Colors::Panel);
            framebuffer.drawRoundedRect(bx, by, bw, bh, 8, Colors::Accent, 2);
            framebuffer.drawText(bx + 18, by + 7, status, Colors::Text);
        }
    }
}
