#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "UI/Panels/PokeparkPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "UI/Pokepark.h"
#include "UI/Common.h"

namespace UI
{
    namespace Panels
    {
        static void drawSky(PKSEFramebuffer &framebuffer, int x, int y, int w, int h, int hour)
        {
            // Time of day sky gradient
            Color topColor, bottomColor;
            if (hour >= 6 && hour < 11) // Morning
            {
                topColor = Color(135, 175, 230);
                bottomColor = Color(245, 215, 175);
            }
            else if (hour >= 11 && hour < 18) // Day
            {
                topColor = Color(85, 155, 235);
                bottomColor = Color(185, 225, 255);
            }
            else if (hour >= 18 && hour < 21) // Sunset
            {
                topColor = Color(110, 65, 145);
                bottomColor = Color(245, 140, 95);
            }
            else // Night
            {
                topColor = Color(18, 22, 55);
                bottomColor = Color(45, 50, 95);
            }

            // Draw sky in horizontal strips
            constexpr int strips = 16;
            const int stripH = h / strips + 1;
            for (int i = 0; i < strips; ++i)
            {
                float t = static_cast<float>(i) / static_cast<float>(strips - 1);
                uint8_t r = static_cast<uint8_t>(topColor.red * (1.0f - t) + bottomColor.red * t);
                uint8_t g = static_cast<uint8_t>(topColor.green * (1.0f - t) + bottomColor.green * t);
                uint8_t b = static_cast<uint8_t>(topColor.blue * (1.0f - t) + bottomColor.blue * t);
                framebuffer.drawFilledRect(x, y + i * (h / strips), w, stripH, Color(r, g, b));
            }

            // Sun / Moon
            if (hour >= 6 && hour < 18)
            {
                // Sun
                int sunX = x + w - 180;
                int sunY = y + 70;
                framebuffer.drawFilledCircle(sunX, sunY, 32, Color(255, 245, 190, 80));
                framebuffer.drawFilledCircle(sunX, sunY, 24, Color(255, 250, 210, 180));
                framebuffer.drawFilledCircle(sunX, sunY, 18, Color(255, 255, 240));
            }
            else
            {
                // Moon & Stars
                int moonX = x + w - 180;
                int moonY = y + 70;
                framebuffer.drawFilledCircle(moonX, moonY, 20, Color(240, 245, 255, 220));
                framebuffer.drawFilledCircle(moonX - 6, moonY - 4, 18, topColor); // crescent shadow

                // Stars
                framebuffer.drawFilledCircle(x + 120, y + 45, 2, Color(255, 255, 255, 180));
                framebuffer.drawFilledCircle(x + 280, y + 80, 2, Color(255, 255, 255, 200));
                framebuffer.drawFilledCircle(x + 450, y + 35, 3, Color(255, 255, 255, 220));
                framebuffer.drawFilledCircle(x + 620, y + 90, 2, Color(255, 255, 255, 160));
                framebuffer.drawFilledCircle(x + 820, y + 50, 3, Color(255, 255, 255, 210));
            }
        }

