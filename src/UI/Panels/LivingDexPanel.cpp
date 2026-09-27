#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "UI/Panels/LivingDexPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Pokemon/LivingDex.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Trainer/Trainer.h"

namespace UI
{
    namespace Panels
    {
        static void drawMiniPokeBall(PKSEFramebuffer &framebuffer, int cx, int cy, int r)
        {
            framebuffer.drawFilledCircle(cx, cy, r, Color(20, 20, 30));
            framebuffer.drawFilledCircle(cx, cy, r - 1, Color(245, 245, 250));
            framebuffer.drawFilledCircle(cx, cy, r - 1, Color(225, 45, 40));
            framebuffer.drawFilledRect(cx - r + 1, cy, (r - 1) * 2, r - 1, Color(245, 245, 250));
            framebuffer.drawFilledRect(cx - r + 1, cy - 1, (r - 1) * 2, 2, Color(20, 20, 30));
            framebuffer.drawFilledCircle(cx, cy, std::max(2, r / 3), Color(20, 20, 30));
            framebuffer.drawFilledCircle(cx, cy, std::max(1, r / 4), Color(255, 255, 255));
        }

        static const char *getGenName(int gen)
        {
            switch (gen)
            {
            case 1:
                return "Gen I (Kanto)";
            case 2:
                return "Gen II (Johto)";
            case 3:
                return "Gen III (Hoenn)";
            case 4:
                return "Gen IV (Sinnoh)";
            case 5:
                return "Gen V (Unova)";
            case 6:
                return "Gen VI (Kalos)";
            case 7:
                return "Gen VII (Alola)";
            case 8:
                return "Gen VIII (Galar)";
            case 9:
                return "Gen IX (Paldea)";
            default:
                return "Unknown Gen";
            }
        }

