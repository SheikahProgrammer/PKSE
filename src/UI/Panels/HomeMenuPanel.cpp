#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "UI/Panels/HomeMenuPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"

using namespace Trainer;

namespace UI
{
    namespace Panels
    {

        // Count live (non-empty) Pokemon in a boxes container.
        template <typename Boxes>
        static int countMons(const Boxes &boxes)
        {
            int count = 0;
            for (const auto &box : boxes)
                for (const auto &pokemon : box)
                    if (pokemon && pokemon->speciesID() != 0)
                        ++count;
            return count;
        }

        // Draw a compact, non-interactive preview of the current box (disc + sprite per slot).
        static void drawBoxPreview(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer, int boxPreviewX,
                                   int boxPreviewY, int boxPreviewWidth, int boxPreviewHeight)
        {
            constexpr int headerH = 40;
            framebuffer.drawCardBand(boxPreviewX, boxPreviewY, boxPreviewWidth, headerH);

            const int box = screen.selectedBoxIndex;
            const bool inRange = box >= 0 && box < static_cast<int>(screen.trainer.boxes.size());
            std::string boxName = (inRange && box < static_cast<int>(screen.trainer.boxNames.size()))
                                      ? screen.trainer.boxNames[box]
                                      : ("Box " + std::to_string(box + 1));
            int boxNameWidth, bnH;
            framebuffer.measureText(boxName, boxNameWidth, bnH, TextStyle::Body);
            framebuffer.drawText(boxPreviewX + (boxPreviewWidth - boxNameWidth) / 2, boxPreviewY + (headerH - bnH) / 2,
                                 boxName, Colors::Text);
            if (!inRange)
                return;

            const int slotsPerBox = static_cast<int>(screen.trainer.getSlotsPerBox());
            const int cols = boxGridColumns(slotsPerBox);
            const int rows = 5;
            const int gridX = boxPreviewX + 16, gy = boxPreviewY + headerH + 10;
            const int gridWidth = boxPreviewWidth - 32, gh = boxPreviewHeight - headerH - 20;
            const int colPitch = gridWidth / cols, rowPitch = gh / rows;
            int discR = std::min(colPitch, rowPitch) / 2 - 5;
            if (discR < 14)
                discR = 14;

            const auto &curBox = screen.trainer.boxes[box];
            const bool pk = isPkForgeTheme();
            const Color wallpaper = Pksm::wallpaperAt(box);
            if (pk)
                framebuffer.pkWallpaper(boxPreviewX + 8, gy - 4, boxPreviewWidth - 16, gh + 8, wallpaper);
            for (int rIndex = 0; rIndex < rows; ++rIndex)
            {
                for (int columnIndex = 0; columnIndex < cols; ++columnIndex)
                {
                    const int index = rIndex * cols + columnIndex;
                    const int centerX = gridX + columnIndex * colPitch + colPitch / 2;
                    const int centerY = gy + rIndex * rowPitch + rowPitch / 2;
                    const auto &pokemon = curBox[index];
                    const bool empty = !pokemon || pokemon->speciesID() == 0;
                    if (pk)
                    {
                        // No cell frames: an empty slot is a faint waiting ball on the wallpaper.
                        if (empty)
                        {
                            const int size = std::min(colPitch, rowPitch) - 6;
                            framebuffer.pkEmptySlot(centerX - size / 2, centerY - size / 2, size, size, wallpaper);
                            continue;
                        }
                    }
                    else
                        framebuffer.drawFilledCircle(centerX, centerY, discR, empty ? Colors::Panel : Colors::PanelAlt);
                    if (empty)
                    {
                        framebuffer.drawCircle(centerX, centerY, discR, Colors::Border, 1);
                        continue;
                    }
                    int markerSize = std::min(static_cast<int>(discR * 1.8), colPitch - 4);
                    if (pokemon->isEgg())
                    {
                        framebuffer.drawEgg(centerX, centerY, markerSize); // eggs show as an egg in the box preview too
                    }
                    else
                    {
                        bool shiny = pokemon->isShiny(pokemon->id32(), pokemon->species());
                        Sprite *internalSpeciesId =
                            SpriteManager::getIconSprite(pokemon->speciesID(), pokemon->form(), shiny);
                        if (internalSpeciesId && internalSpeciesId->data)
                        {
                            framebuffer.drawSpriteFit(centerX - markerSize / 2, centerY - markerSize / 2, markerSize, markerSize,
                                                      internalSpeciesId->width, internalSpeciesId->height, internalSpeciesId->data, internalSpeciesId->channels);
                        }
                    }
                }
            }
        }

