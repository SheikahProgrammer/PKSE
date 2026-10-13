#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "UI/Panels/EncounterLookupPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Legality/EncounterLookup.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/PokemonTypes.h"
#include "Trainer/Trainer.h"

namespace UI
{
    namespace Panels
    {
        static Color getGameBadgeColor(const std::string &gameName)
        {
            if (gameName == "Red" || gameName == "FireRed" || gameName == "Scarlet")
                return Color(205, 45, 45);
            if (gameName == "Blue" || gameName == "Alpha Sapphire" || gameName == "Sword")
                return Color(45, 105, 215);
            if (gameName == "Yellow" || gameName == "Let's Go Pikachu")
                return Color(215, 175, 20);
            if (gameName == "Gold" || gameName == "HeartGold")
                return Color(200, 160, 30);
            if (gameName == "Silver" || gameName == "SoulSilver")
                return Color(145, 160, 175);
            if (gameName == "Crystal")
                return Color(60, 185, 215);
            if (gameName == "Ruby" || gameName == "Omega Ruby" || gameName == "Shield")
                return Color(205, 40, 60);
            if (gameName == "Sapphire")
                return Color(35, 85, 195);
            if (gameName == "Emerald")
                return Color(35, 165, 80);
            if (gameName == "LeafGreen")
                return Color(75, 180, 50);
            if (gameName == "Diamond" || gameName == "Brilliant Diamond")
                return Color(85, 140, 225);
            if (gameName == "Pearl" || gameName == "Shining Pearl")
                return Color(215, 115, 160);
            if (gameName == "Platinum")
                return Color(150, 150, 160);
            if (gameName == "Black" || gameName == "Black 2")
                return Color(45, 50, 60);
            if (gameName == "White" || gameName == "White 2")
                return Color(185, 190, 200);
            if (gameName == "X")
                return Color(40, 100, 210);
            if (gameName == "Y")
                return Color(210, 45, 55);
            if (gameName == "Sun" || gameName == "Ultra Sun")
                return Color(230, 125, 25);
            if (gameName == "Moon" || gameName == "Ultra Moon")
                return Color(50, 115, 205);
            if (gameName == "Let's Go Eevee")
                return Color(175, 115, 55);
            if (gameName == "Legends: Arceus")
                return Color(55, 130, 170);
            if (gameName == "Violet")
                return Color(115, 45, 195);
            if (gameName == "Legends: Z-A")
                return Color(45, 175, 120);

            return Colors::Primary;
        }

        void drawEncounterLookup(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();

            const int cardX = 40, cardY = 30, cardW = 1200, cardH = 660;

            // Soft shadow & modal card background
            framebuffer.drawSoftShadow(cardX, cardY, cardW, cardH, 20);
            framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Panel);
            framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Border, 2);

            // Header Banner
            constexpr int headerH = 56;
            framebuffer.drawCardBand(cardX, cardY, cardW, headerH);

            // Species info
            uint16_t speciesId = screen.encounterLookupSpecies;
            uint8_t formId = screen.encounterLookupForm;
            const char *spName = Trainer::getSpeciesName(speciesId);
            std::string displayName = spName ? spName : ("#" + std::to_string(speciesId));

            // Icon Sprite in Header
            constexpr int iconSize = 48;
            Sprite *iconSp = SpriteManager::getIconSprite(speciesId, formId, false);
            if (iconSp && iconSp->data)
            {
                framebuffer.drawSpriteFit(cardX + 16, cardY + (headerH - iconSize) / 2, iconSize, iconSize,
                                          iconSp->width, iconSp->height, iconSp->data, iconSp->channels);
            }

            // Title
            char titleBuf[128];
            std::snprintf(titleBuf, sizeof(titleBuf), "#%04d %s · Where to Catch", speciesId, displayName.c_str());
            framebuffer.drawText(cardX + 72, cardY + 14, titleBuf, Colors::Text, TextStyle::Heading);

            // Typing pills beside title
            const auto &pInfo = Pokemon::getPersonalInfo(speciesId, formId);
            framebuffer.drawTypePill(cardX + 480, cardY + 16, 75, 24, pInfo.type1);
            if (pInfo.type2 != 255 && pInfo.type2 != pInfo.type1)
            {
                framebuffer.drawTypePill(cardX + 565, cardY + 16, 75, 24, pInfo.type2);
            }

            // Close button top right
            const int closeW = 90, closeH = 30;
            const int closeX = cardX + cardW - closeW - 20, closeY = cardY + 13;
            framebuffer.drawPill(closeX, closeY, closeW, closeH, Color(255, 255, 255, 30));
            framebuffer.drawText(closeX + 16, closeY + 6, "B: Close", Colors::Text, TextStyle::Caption);
            screen.touchButtons.push_back({4999, closeX, closeY, closeW, closeH});

            // =========================================================================
            // Scope Tabs (ALL, THIS SAVE, GEN 1..9)
            // =========================================================================
            const int tabY = cardY + headerH + 12;
            const char *scopeLabels[11] = {
                "ALL (36 Games)", "THIS SAVE", "GEN I", "GEN II", "GEN III", "GEN IV",
                "GEN V", "GEN VI", "GEN VII", "GEN VIII", "GEN IX"
            };