        void drawLivingDexView(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();

            if (screen.livingDexDirty)
            {
                screen.refreshLivingDex();
            }

            const bool isRetroDS = (g_themeMode == ThemeMode::RetroDS);

            // Screen Background
            if (isRetroDS)
                framebuffer.drawFilledRect(0, 0, 1280, 720, Color(0x0C, 0x10, 0x1F));
            else
                framebuffer.drawFilledRect(0, 0, 1280, 720, Colors::Background);

            // =========================================================================
            // 1. TOP HEADER STRIP
            // =========================================================================
            const int hX = 32, hY = 14, hW = 1216, hH = 46;
            const char *headerTitle = screen.livingDexShinyMode ? "SHINY LIVING DEX TRACKER" : "LIVING DEX & COLLECTION TRACKER";

            if (isRetroDS)
            {
                framebuffer.drawRetroDSHeaderStrip(hX, hY, hW, hH, headerTitle);
            }
            else
            {
                framebuffer.drawFilledRoundedRect(hX, hY, hW, hH, 12, Colors::Panel);
                framebuffer.drawRoundedRect(hX, hY, hW, hH, 12, Colors::Border, 1);
                framebuffer.drawText(hX + 18, hY + 12, headerTitle, Colors::Text, TextStyle::Heading);
            }

            // Right header stats
            const int curOwned = screen.livingDexShinyMode ? screen.livingDexProgress.shinyCount : screen.livingDexProgress.ownedCount;
            const float pct = (curOwned * 100.0f) / Pokemon::LIVING_DEX_MAX_SPECIES;
            char statBuf[128];
            std::snprintf(statBuf, sizeof(statBuf), "Nat: %d/%d (%.1f%%)  |  Save: %d  |  Bank: %d  |  Shiny: %d",
                          curOwned, Pokemon::LIVING_DEX_MAX_SPECIES, pct,
                          screen.livingDexProgress.saveOwnedCount,
                          screen.livingDexProgress.bankOwnedCount,
                          screen.livingDexProgress.shinyCount);
            int statW, statH;
            framebuffer.measureText(statBuf, statW, statH, TextStyle::Caption);
            framebuffer.drawText(hX + hW - statW - 16, hY + (hH - statH) / 2, statBuf,
                                 isRetroDS ? Color(0xF4, 0xF8, 0xFF) : Colors::TextDim, TextStyle::Caption);

            // =========================================================================
            // 2. SCOPE TABS & MODE TOGGLES ROW
            // =========================================================================
            const int tabY = 66, tabH = 34;

            struct ScopeTab
            {
                const char *label;
                int scope;
                int width;
            };

            const ScopeTab tabs[11] = {
                {"ALL", -1, 56},
                {"Game", 0, 68},
                {"Gen I", 1, 62},
                {"Gen II", 2, 66},
                {"Gen III", 3, 72},
                {"Gen IV", 4, 70},
                {"Gen V", 5, 66},
                {"Gen VI", 6, 70},
                {"Gen VII", 7, 74},
                {"Gen VIII", 8, 78},
                {"Gen IX", 9, 70},
            };

            int curTabX = hX;
            for (size_t t = 0; t < 11; ++t)
            {
                const auto &tab = tabs[t];
                const bool selected = (screen.livingDexScope == tab.scope);

                if (isRetroDS)
                {
                    framebuffer.drawRetroDSButton(curTabX, tabY, tab.width, tabH, tab.label, selected);
                }
                else
                {
                    framebuffer.drawPill(curTabX, tabY, tab.width, tabH, selected ? Colors::Primary : Colors::PanelAlt);
                    int tw, th;
                    framebuffer.measureText(tab.label, tw, th, TextStyle::Caption);
                    framebuffer.drawText(curTabX + (tab.width - tw) / 2, tabY + (tabH - th) / 2, tab.label,
                                         selected ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
                }
                screen.touchButtons.push_back({3000 + static_cast<int>(t), curTabX, tabY, tab.width, tabH});
                curTabX += tab.width + 6;
            }

            // Right side toggle buttons: [Y] Missing Only and [-] Shiny Mode
            const int toggleH = tabH;
            const int shinyW = 120;
            const int shinyX = hX + hW - shinyW;
            const int missingW = 136;
            const int missingX = shinyX - missingW - 8;

            // Missing Only button
            const bool missActive = screen.livingDexMissingOnly;
            if (isRetroDS)
            {
                framebuffer.drawRetroDSButton(missingX, tabY, missingW, toggleH, missActive ? "MISSING ONLY" : "[Y] Missing", missActive);
            }
            else
            {
                framebuffer.drawPill(missingX, tabY, missingW, toggleH, missActive ? Colors::Warning : Colors::PanelAlt);
                int mw, mh;
                const char *lbl = missActive ? "Missing Only" : "[Y] Missing";
                framebuffer.measureText(lbl, mw, mh, TextStyle::Caption);
                framebuffer.drawText(missingX + (missingW - mw) / 2, tabY + (toggleH - mh) / 2, lbl,
                                     missActive ? Colors::Black : Colors::Text, TextStyle::Caption);
            }
            screen.touchButtons.push_back({3020, missingX, tabY, missingW, toggleH});

            // Shiny Mode button
            const bool shinyActive = screen.livingDexShinyMode;
            if (isRetroDS)
            {
                framebuffer.drawRetroDSButton(shinyX, tabY, shinyW, toggleH, shinyActive ? "SHINY DEX" : "[-] Shiny", shinyActive);
            }
            else
            {
                framebuffer.drawPill(shinyX, tabY, shinyW, toggleH, shinyActive ? Color(255, 215, 0) : Colors::PanelAlt);
                int sw, sh;
                const char *lbl = shinyActive ? "★ Shiny Dex" : "[-] Shiny";
                framebuffer.measureText(lbl, sw, sh, TextStyle::Caption);
                framebuffer.drawText(shinyX + (shinyW - sw) / 2, tabY + (toggleH - sh) / 2, lbl,
                                     shinyActive ? Colors::Black : Colors::Text, TextStyle::Caption);
            }
            screen.touchButtons.push_back({3021, shinyX, tabY, shinyW, toggleH});

            // =========================================================================
            // 3. 30-SLOT GRID (6 columns x 5 rows)
            // =========================================================================
            const int gridX = 32, gridY = 106, gridW = 1216, gridH = 440;
            const int cols = 6, rows = 5;
            const int gapX = 8, gapY = 8;
            const int cardW = (gridW - (cols - 1) * gapX) / cols; // 196
            const int cardH = (gridH - (rows - 1) * gapY) / rows; // 81

            const int totalFiltered = static_cast<int>(screen.livingDexFilteredIds.size());
            const int pageSize = cols * rows;
            const int pageCount = std::max(1, (totalFiltered + pageSize - 1) / pageSize);
            screen.livingDexPage = std::clamp(screen.livingDexPage, 0, pageCount - 1);

            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    const int slotIdx = r * cols + c;
                    const int cardX = gridX + c * (cardW + gapX);
                    const int cardY = gridY + r * (cardH + gapY);
                    const int filteredIdx = screen.livingDexPage * pageSize + slotIdx;

                    if (filteredIdx >= totalFiltered)
                    {
                        // Empty slot beyond filtered list
                        if (isRetroDS)
                            framebuffer.drawRetroDSPanel(cardX, cardY, cardW, cardH, 4, Color(0x10, 0x14, 0x22));
                        else
                        {
                            framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 6, Colors::PanelAlt);
                            framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 6, Colors::Border, 1);
                        }
                        continue;
                    }