        static void drawHabitatScenery(PKSEFramebuffer &framebuffer, int x, int y, int w, int h, ParkEnvironmentId env)
        {
            switch (env)
            {
            case ParkEnvironmentId::Commons:
            {
                // Rolling lush green meadow hills
                framebuffer.drawFilledEllipse(x + 250, y + h + 80, 500, 260, Color(85, 175, 80));
                framebuffer.drawFilledEllipse(x + w - 280, y + h + 60, 480, 240, Color(75, 165, 70));
                framebuffer.drawFilledRect(x, y + h - 180, w, 180, Color(65, 155, 60));

                // Path
                framebuffer.drawFilledEllipse(x + w / 2, y + h + 20, 280, 110, Color(215, 195, 150));

                // Flowers (white daisies / yellow buttercups)
                framebuffer.drawFilledCircle(x + 160, y + h - 90, 4, Color(255, 255, 255));
                framebuffer.drawFilledCircle(x + 160, y + h - 90, 2, Color(255, 215, 0));
                framebuffer.drawFilledCircle(x + 320, y + h - 120, 4, Color(255, 220, 80));
                framebuffer.drawFilledCircle(x + 890, y + h - 75, 4, Color(255, 255, 255));
                framebuffer.drawFilledCircle(x + 1040, y + h - 110, 4, Color(255, 180, 200));
                break;
            }
            case ParkEnvironmentId::WaterfallLake:
            {
                // Shoreline and crystal waterfall
                framebuffer.drawFilledRect(x, y + h - 220, w, 220, Color(75, 155, 75)); // grass shore

                // Waterfall rock cliff on left
                framebuffer.drawFilledRoundedRect(x - 40, y + 40, 180, h - 120, 24, Color(100, 110, 120));
                framebuffer.drawFilledRect(x + 40, y + 90, 48, h - 170, Color(135, 205, 250, 220)); // falling water
                framebuffer.drawFilledRect(x + 50, y + 90, 28, h - 170, Color(225, 245, 255, 240));

                // Lake water area
                framebuffer.drawFilledEllipse(x + 580, y + h - 70, 480, 120, Color(65, 140, 215));
                framebuffer.drawFilledEllipse(x + 580, y + h - 70, 420, 95, Color(85, 165, 235));

                // Water ripples & foam
                framebuffer.drawFilledEllipse(x + 64, y + h - 80, 60, 20, Color(255, 255, 255, 180));
                framebuffer.drawFilledEllipse(x + 480, y + h - 60, 80, 10, Color(255, 255, 255, 130));
                framebuffer.drawFilledEllipse(x + 720, y + h - 80, 90, 12, Color(255, 255, 255, 130));
                break;
            }
            case ParkEnvironmentId::EmberGrove:
            {
                // Autumn canopy and warm basalt stones
                framebuffer.drawFilledRect(x, y + h - 200, w, 200, Color(125, 80, 55)); // earthy terrain
                framebuffer.drawFilledEllipse(x + 300, y + h - 90, 350, 120, Color(160, 95, 60));

                // Basalt stone rings
                framebuffer.drawFilledCircle(x + w / 2 - 120, y + h - 100, 36, Color(55, 50, 55));
                framebuffer.drawFilledCircle(x + w / 2 - 120, y + h - 100, 28, Color(245, 130, 45, 180)); // ember warmth
                framebuffer.drawFilledCircle(x + w / 2 + 140, y + h - 85, 42, Color(65, 60, 65));
                framebuffer.drawFilledCircle(x + w / 2 + 140, y + h - 85, 32, Color(255, 160, 60, 160));

                // Autumn foliage on borders
                framebuffer.drawFilledCircle(x + 60, y + 90, 90, Color(215, 95, 35));
                framebuffer.drawFilledCircle(x + w - 80, y + 80, 110, Color(195, 75, 30));
                break;
            }
            case ParkEnvironmentId::SkySummit:
            {
                // Alpine cliffs and fluffy cloud ocean beneath
                framebuffer.drawFilledRect(x, y + h - 190, w, 190, Color(145, 155, 165)); // stone plateau
                framebuffer.drawFilledEllipse(x + w / 2, y + h - 50, 580, 120, Color(165, 175, 185));

                // Snowcap patches
                framebuffer.drawFilledEllipse(x + 220, y + h - 110, 110, 24, Color(245, 250, 255));
                framebuffer.drawFilledEllipse(x + 890, y + h - 95, 130, 28, Color(245, 250, 255));

                // Cloud layer floating at the bottom horizon
                framebuffer.drawFilledEllipse(x + 180, y + h - 170, 240, 50, Color(255, 255, 255, 190));
                framebuffer.drawFilledEllipse(x + 520, y + h - 180, 280, 60, Color(255, 255, 255, 210));
                framebuffer.drawFilledEllipse(x + 920, y + h - 175, 310, 55, Color(255, 255, 255, 190));
                break;
            }
            }
        }