        // Simple vector icons for the menu's circular buttons (no icon assets). `col` = glyph color,
        // `bg` = the button fill (used to punch holes). kind: 0 Items (bag), 1 Events (gift box), 2 Pokédex (Poké Ball), 3 Trainer (person), 4 Settings (gear).
        static void drawMenuIcon(PKSEFramebuffer &framebuffer, int centerX, int centerY, int cornerRadius, int kind,
                                 Color iconColor, Color backgroundColor)
        {
            if (kind == 0)
            { // Items: a satchel/bag
                const int boxWidth = 2 * cornerRadius, bh = 2 * cornerRadius - 4;
                const int boxX = centerX - boxWidth / 2, by = centerY - bh / 2 + 4;
                framebuffer.drawFilledRoundedRect(boxX, by, boxWidth, bh, 8, iconColor);                    // body
                framebuffer.drawFilledRoundedRect(centerX - cornerRadius + 4, by - 8, 2 * cornerRadius - 8, 14, 6,
                                                  iconColor); // flap
                // clasp (punch)
                framebuffer.drawFilledRoundedRect(centerX - 3, centerY - 2, 6, bh / 2, 3, backgroundColor);
            }
            else if (kind == 1)
            { // Events: a gift box with ribbon
                const int boxW = 2 * cornerRadius - 4, boxH = cornerRadius + 4;
                const int boxX = centerX - boxW / 2, boxY = centerY - boxH / 2 + 5;
                // Box lid
                const int lidW = boxW + 6, lidH = 8;
                framebuffer.drawFilledRoundedRect(centerX - lidW / 2, boxY - lidH, lidW, lidH, 3, iconColor);
                // Box body
                framebuffer.drawFilledRoundedRect(boxX, boxY, boxW, boxH, 4, iconColor);
                // Vertical ribbon
                const int ribW = 6;
                framebuffer.drawFilledRect(centerX - ribW / 2, boxY - lidH, ribW, boxH + lidH, backgroundColor);
                // Horizontal ribbon
                framebuffer.drawFilledRect(boxX, boxY + boxH / 2 - 2, boxW, 4, backgroundColor);
                // Ribbon bow on top
                framebuffer.drawFilledCircle(centerX - 5, boxY - lidH - 3, 4, iconColor);
                framebuffer.drawFilledCircle(centerX + 5, boxY - lidH - 3, 4, iconColor);
                framebuffer.drawFilledCircle(centerX - 5, boxY - lidH - 3, 2, backgroundColor);
                framebuffer.drawFilledCircle(centerX + 5, boxY - lidH - 3, 2, backgroundColor);
            }
            else if (kind == 2)
            { // Pokédex: a vector Poké Ball
                // Outer circle rim in iconColor
                framebuffer.drawFilledCircle(centerX, centerY, cornerRadius - 2, iconColor);
                // Upper half in iconColor, lower half punched with backgroundColor
                framebuffer.drawFilledCircle(centerX, centerY, cornerRadius - 4, iconColor);
                framebuffer.drawFilledRect(centerX - cornerRadius + 4, centerY, (cornerRadius - 4) * 2, cornerRadius - 4, backgroundColor);
                // Center dividing line
                framebuffer.drawFilledRect(centerX - cornerRadius + 4, centerY - 1, (cornerRadius - 4) * 2, 3, iconColor);
                // Center button
                framebuffer.drawFilledCircle(centerX, centerY, std::max(3, (cornerRadius - 2) / 3 + 1), iconColor);
                framebuffer.drawFilledCircle(centerX, centerY, std::max(2, (cornerRadius - 2) / 3 - 1), backgroundColor);
                framebuffer.drawFilledCircle(centerX, centerY, std::max(1, (cornerRadius - 2) / 5), iconColor);
            }
            else if (kind == 3)
            {                                                        // Trainer: a person bust
                // head
                framebuffer.drawFilledCircle(centerX, centerY - cornerRadius / 2, cornerRadius / 2 - 1, iconColor);
                const int shapeWidth = 2 * cornerRadius - 6, sh = cornerRadius + 2;
                framebuffer.drawFilledRoundedRect(centerX - shapeWidth / 2, centerY + 2, shapeWidth, sh, shapeWidth / 2,
                                                  iconColor); // shoulders
            }
            else if (kind == 5)
            { // Poképark: a leafy tree
                const int trunkW = 6, trunkH = cornerRadius;
                framebuffer.drawFilledRoundedRect(centerX - trunkW / 2, centerY, trunkW, trunkH, 2, iconColor);
                framebuffer.drawFilledCircle(centerX - cornerRadius / 3, centerY - cornerRadius / 4, cornerRadius / 2 + 1, iconColor);
                framebuffer.drawFilledCircle(centerX + cornerRadius / 3, centerY - cornerRadius / 4, cornerRadius / 2 + 1, iconColor);
                framebuffer.drawFilledCircle(centerX, centerY - cornerRadius / 2 - 1, cornerRadius / 2 + 2, iconColor);
                framebuffer.drawFilledCircle(centerX, centerY - cornerRadius / 3, 3, backgroundColor);
            }
            else
            { // Settings: a gear
                const double piRadians = 3.14159265358979323846;
                for (int index = 0; index < 8; ++index)
                {
                    double spokeAngleRadians = index * piRadians / 4.0;
                    int spokeX =
                        centerX + static_cast<int>(std::lround(std::cos(spokeAngleRadians) * (cornerRadius - 2)));
                    int spokeY =
                        centerY + static_cast<int>(std::lround(std::sin(spokeAngleRadians) * (cornerRadius - 2)));
                    framebuffer.drawFilledCircle(spokeX, spokeY, 4, iconColor); // teeth
                }
                framebuffer.drawFilledCircle(centerX, centerY, cornerRadius - 6, iconColor);      // body
                framebuffer.drawFilledCircle(centerX, centerY, (cornerRadius - 6) / 2, backgroundColor); // hole
            }
        }