                    const uint16_t speciesId = screen.livingDexFilteredIds[filteredIdx];
                    const auto &rec = screen.livingDexRecords[speciesId];
                    const bool isOwned = screen.livingDexShinyMode ? rec.shiny : rec.owned;
                    const bool isFocused = (slotIdx == screen.livingDexCursor);

                    // Card Background
                    if (isRetroDS)
                    {
                        Color panelColor = isOwned ? Color(0x1B, 0x24, 0x47) : Color(0x10, 0x14, 0x24);
                        framebuffer.drawRetroDSPanel(cardX, cardY, cardW, cardH, 4, panelColor);
                    }
                    else
                    {
                        Color bg = isOwned ? (g_themeMode == ThemeMode::Light ? Color(240, 244, 255) : Color(34, 40, 58))
                                           : (g_themeMode == ThemeMode::Light ? Color(230, 230, 235) : Color(20, 22, 30));
                        framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 6, bg);
                        framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 6, isOwned ? Colors::AccentDim : Colors::Border, 1);
                    }

                    // Left Side: Pokémon Icon Sprite
                    const int sprSize = 52;
                    const int sprX = cardX + 6;
                    const int sprY = cardY + (cardH - sprSize) / 2;
                    Sprite *spr = SpriteManager::getIconSprite(speciesId, 0, screen.livingDexShinyMode || rec.shiny);
                    if (spr && spr->data)
                    {
                        if (isOwned)
                        {
                            framebuffer.drawImageScaled(sprX, sprY, spr->width, spr->height, sprSize, sprSize, spr->data, spr->channels, 255);
                        }
                        else
                        {
                            // Missing / unowned: darkened silhouette (40 alpha)
                            framebuffer.drawImageScaled(sprX, sprY, spr->width, spr->height, sprSize, sprSize, spr->data, spr->channels, 45);
                        }
                    }

                    // Right Side: Dex Number & Species Name
                    char numBuf[16];
                    std::snprintf(numBuf, sizeof(numBuf), "#%03d", speciesId);
                    framebuffer.drawText(cardX + 62, cardY + 8, numBuf, Colors::TextDim, TextStyle::Caption);

                    const char *spName = Trainer::getSpeciesName(speciesId);
                    framebuffer.drawText(cardX + 62, cardY + 26, spName, isOwned ? Colors::Text : Colors::TextDim, TextStyle::Body);

                    // Badges at bottom right / bottom
                    if (rec.owned)
                    {
                        // Poké Ball badge
                        drawMiniPokeBall(framebuffer, cardX + cardW - 14, cardY + cardH - 14, 7);

                        // Storage origin indicator pill
                        if (rec.ownedInSave && rec.ownedInBank)
                        {
                            framebuffer.drawPill(cardX + 62, cardY + 52, 48, 18, Color(0x3B, 0x59, 0x98));
                            framebuffer.drawText(cardX + 67, cardY + 54, "BOTH", Colors::White, TextStyle::Caption);
                        }
                        else if (rec.ownedInSave)
                        {
                            framebuffer.drawPill(cardX + 62, cardY + 52, 46, 18, Color(0x2E, 0x7D, 0x32));
                            framebuffer.drawText(cardX + 68, cardY + 54, "SAVE", Colors::White, TextStyle::Caption);
                        }
                        else if (rec.ownedInBank)
                        {
                            framebuffer.drawPill(cardX + 62, cardY + 52, 46, 18, Color(0x5E, 0x35, 0xB1));
                            framebuffer.drawText(cardX + 68, cardY + 54, "BANK", Colors::White, TextStyle::Caption);
                        }
                    }
                    else
                    {
                        framebuffer.drawText(cardX + 62, cardY + 54, "Missing", Color(120, 120, 140), TextStyle::Caption);
                    }

                    if (rec.shiny)
                    {
                        framebuffer.drawText(cardX + cardW - 28, cardY + 8, "★", Color(255, 215, 0), TextStyle::Caption);
                    }

                    // Focus Selection Highlighting
                    if (isFocused)
                    {
                        if (isRetroDS)
                        {
                            framebuffer.drawCornerBrackets(cardX - 1, cardY - 1, cardW + 2, cardH + 2, 7, 2,
                                                           Color(0xF4, 0xF8, 0xFF, 0xEE));
                        }
                        else
                        {
                            framebuffer.drawRoundedRect(cardX - 2, cardY - 2, cardW + 4, cardH + 4, 8, Colors::Primary, 2);
                        }
                    }

                    screen.touchButtons.push_back({3100 + slotIdx, cardX, cardY, cardW, cardH});
                }
            }

            // =========================================================================
            // 4. CURSOR PREVIEW & TYPE BANNER
            // =========================================================================
            const int bX = 32, bY = 554, bW = 1216, bH = 88;
            if (isRetroDS)
            {
                framebuffer.drawRetroDSPanel(bX, bY, bW, bH, 8, Color(0x18, 0x20, 0x40));
            }
            else
            {
                framebuffer.drawFilledRoundedRect(bX, bY, bW, bH, 10, Colors::Panel);
                framebuffer.drawRoundedRect(bX, bY, bW, bH, 10, Colors::Border, 1);
            }

            const int curFilteredIdx = screen.livingDexPage * pageSize + screen.livingDexCursor;
            if (curFilteredIdx >= 0 && curFilteredIdx < totalFiltered)
            {
                const uint16_t curSpecies = screen.livingDexFilteredIds[curFilteredIdx];
                const auto &rec = screen.livingDexRecords[curSpecies];
                const bool curOwned = screen.livingDexShinyMode ? rec.shiny : rec.owned;

                // Left Sprite Icon (large)
                const int pSprSize = 64;
                Sprite *curSpr = SpriteManager::getIconSprite(curSpecies, 0, screen.livingDexShinyMode || rec.shiny);
                if (curSpr && curSpr->data)
                {
                    framebuffer.drawImageScaled(bX + 16, bY + (bH - pSprSize) / 2, curSpr->width, curSpr->height,
                                                pSprSize, pSprSize, curSpr->data, curSpr->channels, curOwned ? 255 : 60);
                }

                // Dex ID & Name
                char titleBuf[64];
                std::snprintf(titleBuf, sizeof(titleBuf), "#%03d %s", curSpecies, Trainer::getSpeciesName(curSpecies));
                framebuffer.drawText(bX + 92, bY + 12, titleBuf, Colors::Text, TextStyle::Heading);

                // Generation Tag
                const int gen = Pokemon::getSpeciesGeneration(curSpecies);
                framebuffer.drawText(bX + 92, bY + 38, getGenName(gen), Colors::TextDim, TextStyle::Caption);

                // Dual Type Pills
                const auto &pInfo = Pokemon::getPersonalInfo(curSpecies, 0);
                int typeX = bX + 340;
                typeX += framebuffer.drawTypePill(typeX, bY + 14, 82, 24, pInfo.type1) + 8;
                if (pInfo.type2 != 255 && pInfo.type2 != pInfo.type1)
                {
                    framebuffer.drawTypePill(typeX, bY + 14, 82, 24, pInfo.type2);
                }

                // Ownership / Location Summary Text
                if (rec.owned)
                {
                    std::string locSummary = "Owned: " + std::to_string(rec.locations.size()) + " copies (";
                    if (rec.ownedInSave && rec.ownedInBank)
                        locSummary += "Save + Bank";
                    else if (rec.ownedInSave)
                        locSummary += "Save only";
                    else
                        locSummary += "Bank only";
                    locSummary += ") · Press [A] to view exact box locations";

                    framebuffer.drawText(bX + 92, bY + 60, locSummary,
                                         isRetroDS ? Color(0x9E, 0xDC, 0xFF) : Colors::Primary, TextStyle::Caption);
                }
                else
                {
                    framebuffer.drawText(bX + 92, bY + 60,
                                         "Missing from your collection — Press [A] for details",
                                         Color(220, 100, 100), TextStyle::Caption);
                }
            }

            // =========================================================================
            // 5. FOOTER INSTRUCTIONS BAR
            // =========================================================================
            const int ftY = 652, ftH = 46;
            if (isRetroDS)
            {
                framebuffer.drawRetroDSPanel(hX, ftY, hW, ftH, 6, Color(0x14, 0x1B, 0x36));
            }
            else
            {
                framebuffer.drawFilledRoundedRect(hX, ftY, hW, ftH, 10, Colors::Panel);
                framebuffer.drawRoundedRect(hX, ftY, hW, ftH, 10, Colors::Border, 1);
            }

            const char *inst = "D-Pad: Move | A: Location Details | L/R: Page | ZL/ZR: ±5 | X: Scope | Y: Missing | -: Shiny | B: Back";
            int iw, ih;
            framebuffer.measureText(inst, iw, ih, TextStyle::Caption);
            framebuffer.drawText(hX + 20, ftY + (ftH - ih) / 2, inst, Colors::TextDim, TextStyle::Caption);

            // Page info on right
            char pageBuf[64];
            int firstOnPage = totalFiltered > 0 ? (screen.livingDexPage * pageSize + 1) : 0;
            int lastOnPage = std::min(totalFiltered, (screen.livingDexPage + 1) * pageSize);
            std::snprintf(pageBuf, sizeof(pageBuf), "Page %d / %d  (%d-%d of %d)",
                          screen.livingDexPage + 1, pageCount, firstOnPage, lastOnPage, totalFiltered);
            int pw, ph;
            framebuffer.measureText(pageBuf, pw, ph, TextStyle::Caption);
            framebuffer.drawText(hX + hW - pw - 20, ftY + (ftH - ph) / 2, pageBuf,
                                 isRetroDS ? Color(0xF4, 0xF8, 0xFF) : Colors::Text, TextStyle::Caption);

            // =========================================================================
            // 6. LOCATION INSPECTOR MODAL (When active)
            // =========================================================================
            if (screen.livingDexModalActive && screen.livingDexModalSpecies > 0 &&
                screen.livingDexModalSpecies <= Pokemon::LIVING_DEX_MAX_SPECIES)
            {
                const uint16_t modalSpecies = screen.livingDexModalSpecies;
                const auto &rec = screen.livingDexRecords[modalSpecies];

                // Modal backdrop scrim
                framebuffer.drawFilledRect(0, 0, 1280, 720, Color(0, 0, 0, 180));

                const int mX = 220, mY = 110, mW = 840, mH = 500;
                char mTitle[96];
                std::snprintf(mTitle, sizeof(mTitle), "#%03d %s — Collection Details", modalSpecies,
                              Trainer::getSpeciesName(modalSpecies));

                if (isRetroDS)
                {
                    framebuffer.drawRetroDSPanel(mX, mY, mW, mH, 10, Color(0x1B, 0x24, 0x47));
                    framebuffer.drawRetroDSHeaderStrip(mX, mY, mW, 42, mTitle);
                }
                else
                {
                    framebuffer.drawSoftShadow(mX, mY, mW, mH, 16);
                    framebuffer.drawFilledRoundedRect(mX, mY, mW, mH, 12, Colors::Panel);
                    framebuffer.drawRoundedRect(mX, mY, mW, mH, 12, Colors::Border, 1);
                    framebuffer.drawFilledRoundedRect(mX, mY, mW, 42, 12, Colors::PanelAlt);
                    framebuffer.drawText(mX + 20, mY + 12, mTitle, Colors::Text, TextStyle::Heading);
                }

                // Left Column: Pokémon Portrait & Stats (W = 240)
                const int leftColX = mX + 24, leftColY = mY + 58, leftColW = 240;
                const int pLargeSize = 88;
                Sprite *lSpr = SpriteManager::getIconSprite(modalSpecies, 0, rec.shiny);
                if (lSpr && lSpr->data)
                {
                    framebuffer.drawImageScaled(leftColX + (leftColW - pLargeSize) / 2, leftColY,
                                                lSpr->width, lSpr->height, pLargeSize, pLargeSize,
                                                lSpr->data, lSpr->channels, rec.owned ? 255 : 60);
                }

                const auto &pInfo = Pokemon::getPersonalInfo(modalSpecies, 0);
                int tY = leftColY + pLargeSize + 14;
                framebuffer.drawTypePill(leftColX + 20, tY, 90, 26, pInfo.type1);
                if (pInfo.type2 != 255 && pInfo.type2 != pInfo.type1)
                {
                    framebuffer.drawTypePill(leftColX + 120, tY, 90, 26, pInfo.type2);
                }

                const int gen = Pokemon::getSpeciesGeneration(modalSpecies);
                framebuffer.drawText(leftColX + 20, tY + 38, getGenName(gen), Colors::TextDim, TextStyle::Caption);

                char copyCountBuf[48];
                std::snprintf(copyCountBuf, sizeof(copyCountBuf), "Total Copies: %zu", rec.locations.size());
                framebuffer.drawText(leftColX + 20, tY + 62, copyCountBuf,
                                     rec.owned ? Colors::Primary : Color(220, 100, 100), TextStyle::Body);

                // Right Column: Exact Storage Locations List (W = 530)
                const int rightColX = mX + 280, rightColY = mY + 58, rightColW = mW - 304;

                if (rec.locations.empty())
                {
                    // Unowned message
                    if (isRetroDS)
                        framebuffer.drawRetroDSPanel(rightColX, rightColY, rightColW, 140, 6, Color(0x14, 0x1B, 0x36));
                    else
                    {
                        framebuffer.drawFilledRoundedRect(rightColX, rightColY, rightColW, 140, 8, Colors::PanelAlt);
                        framebuffer.drawRoundedRect(rightColX, rightColY, rightColW, 140, 8, Colors::Border, 1);
                    }
                    framebuffer.drawText(rightColX + 24, rightColY + 24, "Missing from your collection", Colors::Text, TextStyle::Heading);
                    framebuffer.drawText(rightColX + 24, rightColY + 56,
                                         "No copies of this Pokémon were found in your save file", Colors::TextDim, TextStyle::Body);
                    framebuffer.drawText(rightColX + 24, rightColY + 80,
                                         "or the unified PKSE Bank.", Colors::TextDim, TextStyle::Body);
                }
                else
                {
                    // Render up to 5 location rows
                    int locRowY = rightColY;
                    const int rowH = 64;
                    for (size_t i = 0; i < rec.locations.size() && i < 5; ++i)
                    {
                        const auto &loc = rec.locations[i];
                        if (isRetroDS)
                        {
                            framebuffer.drawRetroDSPanel(rightColX, locRowY, rightColW, rowH, 4, Color(0x14, 0x1E, 0x3C));
                        }
                        else
                        {
                            framebuffer.drawFilledRoundedRect(rightColX, locRowY, rightColW, rowH, 6, Colors::PanelAlt);
                            framebuffer.drawRoundedRect(rightColX, locRowY, rightColW, rowH, 6, Colors::Border, 1);
                        }

                        // Storage Tag
                        if (loc.inBank)
                        {
                            framebuffer.drawPill(rightColX + 12, locRowY + 14, 60, 22, Color(0x5E, 0x35, 0xB1));
                            framebuffer.drawText(rightColX + 20, locRowY + 16, "BANK", Colors::White, TextStyle::Caption);
                        }
                        else
                        {
                            framebuffer.drawPill(rightColX + 12, locRowY + 14, 60, 22, Color(0x2E, 0x7D, 0x32));
                            framebuffer.drawText(rightColX + 20, locRowY + 16, "SAVE", Colors::White, TextStyle::Caption);
                        }

                        // Box & Slot Info
                        char locText[96];
                        if (loc.box < 0)
                            std::snprintf(locText, sizeof(locText), "Party (Slot %d)", loc.slot + 1);
                        else
                            std::snprintf(locText, sizeof(locText), "%s · Slot %d", loc.boxName.c_str(), loc.slot + 1);
                        framebuffer.drawText(rightColX + 82, locRowY + 14, locText, Colors::Text, TextStyle::Body);

                        // Level & Nickname & Shiny status
                        char detailText[128];
                        std::string nick = loc.nickname.empty() ? "" : (" \"" + loc.nickname + "\"");
                        std::string shinyStr = loc.isShiny ? " ★ Shiny" : "";
                        std::snprintf(detailText, sizeof(detailText), "Lv. %d%s%s", loc.level, nick.c_str(), shinyStr.c_str());
                        framebuffer.drawText(rightColX + 82, locRowY + 38, detailText,
                                             loc.isShiny ? Color(255, 215, 0) : Colors::TextDim, TextStyle::Caption);

                        locRowY += rowH + 8;
                    }

                    if (rec.locations.size() > 5)
                    {
                        char moreBuf[64];
                        std::snprintf(moreBuf, sizeof(moreBuf), "+ %zu more copies in storage", rec.locations.size() - 5);
                        framebuffer.drawText(rightColX + 12, locRowY + 4, moreBuf, Colors::TextDim, TextStyle::Caption);
                    }
                }

                // Modal Close Button at bottom right
                const int btnW = 140, btnH = 38;
                const int btnX = mX + mW - btnW - 24, btnY = mY + mH - btnH - 16;
                if (isRetroDS)
                {
                    framebuffer.drawRetroDSButton(btnX, btnY, btnW, btnH, "CLOSE [B]", true);
                }
                else
                {
                    framebuffer.drawPill(btnX, btnY, btnW, btnH, Colors::Primary);
                    int bw, bh;
                    framebuffer.measureText("Close [B]", bw, bh, TextStyle::Body);
                    framebuffer.drawText(btnX + (btnW - bw) / 2, btnY + (btnH - bh) / 2, "Close [B]", Colors::PrimaryText, TextStyle::Body);
                }
                screen.touchButtons.push_back({3999, btnX, btnY, btnW, btnH});
            }
        }
    }
}