        static void drawEmotionBubble(PKSEFramebuffer &framebuffer, int cx, int cy, ParkEmotion emotion)
        {
            if (emotion == ParkEmotion::None) return;

            constexpr int bw = 32, bh = 26;
            framebuffer.drawFilledRoundedRect(cx - bw / 2, cy - bh, bw, bh, 8, Colors::Panel);
            framebuffer.drawRoundedRect(cx - bw / 2, cy - bh, bw, bh, 8, Colors::Border, 1);

            // Little triangular bubble tail
            framebuffer.drawFilledCircle(cx, cy - 1, 3, Colors::Panel);

            switch (emotion)
            {
            case ParkEmotion::Heart:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x99\xA5", Color(255, 75, 115), TextStyle::Caption);
                break;
            case ParkEmotion::Music:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x99\xAA", Color(45, 195, 145), TextStyle::Caption);
                break;
            case ParkEmotion::Sleep:
                framebuffer.drawText(cx - 10, cy - bh + 5, "Zzz", Color(155, 135, 225), TextStyle::Caption);
                break;
            case ParkEmotion::Angry:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x9C\x96", Color(245, 65, 55), TextStyle::Caption);
                break;
            case ParkEmotion::Exclamation:
                framebuffer.drawText(cx - 3, cy - bh + 4, "!", Color(255, 185, 45), TextStyle::Heading);
                break;
            case ParkEmotion::Question:
                framebuffer.drawText(cx - 4, cy - bh + 4, "?", Color(85, 165, 245), TextStyle::Heading);
                break;
            default:
                break;
            }
        }

