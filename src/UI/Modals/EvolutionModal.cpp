#include "UI/Modals/EvolutionModal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/Evolution.h"
#include "Trainer/Trainer.h"
#include "Utils/HelperUtilities.h"

namespace UI
{
    namespace Modals
    {
        void drawEvolutionModal(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            const Pokemon::Pokemon *pokemon = screen.detailsTargetPokemon();
            if (!pokemon || pokemon->speciesID() == 0)
                return;

            const Pokemon::EvolutionInfo evoInfo = Pokemon::getEvolutionInfo(*pokemon);
            const int screenW = framebuffer.getWidth();
            const int screenH = framebuffer.getHeight();

            // Dim background
            framebuffer.drawFilledRect(0, 0, screenW, screenH, Color(0, 0, 0, 165));

            // Card window dimensions
            const int cardW = 760;
            const int cardH = 580;
            const int cardX = (screenW - cardW) / 2;
            const int cardY = (screenH - cardH) / 2;

            // Panel frame
            const Color accentColor = (g_themeMode == ThemeMode::RetroDS)
                                          ? Color(0xE8, 0x38, 0x48)
                                          : Colors::Accent;
            if (g_themeMode == ThemeMode::RetroDS)
            {
                framebuffer.drawRetroDSPanel(cardX, cardY, cardW, cardH, 16, Colors::Panel);
            }
            else
            {
                framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Panel);
                framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Border, 1);
            }

            screen.touchButtons.clear();

            // Top Header
            const char *title = evoInfo.hasEvolutions() ? "EVOLUTION" : "EVOLUTION LINEAGE";
            framebuffer.drawText(cardX + 28, cardY + 16, title, accentColor, TextStyle::Heading);

            int titleW = 0, titleH = 0;
            framebuffer.measureText(title, titleW, titleH, TextStyle::Heading);

            std::string subTitle = "No. " + UI::dexNumberLabel(pokemon->speciesID()) + " · " + evoInfo.currentSpeciesName;
            framebuffer.drawText(cardX + 28 + titleW + 16, cardY + 22, subTitle, Colors::TextDim, TextStyle::Caption);

            // Close button (×)
            const int closeX = cardX + cardW - 36;
            const int closeY = cardY + 28;
            framebuffer.drawFilledCircle(closeX, closeY, 18, Colors::PanelAlt);
            framebuffer.drawCircle(closeX, closeY, 18, Colors::Border, 1);
            framebuffer.drawText(closeX - 6, closeY - 11, "\xC3\x97", Colors::Text, TextStyle::Heading);
            screen.touchButtons.push_back({99, closeX - 22, closeY - 22, 44, 44});

            // Header separator
            framebuffer.drawFilledRect(cardX + 16, cardY + 52, cardW - 32, 2, Colors::Border);

            const float nowT = static_cast<float>(framebuffer.getTimeSeconds());
            int contentY = cardY + 62;