            // Calculate scope query
            int queryScope = -1;
            if (screen.encounterLookupScope == 1)
                queryScope = 0; // Current save
            else if (screen.encounterLookupScope >= 2 && screen.encounterLookupScope <= 10)
                queryScope = screen.encounterLookupScope - 1; // Gen 1..9

            const auto &result = Legality::queryEncountersCached(speciesId, formId, queryScope, screen.trainer.getGameGroup());

            // Tab rendering
            int tabX = cardX + 24;
            for (int t = 0; t < 11; ++t)
            {
                bool active = (screen.encounterLookupScope == t);
                int tW, tH;
                framebuffer.measureText(scopeLabels[t], tW, tH, TextStyle::Caption);
                int pillW = tW + 18, pillH = 26;

                if (active)
                {
                    framebuffer.drawPill(tabX, tabY, pillW, pillH, Colors::Primary);
                    framebuffer.drawText(tabX + 9, tabY + (pillH - tH) / 2, scopeLabels[t], Colors::PrimaryText, TextStyle::Caption);
                }
                else
                {
                    framebuffer.drawPill(tabX, tabY, pillW, pillH, Colors::PanelAlt);
                    framebuffer.drawText(tabX + 9, tabY + (pillH - tH) / 2, scopeLabels[t], Colors::TextDim, TextStyle::Caption);
                }

                screen.touchButtons.push_back({4000 + t, tabX, tabY, pillW, pillH});
                tabX += pillW + 8;
            }

            // Breeding availability indicator on tab row
            if (result.hasBreeding)
            {
                int bX = cardX + cardW - 170, bY = tabY;
                framebuffer.drawPill(bX, bY, 150, 26, Color(45, 175, 125, 40));
                framebuffer.drawCircle(bX + 12, bY + 13, 5, Color(45, 175, 125), 2);
                framebuffer.drawText(bX + 24, bY + 6, "Egg Breeding Legal", Color(45, 175, 125), TextStyle::Caption);
            }

            // =========================================================================
            // Route List Display
            // =========================================================================
            const int listX = cardX + 24, listY = tabY + 38;
            const int listW = cardW - 48, listH = cardH - headerH - 105;

            framebuffer.drawFilledRoundedRect(listX, listY, listW, listH, 12, Colors::PanelAlt);
            framebuffer.drawRoundedRect(listX, listY, listW, listH, 12, Colors::Border, 1);