        void drawPokepark(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();
            PokeparkManager &mgr = PokeparkManager::instance();

            // 1. Advance simulation
            mgr.update(1.0f / 60.0f);

            const int panelX = 32, panelY = 96, panelW = 1216, panelH = 588;

            // Frame outline
            framebuffer.drawSoftShadow(panelX, panelY, panelW, panelH, 16);
            framebuffer.drawFilledRoundedRect(panelX, panelY, panelW, panelH, 16, Colors::Panel);
            framebuffer.drawRoundedRect(panelX, panelY, panelW, panelH, 16, Colors::Border, 1);

            // Compute hour for sky lighting
            time_t now = time(nullptr);
            struct tm *tInfo = localtime(&now);
            int hour = tInfo ? tInfo->tm_hour : 12;

            // Scenery background
            framebuffer.setClipRect(panelX + 2, panelY + 2, panelW - 4, panelH - 4);
            drawSky(framebuffer, panelX + 2, panelY + 2, panelW - 4, panelH - 4, hour);
            drawHabitatScenery(framebuffer, panelX, panelY, panelW, panelH, mgr.getCurrentHabitat());

            // Header banner
            const auto &envInfo = PokeparkManager::getEnvironmentInfo(mgr.getCurrentHabitat());
            constexpr int headerH = 50;
            framebuffer.drawFilledRoundedRect(panelX, panelY, panelW, headerH, 16, Color(20, 25, 35, 190));
            framebuffer.drawFilledRect(panelX, panelY + headerH - 16, panelW, 16, Color(20, 25, 35, 190));

            // Habitat Title
            std::string titleStr = std::string(envInfo.name) + "  ·  " + envInfo.subtitle;
            framebuffer.drawText(panelX + 24, panelY + 12, titleStr, Color(255, 255, 255), TextStyle::Heading);

            // L/R Habitat Switcher Indicator on top right
            std::string habitatNav = "◀ L  Habitats (1-4)  R ▶";
            int hNavW, hNavH;
            framebuffer.measureText(habitatNav, hNavW, hNavH, TextStyle::Caption);
            const int navPillW = hNavW + 28, navPillH = 30;
            const int navPillX = panelX + panelW - navPillW - 20, navPillY = panelY + 10;
            framebuffer.drawPill(navPillX, navPillY, navPillW, navPillH, Color(255, 255, 255, 40));
            framebuffer.drawText(navPillX + 14, navPillY + (navPillH - hNavH) / 2, habitatNav,
                                 Color(255, 255, 255), TextStyle::Caption);
            screen.touchButtons.push_back({250, navPillX, navPillY, navPillW / 2, navPillH});
            screen.touchButtons.push_back({251, navPillX + navPillW / 2, navPillY, navPillW / 2, navPillH});

            // Journal button beside it
            const int jPillW = 100, jPillH = 30;
            const int jPillX = navPillX - jPillW - 12, jPillY = navPillY;
            framebuffer.drawPill(jPillX, jPillY, jPillW, jPillH, Color(255, 255, 255, 40));
            framebuffer.drawText(jPillX + 16, jPillY + (jPillH - hNavH) / 2, "Y: Journal",
                                 Color(255, 255, 255), TextStyle::Caption);
            screen.touchButtons.push_back({260, jPillX, jPillY, jPillW, jPillH});

            // 2. Draw Roaming Residents
            auto visibleIndices = mgr.getVisibleResidentIndices(mgr.getCurrentHabitat());
            const auto &residents = mgr.getResidents();

            // Clamp selected resident index
            if (screen.pokeparkSelectedResident < 0) screen.pokeparkSelectedResident = 0;
            if (!visibleIndices.empty() && screen.pokeparkSelectedResident >= static_cast<int>(visibleIndices.size()))
                screen.pokeparkSelectedResident = static_cast<int>(visibleIndices.size()) - 1;

            for (size_t vi = 0; vi < visibleIndices.size(); ++vi)
            {
                int rIdx = visibleIndices[vi];
                const auto &res = residents[rIdx];

                const int rx = panelX + static_cast<int>(res.x);
                const int ry = panelY + static_cast<int>(res.y);
                const bool isSelected = (static_cast<int>(vi) == screen.pokeparkSelectedResident);

                // Bobbing while walking / breathing
                float bob = res.isMoving ? std::sin(res.animClock * 10.0f) * 3.5f
                                         : std::sin(res.animClock * 2.5f) * 1.5f;

                // Soft ground shadow
                framebuffer.drawFilledEllipse(rx, ry + 26, 26, 9, Color(0, 0, 0, 70));

                // Selection glowing disc
                if (isSelected)
                {
                    framebuffer.drawFilledEllipse(rx, ry + 26, 34, 12, Color(255, 215, 0, 120));
                    framebuffer.drawCircle(rx, ry + 26, 32, Colors::Primary, 2);

                    // Name pill above head
                    int nw, nh;
                    framebuffer.measureText(res.name, nw, nh, TextStyle::Caption);
                    const int npW = nw + 18, npH = 22;
                    const int npX = rx - npW / 2, npY = ry - 48 - static_cast<int>(bob);
                    framebuffer.drawPill(npX, npY, npW, npH, Color(20, 25, 35, 200));
                    framebuffer.drawText(npX + 9, npY + (npH - nh) / 2, res.name, Color(255, 255, 255), TextStyle::Caption);
                }

                // Sprite
                constexpr int spriteSize = 64;
                Sprite *sp = SpriteManager::getIconSprite(res.species, res.form, res.shiny);
                if (sp && sp->data)
                {
                    framebuffer.drawSpriteFit(rx - spriteSize / 2, ry - spriteSize / 2 - static_cast<int>(bob), spriteSize, spriteSize,
                                              sp->width, sp->height, sp->data, sp->channels);
                }

                // Emotion Bubble
                if (res.emotion != ParkEmotion::None)
                {
                    drawEmotionBubble(framebuffer, rx, ry - 38 - static_cast<int>(bob), res.emotion);
                }

                // Touch button for this resident
                screen.touchButtons.push_back({200 + static_cast<int>(vi), rx - 35, ry - 40, 70, 75});
            }

            framebuffer.clearClip();

            // Bottom control guide
            framebuffer.drawFilledRoundedRect(panelX + 16, panelY + panelH - 36, panelW - 32, 28, 8, Color(20, 25, 35, 180));
            framebuffer.drawText(panelX + 28, panelY + panelH - 30,
                                 "L / R: Switch Biome   |   D-Pad: Select   |   A: Profile   |   X: Quick Talk   |   Y: Journal   |   B: Back",
                                 Color(240, 240, 245), TextStyle::Caption);

            // 3. Modals & Overlays

            // Overlay A: Resident Profile Card
            if (screen.pokeparkCardActive && !visibleIndices.empty())
            {
                PopIn pop(framebuffer, "pokeparkCard");
                int rIdx = visibleIndices[screen.pokeparkSelectedResident];
                const auto &res = residents[rIdx];

                constexpr int cardW = 680, cardH = 430;
                const int cardX = panelX + (panelW - cardW) / 2;
                const int cardY = panelY + (panelH - cardH) / 2;

                framebuffer.drawSoftShadow(cardX, cardY, cardW, cardH, 20);
                framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Panel);
                framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Border, 2);