            if (evoInfo.hasEvolutions())
            {
                const int branchCount = static_cast<int>(evoInfo.candidates.size());
                int branchIndex = screen.evolutionModalBranchIndex;
                if (branchIndex < 0) branchIndex = 0;
                if (branchIndex >= branchCount) branchIndex = branchCount - 1;

                // Branch chips (if multiple evolutions exist, e.g. Eevee, Applin, Tyrogue, Kirlia)
                if (branchCount > 1)
                {
                    const int chipH = 32;
                    int totalChipsW = 0;
                    std::vector<int> chipWidths;
                    chipWidths.reserve(branchCount);

                    for (int i = 0; i < branchCount; ++i)
                    {
                        int tw = 0, th = 0;
                        framebuffer.measureText(evoInfo.candidates[i].speciesName, tw, th, TextStyle::Body);
                        int cw = tw + 24;
                        chipWidths.push_back(cw);
                        totalChipsW += cw + 6;
                    }
                    totalChipsW -= 6;

                    // Centered start or scroll offset
                    int chipX = cardX + (cardW - totalChipsW) / 2;
                    if (totalChipsW > cardW - 40)
                    {
                        // Scroll so current branch is centered
                        int currentMid = 0;
                        for (int i = 0; i < branchIndex; ++i)
                            currentMid += chipWidths[i] + 6;
                        currentMid += chipWidths[branchIndex] / 2;
                        chipX = (cardX + cardW / 2) - currentMid;
                    }

                    for (int i = 0; i < branchCount; ++i)
                    {
                        const int cw = chipWidths[i];
                        const bool isSelected = (i == branchIndex);

                        if (chipX + cw > cardX + 10 && chipX < cardX + cardW - 10)
                        {
                            if (isSelected)
                            {
                                framebuffer.drawFilledRoundedRect(chipX, contentY, cw, chipH, 8, Colors::Selected);
                                framebuffer.drawRoundedRect(chipX, contentY, cw, chipH, 8, accentColor, 2);
                                framebuffer.drawCornerBrackets(chipX, contentY, cw, chipH, 6, 2, accentColor,
                                                               std::sin(nowT * 5.0f) * 1.5f);
                            }
                            else
                            {
                                framebuffer.drawFilledRoundedRect(chipX, contentY, cw, chipH, 8, Colors::PanelAlt);
                                framebuffer.drawRoundedRect(chipX, contentY, cw, chipH, 8, Colors::Border, 1);
                            }

                            int tw = 0, th = 0;
                            framebuffer.measureText(evoInfo.candidates[i].speciesName, tw, th, TextStyle::Body);
                            framebuffer.drawText(chipX + (cw - tw) / 2, contentY + 6,
                                                 evoInfo.candidates[i].speciesName,
                                                 isSelected ? Colors::Text : Colors::TextDim, TextStyle::Body);

                            screen.touchButtons.push_back({1000 + i, chipX, contentY, cw, chipH});
                        }
                        chipX += cw + 6;
                    }
                    contentY += chipH + 12;
                }

                const Pokemon::EvolutionCandidate &cand = evoInfo.candidates[branchIndex];

                // Stage: Dual Pedestals & Evolution Arrow
                const int stageH = 138;
                const int stageW = cardW - 40;
                const int stageX = cardX + 20;
                const int stageY = contentY;

                framebuffer.drawFilledRoundedRect(stageX, stageY, stageW, stageH, 12, Colors::PanelAlt);
                framebuffer.drawRoundedRect(stageX, stageY, stageW, stageH, 12, Colors::Border, 1);

                // Left Pedestal: Origin Pokémon
                const int leftCenterX = stageX + static_cast<int>(stageW * 0.22f);
                framebuffer.drawFilledEllipse(leftCenterX, stageY + 98, 48, 12, Color(0, 0, 0, 80));

                Sprite *origSprite = SpriteManager::getSprite(evoInfo.speciesId, evoInfo.formId, evoInfo.isShiny);
                if (origSprite && origSprite->data)
                {
                    constexpr int spSz = 80;
                    framebuffer.drawImageScaled(leftCenterX - spSz / 2, stageY + 12,
                                                origSprite->width, origSprite->height,
                                                spSz, spSz, origSprite->data, origSprite->channels);
                }

                // Left labels
                int nameW = 0, nameH = 0;
                framebuffer.measureText(evoInfo.currentNickname, nameW, nameH, TextStyle::Body);
                framebuffer.drawText(leftCenterX - nameW / 2, stageY + 98, evoInfo.currentNickname, Colors::Text, TextStyle::Body);

                std::string lvlStr = "Lv. " + std::to_string(evoInfo.currentLevel);
                int lvlW = 0, lvlH = 0;
                framebuffer.measureText(lvlStr, lvlW, lvlH, TextStyle::Caption);
                framebuffer.drawText(leftCenterX - lvlW / 2, stageY + 116, lvlStr, Colors::TextDim, TextStyle::Caption);

                // Center: Animated Evolution Chevrons
                const int midCenterX = stageX + stageW / 2;
                const int midCenterY = stageY + 52;
                const Color beatColor = cand.isTrade
                                            ? Color(0x00, 0xD2, 0xD3)
                                            : (cand.conditionMet ? Color(0x2E, 0xCC, 0x71) : accentColor);

                // Draw 3 flow chevrons
                const float pulseAnim = std::fmod(nowT * 2.5f, 3.0f);
                for (int c = 0; c < 3; ++c)
                {
                    const float offset = (c - 1) * 20.0f;
                    float alphaFrac = 0.4f + 0.6f * std::max(0.0f, 1.0f - std::abs(pulseAnim - c));
                    Color cCol = beatColor;
                    cCol.alpha = static_cast<uint8_t>(255 * alphaFrac);
                    framebuffer.drawText(midCenterX + static_cast<int>(offset) - 6, midCenterY - 10,
                                         "\xE2\x9D\xAF", cCol, TextStyle::Heading); // ❯
                }

                int labelW = 0, labelH = 0;
                framebuffer.measureText("EVOLVES TO", labelW, labelH, TextStyle::Caption);
                framebuffer.drawText(midCenterX - labelW / 2, midCenterY + 16, "EVOLVES TO", Colors::TextDim, TextStyle::Caption);

                // Right Pedestal: Destination Pokémon with soft glow
                const int rightCenterX = stageX + static_cast<int>(stageW * 0.78f);
                // Radial glow backing
                Color glowCol = beatColor;
                glowCol.alpha = 45;
                framebuffer.drawFilledCircle(rightCenterX, stageY + 52, 46, glowCol);
                framebuffer.drawFilledEllipse(rightCenterX, stageY + 98, 48, 12, Color(0, 0, 0, 80));

                Sprite *destSprite = SpriteManager::getSprite(cand.destinationSpeciesId, cand.destinationFormId, evoInfo.isShiny);
                if (destSprite && destSprite->data)
                {
                    constexpr int spSz = 80;
                    framebuffer.drawImageScaled(rightCenterX - spSz / 2, stageY + 12,
                                                destSprite->width, destSprite->height,
                                                spSz, spSz, destSprite->data, destSprite->channels);
                }

                // Right labels
                int destNameW = 0, destNameH = 0;
                framebuffer.measureText(cand.speciesName, destNameW, destNameH, TextStyle::Body);
                framebuffer.drawText(rightCenterX - destNameW / 2, stageY + 98, cand.speciesName, accentColor, TextStyle::Body);

                std::string destDex = "No. " + UI::dexNumberLabel(cand.destinationSpeciesId);
                int dexW = 0, dexH = 0;
                framebuffer.measureText(destDex, dexW, dexH, TextStyle::Caption);
                framebuffer.drawText(rightCenterX - dexW / 2, stageY + 116, destDex, Colors::TextDim, TextStyle::Caption);

                contentY += stageH + 10;

                // Requirement Card Banner
                const int reqH = 42;
                const int reqW = cardW - 40;
                const int reqX = cardX + 20;
                const int reqY = contentY;

                framebuffer.drawFilledRoundedRect(reqX, reqY, reqW, reqH, 10, Colors::Panel);
                framebuffer.drawRoundedRect(reqX, reqY, reqW, reqH, 10, Colors::Border, 1);

                // Left: requirement text
                framebuffer.drawText(reqX + 16, reqY + 11, "Condition:  " + cand.requirement, Colors::Text, TextStyle::Body);

                // Right: status chip
                const Color statusTone = cand.isTrade
                                             ? Color(0x00, 0xD2, 0xD3)
                                             : (cand.conditionMet ? Color(0x2E, 0xCC, 0x71) : Color(0xF3, 0x9C, 0x12));
                int statTextW = 0, statTextH = 0;
                framebuffer.measureText(cand.statusText, statTextW, statTextH, TextStyle::Caption);
                const int chipBadgeW = statTextW + 20;
                const int chipBadgeX = reqX + reqW - 12 - chipBadgeW;
                framebuffer.drawFilledRoundedRect(chipBadgeX, reqY + 8, chipBadgeW, 26, 6,
                                                  Color(statusTone.red, statusTone.green, statusTone.blue, 45));
                framebuffer.drawRoundedRect(chipBadgeX, reqY + 8, chipBadgeW, 26, 6, statusTone, 1);
                framebuffer.drawText(chipBadgeX + 10, reqY + 13, cand.statusText, statusTone, TextStyle::Caption);

                contentY += reqH + 10;

                // 6-Stat Delta Comparison Grid
                const int statGridH = 76;
                const int statGridW = cardW - 40;
                const int statGridX = cardX + 20;
                const int statGridY = contentY;

                const char *statLabels[6] = {"HP", "ATK", "DEF", "SPA", "SPD", "SPE"};
                const int cellGap = 8;
                const int cellW = (statGridW - cellGap * 2) / 3;
                const int cellH = 34;

                for (int s = 0; s < 6; ++s)
                {
                    const int col = s % 3;
                    const int row = s / 3;
                    const int cx = statGridX + col * (cellW + cellGap);
                    const int cy = statGridY + row * (cellH + 8);

                    framebuffer.drawFilledRoundedRect(cx, cy, cellW, cellH, 6, Colors::PanelAlt);
                    framebuffer.drawRoundedRect(cx, cy, cellW, cellH, 6, Colors::Border, 1);

                    // Label
                    framebuffer.drawText(cx + 8, cy + 9, statLabels[s], Colors::TextDim, TextStyle::Caption);

                    // Before -> After
                    const int before = cand.statsBefore[s];
                    const int after = cand.statsAfter[s];
                    const int delta = after - before;

                    std::string progStr = std::to_string(before) + " \xE2\x9E\x94 " + std::to_string(after);
                    framebuffer.drawText(cx + 42, cy + 9, progStr, Colors::Text, TextStyle::Body);

                    // Delta badge
                    std::string deltaStr;
                    Color deltaCol = Colors::TextDim;
                    if (delta > 0)
                    {
                        deltaStr = "\xE2\x96\xB2+" + std::to_string(delta);
                        deltaCol = Color(0x2E, 0xCC, 0x71); // Green
                    }
                    else if (delta < 0)
                    {
                        deltaStr = "\xE2\x96\xBC" + std::to_string(delta);
                        deltaCol = Color(0xE7, 0x4C, 0x3C); // Red
                    }
                    else
                    {
                        deltaStr = "=";
                        deltaCol = Colors::TextDim;
                    }

                    int dw = 0, dh = 0;
                    framebuffer.measureText(deltaStr, dw, dh, TextStyle::Caption);
                    framebuffer.drawText(cx + cellW - 8 - dw, cy + 9, deltaStr, deltaCol, TextStyle::Caption);
                }

                contentY += statGridH + 10;

                // Ability & Progression Bar
                const int detailH = 36;
                const int detailW = cardW - 40;
                const int detailX = cardX + 20;
                const int detailY = contentY;

                framebuffer.drawFilledRoundedRect(detailX, detailY, detailW, detailH, 8, Colors::Panel);
                framebuffer.drawRoundedRect(detailX, detailY, detailW, detailH, 8, Colors::Border, 1);

                std::string abilityLine = "Ability: " + cand.abilityBefore;
                if (cand.abilityBefore != cand.abilityAfter && cand.abilityAfter != "—")
                    abilityLine += " \xE2\x9E\x94 " + cand.abilityAfter;
                framebuffer.drawText(detailX + 14, detailY + 9, abilityLine, Colors::Text, TextStyle::Caption);

                if (!cand.consumedItem.empty())
                {
                    std::string itemLine = "Item Consumed: " + cand.consumedItem;
                    int iw = 0, ih = 0;
                    framebuffer.measureText(itemLine, iw, ih, TextStyle::Caption);
                    framebuffer.drawText(detailX + detailW - 14 - iw, detailY + 9, itemLine, Color(0xF3, 0x9C, 0x12), TextStyle::Caption);
                }

                // Action Buttons at bottom
                const int btnY = cardY + cardH - 58;
                const int cancelW = 180;
                const int cancelX = cardX + 40;
                const int evolveW = 240;
                const int evolveX = cardX + cardW - 40 - evolveW;
                const int btnH = 44;

                // Cancel button
                if (g_themeMode == ThemeMode::RetroDS)
                {
                    framebuffer.drawRetroDSButton(cancelX, btnY, cancelW, btnH, "Cancel (B)", false, accentColor, 10);
                }
                else
                {
                    framebuffer.drawFilledRoundedRect(cancelX, btnY, cancelW, btnH, 10, Colors::PanelAlt);
                    framebuffer.drawRoundedRect(cancelX, btnY, cancelW, btnH, 10, Colors::Border, 1);
                    int cw = 0, ch = 0;
                    framebuffer.measureText("Cancel (B)", cw, ch, TextStyle::Body);
                    framebuffer.drawText(cancelX + (cancelW - cw) / 2, btnY + 12, "Cancel (B)", Colors::Text, TextStyle::Body);
                }
                screen.touchButtons.push_back({200, cancelX, btnY, cancelW, btnH});

                // Evolve button
                if (g_themeMode == ThemeMode::RetroDS)
                {
                    const char *evolveLabel = cand.isTrade ? "Trade Evolve (A)" : "Evolve (A)";
                    framebuffer.drawRetroDSButton(evolveX, btnY, evolveW, btnH, nullptr, true,
                                                  cand.isTrade ? Color(0x00, 0xD2, 0xD3) : accentColor, 10);
                    int ew = 0, eh = 0;
                    framebuffer.measureText(evolveLabel, ew, eh, TextStyle::Body);
                    const int evoIconSz = 18;
                    const int evoContentW = evoIconSz + 8 + ew;
                    const int evoContentX = evolveX + (evolveW - evoContentW) / 2;
                    framebuffer.drawEvolveArrow(evoContentX, btnY + (btnH - evoIconSz) / 2, evoIconSz, Color(0xF4, 0xF8, 0xFF));
                    framebuffer.drawText(evoContentX + evoIconSz + 8, btnY + 12, evolveLabel, Color(0xF4, 0xF8, 0xFF), TextStyle::Body);
                }
                else
                {
                    const Color evolveBg = cand.isTrade
                                               ? Color(0x00, 0x9B, 0x9E)
                                               : (cand.conditionMet ? Colors::Evolve : Colors::Selected);
                    framebuffer.drawFilledRoundedRect(evolveX, btnY, evolveW, btnH, 10, evolveBg);
                    framebuffer.drawRoundedRect(evolveX, btnY, evolveW, btnH, 10, accentColor, 2);
                    framebuffer.drawCornerBrackets(evolveX, btnY, evolveW, btnH, 8, 2, accentColor,
                                                   std::sin(nowT * 5.0f) * 1.5f);

                    const char *evolveLabel = cand.isTrade ? "Trade Evolve (A)" : "Evolve (A)";
                    int ew = 0, eh = 0;
                    framebuffer.measureText(evolveLabel, ew, eh, TextStyle::Body);
                    const int evoIconSz = 18;
                    const int evoContentW = evoIconSz + 8 + ew;
                    const int evoContentX = evolveX + (evolveW - evoContentW) / 2;

                    framebuffer.drawEvolveArrow(evoContentX, btnY + (btnH - evoIconSz) / 2, evoIconSz, Colors::EvolveText);
                    framebuffer.drawText(evoContentX + evoIconSz + 8, btnY + 12, evolveLabel, Colors::EvolveText, TextStyle::Body);
                }
                screen.touchButtons.push_back({201, evolveX, btnY, evolveW, btnH});
            }
            else
            {
                // Final stage / Lineage Display
                const int chainCount = static_cast<int>(evoInfo.chain.size());
                const int stageH = 220;
                const int stageW = cardW - 40;
                const int stageX = cardX + 20;
                const int stageY = contentY + 20;

                framebuffer.drawFilledRoundedRect(stageX, stageY, stageW, stageH, 12, Colors::PanelAlt);
                framebuffer.drawRoundedRect(stageX, stageY, stageW, stageH, 12, Colors::Border, 1);

                if (chainCount > 0)
                {
                    const int stepW = stageW / chainCount;
                    for (int i = 0; i < chainCount; ++i)
                    {
                        const auto &st = evoInfo.chain[i];
                        const int cx = stageX + i * stepW + stepW / 2;
                        const int cy = stageY + 70;

                        framebuffer.drawFilledEllipse(cx, cy + 48, 44, 11, Color(0, 0, 0, 75));

                        Sprite *sp = SpriteManager::getSprite(st.speciesId, st.formId, evoInfo.isShiny);
                        if (sp && sp->data)
                        {
                            constexpr int sz = 80;
                            framebuffer.drawImageScaled(cx - sz / 2, cy - sz / 2, sp->width, sp->height,
                                                        sz, sz, sp->data, sp->channels);
                        }

                        if (st.isCurrent)
                        {
                            framebuffer.drawCornerBrackets(cx - 46, cy - 46, 92, 92, 10, 2, accentColor,
                                                           std::sin(nowT * 5.0f) * 1.5f);
                        }

                        int nw = 0, nh = 0;
                        framebuffer.measureText(st.name, nw, nh, TextStyle::Body);
                        framebuffer.drawText(cx - nw / 2, cy + 54, st.name,
                                             st.isCurrent ? accentColor : Colors::Text, TextStyle::Body);

                        std::string dexLabel = "No. " + UI::dexNumberLabel(st.speciesId);
                        int dw = 0, dh = 0;
                        framebuffer.measureText(dexLabel, dw, dh, TextStyle::Caption);
                        framebuffer.drawText(cx - dw / 2, cy + 72, dexLabel, Colors::TextDim, TextStyle::Caption);

                        // Arrow to next stage
                        if (i < chainCount - 1)
                        {
                            const int arrX = stageX + (i + 1) * stepW - 10;
                            framebuffer.drawText(arrX, cy - 8, "\xE2\x9E\x94", Colors::TextDim, TextStyle::Heading); // ➔
                        }
                    }
                }

                // Final stage badge
                const int badgeY = stageY + stageH + 20;
                int bw = 0, bh = 0;
                const char *finalMsg = chainCount > 1
                                           ? "Fully Evolved · Final Evolution Stage"
                                           : "This Pokémon does not evolve in this game.";
                framebuffer.measureText(finalMsg, bw, bh, TextStyle::Body);
                const int badgeX = cardX + (cardW - (bw + 32)) / 2;
                framebuffer.drawFilledRoundedRect(badgeX, badgeY, bw + 32, 38, 8, Colors::Panel);
                framebuffer.drawRoundedRect(badgeX, badgeY, bw + 32, 38, 8, Colors::Border, 1);
                framebuffer.drawText(badgeX + 16, badgeY + 10, finalMsg, Colors::TextDim, TextStyle::Body);

                // Close Button
                const int btnY = cardY + cardH - 58;
                const int closeW = 200;
                const int closeBtnX = cardX + (cardW - closeW) / 2;
                if (g_themeMode == ThemeMode::RetroDS)
                {
                    framebuffer.drawRetroDSButton(closeBtnX, btnY, closeW, 44, "Close (B)", false, accentColor, 10);
                }
                else
                {
                    framebuffer.drawFilledRoundedRect(closeBtnX, btnY, closeW, 44, 10, Colors::PanelAlt);
                    framebuffer.drawRoundedRect(closeBtnX, btnY, closeW, 44, 10, Colors::Border, 1);
                    int cw = 0, ch = 0;
                    framebuffer.measureText("Close (B)", cw, ch, TextStyle::Body);
                    framebuffer.drawText(closeBtnX + (closeW - cw) / 2, btnY + 12, "Close (B)", Colors::Text, TextStyle::Body);
                }
                screen.touchButtons.push_back({200, closeBtnX, btnY, closeW, 44});
            }
        }
    }
}