            const auto &routes = result.routes;
            if (routes.empty())
            {
                // Empty state message
                framebuffer.drawText(listX + 40, listY + 60, "No wild encounters found in this filter scope.",
                                     Colors::Text, TextStyle::Heading);
                framebuffer.drawText(listX + 40, listY + 100,
                                     "This Pokémon may be exclusive to events, transferable via PKSE Bank / Pokémon HOME,",
                                     Colors::TextDim, TextStyle::Body);
                framebuffer.drawText(listX + 40, listY + 126,
                                     "or obtainable by breeding an egg from a compatible nursery partner.",
                                     Colors::TextDim, TextStyle::Body);
            }
            else
            {
                constexpr int rowsPerPage = 5;
                const int rowH = 76;
                const int rowGap = 8;
                int maxScroll = std::max(0, static_cast<int>(routes.size()) - rowsPerPage);

                if (screen.encounterLookupSelectedRow < 0) screen.encounterLookupSelectedRow = 0;
                if (screen.encounterLookupSelectedRow >= static_cast<int>(routes.size()))
                    screen.encounterLookupSelectedRow = static_cast<int>(routes.size()) - 1;

                // Clamp scroll window to include selected row
                if (screen.encounterLookupSelectedRow < screen.encounterLookupScroll)
                    screen.encounterLookupScroll = screen.encounterLookupSelectedRow;
                else if (screen.encounterLookupSelectedRow >= screen.encounterLookupScroll + rowsPerPage)
                    screen.encounterLookupScroll = screen.encounterLookupSelectedRow - rowsPerPage + 1;

                int curY = listY + 12;
                int start = std::clamp(screen.encounterLookupScroll, 0, maxScroll);

                for (int i = 0; i < rowsPerPage && (start + i) < static_cast<int>(routes.size()); ++i)
                {
                    int rIdx = start + i;
                    const auto &entry = routes[rIdx];
                    bool selected = (rIdx == screen.encounterLookupSelectedRow);

                    // Row background
                    const int rx = listX + 14, rw = listW - 28;
                    framebuffer.drawFilledRoundedRect(rx, curY, rw, rowH, 10,
                                                      selected ? Color(40, 50, 75) : Colors::Panel);
                    if (selected)
                        framebuffer.drawRoundedRect(rx, curY, rw, rowH, 10, Colors::Primary, 2);

                    // 1. Game Badge (Left)
                    Color gColor = getGameBadgeColor(entry.gameName);
                    framebuffer.drawPill(rx + 16, curY + 14, 150, 28, gColor);
                    int gw, gh;
                    framebuffer.measureText(entry.gameName, gw, gh, TextStyle::Body);
                    framebuffer.drawText(rx + 16 + (150 - gw) / 2, curY + 14 + (28 - gh) / 2,
                                         entry.gameName, Colors::White, TextStyle::Body);

                    // Gen indicator under game badge
                    char genBuf[32];
                    std::snprintf(genBuf, sizeof(genBuf), "GEN %d", entry.generation);
                    framebuffer.drawText(rx + 16 + 55, curY + 48, genBuf, Colors::TextDim, TextStyle::Caption);

                    // 2. Location Name (Center bold)
                    framebuffer.drawText(rx + 185, curY + 14, entry.locationName, Colors::Text, TextStyle::Heading);

                    // Method pill
                    framebuffer.drawPill(rx + 185, curY + 44, 150, 22, Color(255, 255, 255, 30));
                    framebuffer.drawText(rx + 195, curY + 46, entry.kindStr, Colors::TextDim, TextStyle::Caption);

                    // Level range
                    char lvlBuf[48];
                    if (entry.minLevel == entry.maxLevel)
                        std::snprintf(lvlBuf, sizeof(lvlBuf), "Level %d", entry.minLevel);
                    else
                        std::snprintf(lvlBuf, sizeof(lvlBuf), "Lv. %d - %d", entry.minLevel, entry.maxLevel);

                    framebuffer.drawText(rx + 355, curY + 46, lvlBuf, Colors::Text, TextStyle::Body);

                    // 3. Shiny Policy Badge
                    Color sCol = (entry.shiny == Legality::ENCOUNTER_SHINY_NEVER) ? Color(205, 50, 50) :
                                 (entry.shiny == Legality::ENCOUNTER_SHINY_ALWAYS) ? Color(215, 175, 20) :
                                 Color(45, 165, 95);
                    framebuffer.drawPill(rx + rw - 330, curY + 24, 120, 26, sCol);
                    int sw, sh;
                    framebuffer.measureText(entry.shinyStr, sw, sh, TextStyle::Caption);
                    framebuffer.drawText(rx + rw - 330 + (120 - sw) / 2, curY + 24 + (26 - sh) / 2,
                                         entry.shinyStr, Colors::White, TextStyle::Caption);

                    // 4. Catch / Materialize Button (Right)
                    bool canMaterialize = Legality::canMaterializeInSave(screen.trainer, speciesId, formId);
                    const int btnW = 160, btnH = 38;
                    const int btnX = rx + rw - btnW - 16, btnY = curY + (rowH - btnH) / 2;

                    if (canMaterialize)
                    {
                        framebuffer.drawFilledRoundedRect(btnX, btnY, btnW, btnH, 8,
                                                          selected ? Colors::Primary : Colors::PanelAlt);
                        if (selected)
                            framebuffer.drawRoundedRect(btnX, btnY, btnW, btnH, 8, Colors::Accent, 1);
                        framebuffer.drawText(btnX + 16, btnY + 10, "A: Catch Mon",
                                             selected ? Colors::PrimaryText : Colors::Text, TextStyle::Body);
                    }
                    else
                    {
                        framebuffer.drawFilledRoundedRect(btnX, btnY, btnW, btnH, 8, Color(40, 45, 55));
                        framebuffer.drawText(btnX + 20, btnY + 10, "Other Save", Colors::TextDim, TextStyle::Caption);
                    }

                    // Register touch button for row & catch
                    screen.touchButtons.push_back({4100 + i, rx, curY, rw, rowH});
                    curY += rowH + rowGap;
                }

                // Scrollbar indicator
                if (routes.size() > rowsPerPage)
                {
                    const int sbX = listX + listW - 10, sbY = listY + 12;
                    const int sbH = listH - 24;
                    framebuffer.drawFilledRoundedRect(sbX, sbY, 4, sbH, 2, Color(255, 255, 255, 20));

                    float thumbPct = static_cast<float>(rowsPerPage) / routes.size();
                    int thumbH = std::max(20, static_cast<int>(sbH * thumbPct));
                    float scrollPct = static_cast<float>(screen.encounterLookupScroll) / maxScroll;
                    int thumbY = sbY + static_cast<int>((sbH - thumbH) * scrollPct);

                    framebuffer.drawFilledRoundedRect(sbX, thumbY, 4, thumbH, 2, Colors::Primary);
                }
            }

            // =========================================================================
            // Footer Control Guide
            // =========================================================================
            const int footY = cardY + cardH - 34;
            framebuffer.drawText(cardX + 24, footY,
                                 "A: Catch & Materialize into Box  |  L / R: Switch Generation  |  Up / Down: Select  |  B: Close",
                                 Colors::TextDim, TextStyle::Caption);

            // Counter info
            char countBuf[64];
            std::snprintf(countBuf, sizeof(countBuf), "Found %zu location(s)", routes.size());
            int cw, ch;
            framebuffer.measureText(countBuf, cw, ch, TextStyle::Caption);
            framebuffer.drawText(cardX + cardW - cw - 24, footY, countBuf, Colors::Primary, TextStyle::Caption);
        }
    }
}