                // Header
                framebuffer.drawCardBand(cardX, cardY, cardW, 46);
                framebuffer.drawText(cardX + 24, cardY + 12, res.name + " · Resident Profile", Colors::Text, TextStyle::Heading);

                // Big Render / Sprite
                constexpr int pSize = 130;
                const int pX = cardX + 30, pY = cardY + 70;
                framebuffer.drawFilledRoundedRect(pX, pY, pSize, pSize, 14, Colors::PanelAlt);
                framebuffer.drawRoundedRect(pX, pY, pSize, pSize, 14, Colors::Border, 1);
                Sprite *bigSp = SpriteManager::getSprite(res.species, res.form, res.shiny);
                if (bigSp && bigSp->data)
                {
                    framebuffer.drawImageScaled(pX + 5, pY + 5, bigSp->width, bigSp->height, pSize - 10, pSize - 10,
                                                bigSp->data, bigSp->channels);
                }

                // Info Rows on right
                const int infoX = cardX + 185;
                int rowY = cardY + 65;

                auto drawRow = [&](const char *label, const std::string &val)
                {
                    framebuffer.drawText(infoX, rowY, label, Colors::TextDim, TextStyle::Body);
                    framebuffer.drawText(infoX + 130, rowY, val, Colors::Text, TextStyle::Body);
                    rowY += 34;
                };

                drawRow("Personality", PokeparkManager::getPersonalityName(res.personality));
                drawRow("Trait", PokeparkManager::getPersonalityDescription(res.personality));
                rowY += 4;
                drawRow("Current Mood", PokeparkManager::getMoodName(res.mood));
                drawRow("Source", res.sourceInfo);

                // Action Buttons at bottom: [A] Talk | [X] Play | [Y] Snack | [R] Relax
                constexpr int btnW = 140, btnH = 44, btnGap = 16;
                const int btnStartX = cardX + (cardW - (btnW * 4 + btnGap * 3)) / 2;
                const int btnY = cardY + cardH - 64;

                const char *actions[4] = {"A: Talk", "X: Play", "Y: Snack", "R: Relax"};
                for (int b = 0; b < 4; ++b)
                {
                    int bx = btnStartX + b * (btnW + btnGap);
                    bool selected = (screen.pokeparkCardActionSelected == b);
                    framebuffer.drawFilledRoundedRect(bx, btnY, btnW, btnH, 10, selected ? Colors::Primary : Colors::PanelAlt);
                    if (selected)
                        framebuffer.drawRoundedRect(bx, btnY, btnW, btnH, 10, Colors::Accent, 2);
                    int tw, th;
                    framebuffer.measureText(actions[b], tw, th, TextStyle::Body);
                    framebuffer.drawText(bx + (btnW - tw) / 2, btnY + (btnH - th) / 2, actions[b],
                                         selected ? Colors::PrimaryText : Colors::Text, TextStyle::Body);
                    screen.touchButtons.push_back({300 + b, bx, btnY, btnW, btnH});
                }
            }