        void drawHomeMenu(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();

            const int previewX = 32, pY = 96, pW = 560, pH = 540;
            framebuffer.drawSoftShadow(previewX, pY, pW, pH, 16);
            framebuffer.drawFilledRoundedRect(previewX, pY, pW, pH, 16, Colors::Panel);
            drawBoxPreview(screen, framebuffer, previewX, pY, pW, pH);
            framebuffer.drawRoundedRect(previewX, pY, pW, pH, 16, Colors::Border, 1);

            const int rightColumnX = 648, rW = 600;
            const int stored = countMons(screen.trainer.boxes);
            int party = 0;
            for (const auto &m : screen.trainer.party)
                if (m && m->speciesID() != 0)
                    ++party;
            int bankN = 0;
            if (screen.bank)
                bankN = countMons(screen.bank->boxes);

            struct Pill
            {
                const char *label;
                std::string subtitle;
                int menuIndex;
            };
            Pill pills[3] = {
                {"Pokémon", std::to_string(stored) + " stored", 0},
                {"Party", std::to_string(party) + " / 6", 1},
                {"Storage", std::to_string(bankN) + " in bank", 2},
            };
            const int pillH = 76, pillGap = 18;
            int panelY = 108;
            const bool pk = isPkForgeTheme();
            for (const auto &p : pills)
            {
                const bool focused = (screen.homeMenuIndex == p.menuIndex);
                if (pk)
                {
                    // The Nintendo focus pop (DsCard.Selected): a short spring up when focused and back
                    // when not -- kept to 2% here, since a full-width row at 5% crowds its neighbours.
                    static const char *const pillIds[3] = {"home.pill.0", "home.pill.1", "home.pill.2"};
                    const float pop = framebuffer.spring(pillIds[p.menuIndex], focused ? 1.02f : 1.0f, 900.0f,
                                                         focused ? 33.0f : 60.0f, 1.0f);
                    framebuffer.pushScale(rightColumnX + rW / 2.0f, panelY + pillH / 2.0f, pop);
                    // DsFolderButton: the menu-button row, its count as the detail on the right.
                    if (focused)
                        framebuffer.pkSelectedButton(rightColumnX, panelY, rW, pillH, 4);
                    else
                        framebuffer.pkBlackButton(rightColumnX, panelY, rW, pillH, 4);
                    framebuffer.drawText(rightColumnX + 30, framebuffer.textYCenteredOn(panelY + pillH / 2, TextStyle::Heading),
                                         p.label, Pksm::Ink, TextStyle::Heading);
                    int subtitleWidth, sH;
                    framebuffer.measureText(p.subtitle, subtitleWidth, sH, TextStyle::Body);
                    framebuffer.drawText(rightColumnX + rW - subtitleWidth - 24,
                                         framebuffer.textYCenteredOn(panelY + pillH / 2), p.subtitle,
                                         focused ? Pksm::Ink : Pksm::InkSoft);
                    framebuffer.popTransform();
                    screen.touchButtons.push_back({100 + p.menuIndex, rightColumnX, panelY, rW, pillH});
                    panelY += pillH + pillGap;
                    continue;
                }
                framebuffer.drawSoftShadow(rightColumnX, panelY, rW, pillH, pillH / 2);
                framebuffer.drawPill(rightColumnX, panelY, rW, pillH, focused ? Colors::Primary : Colors::PanelAlt);
                const Color labelColor = focused ? Colors::PrimaryText : Colors::Text;
                if (focused)
                    // ▶ pointer
                    framebuffer.drawSymbol(rightColumnX - 30, panelY + pillH / 2 - 12, "\xE2\x96\xB6", Colors::Primary);

                int labelWidth, lH;
                framebuffer.measureText(p.label, labelWidth, lH, TextStyle::Heading);
                framebuffer.drawText(rightColumnX + 36, panelY + (pillH - lH) / 2, p.label, labelColor,
                                     TextStyle::Heading);

                // Count sub-pill on the right.
                int subtitleWidth, sH;
                framebuffer.measureText(p.subtitle, subtitleWidth, sH, TextStyle::Caption);
                const int subW = subtitleWidth + 28, subH = 30;
                const int subX = rightColumnX + rW - subW - 20, subY = panelY + (pillH - subH) / 2;
                framebuffer.drawPill(subX, subY, subW, subH, focused ? Colors::PrimaryText : Colors::Panel);
                framebuffer.drawText(subX + 14, subY + (subH - sH) / 2, p.subtitle,
                                     focused ? Colors::Primary : Colors::TextDim, TextStyle::Caption);

                screen.touchButtons.push_back({100 + p.menuIndex, rightColumnX, panelY, rW, pillH});
                panelY += pillH + pillGap;
            }

            struct Icon
            {
                const char *label;
                const char *glyph;
                int menuIndex;
                int kind;
            };
            Icon icons[6] = {
                {"Items", "I", 3, 0},
                {"Events", "E", 4, 1},
                {"Pokédex", "D", 5, 2},
                {"Poképark", "P", 6, 5},
                {"Trainer", "T", 7, 3},
                {"Settings", "S", 8, 4}
            };
            const int iconR = 32;
            const int slot = rW / 6;
            const int iconTop = panelY + 22;
            if (pk)
            {
                // DsCard: a navy tile per destination, the icon as hero and the label beneath.
                constexpr int tileGap = 10, tileH = 104;
                const int tileW = (rW - tileGap * 5) / 6;
                const int tileY = panelY + 4;
                for (int innerIndex = 0; innerIndex < 6; ++innerIndex)
                {
                    const auto &ic = icons[innerIndex];
                    const int tileX = rightColumnX + innerIndex * (tileW + tileGap);
                    const bool focused = (screen.homeMenuIndex == ic.menuIndex);
                    // DsCard's focus pop: up to 105% on a springy curve (130ms), back on a smooth one.
                    static const char *const tileIds[6] = {"home.tile.0", "home.tile.1", "home.tile.2",
                                                           "home.tile.3", "home.tile.4", "home.tile.5"};
                    const float pop = framebuffer.spring(tileIds[innerIndex], focused ? 1.05f : 1.0f, 900.0f,
                                                         focused ? 33.0f : 60.0f, 1.0f);
                    framebuffer.pushScale(tileX + tileW / 2.0f, tileY + tileH / 2.0f, pop);
                    if (focused)
                        framebuffer.pkSelectedButton(tileX, tileY, tileW, tileH, 6);
                    else
                        framebuffer.pkBlackButton(tileX, tileY, tileW, tileH, 6);
                    static const char *const pksmIcons[6] = {"bag", "events", "pokedex", "park", "trainer", "settings"};
                    Sprite *icon = SpriteManager::getUiIcon(pksmIcons[innerIndex]);
                    if (icon && icon->data)
                    {
                        // Pixel art at an integer-ish 36 px box, aspect kept (PksmIcons.Icon, size 36).
                        const int box = 40;
                        const int iw = icon->width >= icon->height ? box : icon->width * box / icon->height;
                        const int ih = icon->width >= icon->height ? icon->height * box / icon->width : box;
                        framebuffer.drawImageScaled(tileX + (tileW - iw) / 2, tileY + 40 - ih / 2, icon->width,
                                                    icon->height, iw, ih, icon->data, icon->channels);
                    }
                    else
                        drawMenuIcon(framebuffer, tileX + tileW / 2, tileY + 40, 18, ic.kind, Pksm::Ink,
                                     focused ? Color(0x31, 0x55, 0x9A) : Pksm::LogoDeck);
                    int labelWidth, lH;
                    framebuffer.measureText(ic.label, labelWidth, lH, TextStyle::Caption);
                    framebuffer.drawText(tileX + (tileW - labelWidth) / 2, tileY + tileH - 30, ic.label, Pksm::Ink,
                                         TextStyle::Caption);
                    framebuffer.popTransform();
                    screen.touchButtons.push_back({100 + ic.menuIndex, tileX, tileY, tileW, tileH});
                }
                return;
            }
            for (int innerIndex = 0; innerIndex < 6; ++innerIndex)
            {
                const auto &ic = icons[innerIndex];
                const int centerX = rightColumnX + slot * innerIndex + slot / 2;
                const int centerY = iconTop + iconR;
                const bool focused = (screen.homeMenuIndex == ic.menuIndex);
                if (focused)
                    framebuffer.drawFilledCircle(
                        centerX, centerY, iconR + 5,
                        Color(Colors::Primary.red, Colors::Primary.green, Colors::Primary.blue, 70));
                framebuffer.drawFilledCircle(centerX, centerY, iconR, focused ? Colors::Primary : Colors::PanelAlt);
                framebuffer.drawCircle(centerX, centerY, iconR, focused ? Colors::Primary : Colors::Border, 2);

                drawMenuIcon(framebuffer, centerX, centerY, iconR - 10, ic.kind,
                             focused ? Colors::PrimaryText : Colors::Text,
                             focused ? Colors::Primary : Colors::PanelAlt);

                int labelWidth, lH;
                framebuffer.measureText(ic.label, labelWidth, lH, TextStyle::Caption);
                framebuffer.drawText(centerX - labelWidth / 2, centerY + iconR + 8, ic.label,
                                     focused ? Colors::Primary : Colors::TextDim, TextStyle::Caption);

                screen.touchButtons.push_back(
                    {100 + ic.menuIndex, centerX - iconR, centerY - iconR, 2 * iconR, 2 * iconR + 24});
            }
        }
    }
}