            // Overlay B: Dialogue Speech Balloon
            if (!screen.pokeparkDialogueText.empty())
            {
                constexpr int dW = 740, dH = 120;
                const int dX = panelX + (panelW - dW) / 2;
                const int dY = panelY + panelH - dH - 30;

                framebuffer.drawSoftShadow(dX, dY, dW, dH, 16);
                framebuffer.drawFilledRoundedRect(dX, dY, dW, dH, 16, Colors::Panel);
                framebuffer.drawRoundedRect(dX, dY, dW, dH, 16, Colors::Accent, 2);

                framebuffer.drawText(dX + 26, dY + 24, screen.pokeparkDialogueText, Colors::Text, TextStyle::Body);
                framebuffer.drawText(dX + dW - 140, dY + dH - 30, "A: Continue", Colors::TextDim, TextStyle::Caption);

                screen.touchButtons.push_back({350, dX, dY, dW, dH});
            }

            // Overlay C: Adventure Journal Page
            if (screen.pokeparkJournalActive)
            {
                PopIn pop(framebuffer, "pokeparkJournal");
                constexpr int jW = 860, jH = 500;
                const int jX = panelX + (panelW - jW) / 2;
                const int jY = panelY + (panelH - jH) / 2;

                framebuffer.drawSoftShadow(jX, jY, jW, jH, 20);
                framebuffer.drawFilledRoundedRect(jX, jY, jW, jH, 16, Colors::Panel);
                framebuffer.drawRoundedRect(jX, jY, jW, jH, 16, Colors::Border, 2);

                framebuffer.drawCardBand(jX, jY, jW, 46);
                framebuffer.drawText(jX + 24, jY + 12, "Poképark Adventure Journal", Colors::Text, TextStyle::Heading);

                const auto &journal = mgr.getJournal();
                if (journal.empty())
                {
                    framebuffer.drawText(jX + 40, jY + 140, "No entries yet. Leave your Pokémon to explore and play together!",
                                         Colors::TextDim, TextStyle::Body);
                }
                else
                {
                    constexpr int rowsShown = 6;
                    int startIndex = std::clamp(screen.pokeparkJournalScroll, 0, std::max(0, static_cast<int>(journal.size()) - rowsShown));

                    int ey = jY + 62;
                    for (int i = 0; i < rowsShown && (startIndex + i) < static_cast<int>(journal.size()); ++i)
                    {
                        const auto &entry = journal[startIndex + i];
                        framebuffer.drawFilledRoundedRect(jX + 20, ey, jW - 40, 58, 10, Colors::PanelAlt);

                        // Time pill
                        framebuffer.drawPill(jX + 30, ey + 15, 65, 28, Colors::Background);
                        framebuffer.drawText(jX + 38, ey + 20, entry.timeStr, Colors::TextDim, TextStyle::Caption);

                        // Kind badge
                        Color badgeCol = (entry.kind == ParkJournalEventKind::Friendship) ? Color(45, 175, 125) :
                                         (entry.kind == ParkJournalEventKind::Affection)  ? Color(245, 75, 125) :
                                         (entry.kind == ParkJournalEventKind::Discovery)  ? Color(235, 165, 45) :
                                         Colors::Primary;
                        framebuffer.drawFilledCircle(jX + 115, ey + 29, 6, badgeCol);

                        // Event narrative line
                        framebuffer.drawText(jX + 135, ey + 18, entry.text, Colors::Text, TextStyle::Body);

                        ey += 66;
                    }
                }

                // Footer
                framebuffer.drawText(jX + 26, jY + jH - 32, "Up / Down: Scroll  |  B: Close", Colors::TextDim, TextStyle::Caption);
            }
        }
    }
}
