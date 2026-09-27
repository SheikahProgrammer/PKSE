#include <algorithm>
#include <string>
#include <vector>

#include "UI/Panels/EventGalleryPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Events/EventDatabase.h"
#include "Names/MoveNames.h"
#include "Names/ItemNames.h"
#include "Enums/Ball.h"
#include "Trainer/Trainer.h"

namespace UI {
namespace Panels {

    void drawEventGallery(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        screen.touchButtons.clear();

        auto& db = Events::EventDatabase::getInstance();
        if (!db.isLoaded()) {
            db.load();
            screen.refreshEventList();
        }

        const auto& list = screen.filteredEvents;
        const int totalCount = static_cast<int>(list.size());

        // Clamp selected index and scroll offset
        if (screen.eventSelectedIndex >= totalCount) {
            screen.eventSelectedIndex = std::max(0, totalCount - 1);
        }
        constexpr int visibleRows = 7;
        if (screen.eventSelectedIndex < screen.eventScrollOffset) {
            screen.eventScrollOffset = screen.eventSelectedIndex;
        } else if (screen.eventSelectedIndex >= screen.eventScrollOffset + visibleRows) {
            screen.eventScrollOffset = screen.eventSelectedIndex - visibleRows + 1;
        }

        // =========================================================================
        // TOP HEADER BAR
        // =========================================================================
        const int hX = 32, hY = 20, hW = 1216, hH = 54;
        fb.drawFilledRoundedRect(hX, hY, hW, hH, 14, Colors::Panel);
        fb.drawRoundedRect(hX, hY, hW, hH, 14, Colors::Border, 1);

        // Title
        int tw, th;
        fb.measureText("Mystery Gift & Events", tw, th, TextStyle::Heading);
        fb.drawText(hX + 20, hY + (hH - th) / 2, "Mystery Gift & Events", Colors::Text, TextStyle::Heading);

        // Filter status button [Y]
        const int fy = hY + 10, fh = 34;
        std::string filterLabel = screen.eventFilterCurrentGame ? "[Y] Filter: Current Game" : "[Y] Filter: All Gens";
        int flw, flh;
        fb.measureText(filterLabel, flw, flh, TextStyle::Caption);
        const int fw = flw + 24;
        const int fx = hX + 380;
        fb.drawPill(fx, fy, fw, fh, screen.eventFilterCurrentGame ? Colors::Primary : Colors::PanelAlt);
        fb.drawText(fx + 12, fy + (fh - flh) / 2, filterLabel, screen.eventFilterCurrentGame ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
        screen.touchButtons.push_back({ 2001, fx, fy, fw, fh }); // Touch button for filter toggle

        // Search button [X]
        std::string searchLabel = screen.eventSearchQuery.empty() ? "[X] Search" : ("[X] Search: " + screen.eventSearchQuery);
        int slw, slh;
        fb.measureText(searchLabel, slw, slh, TextStyle::Caption);
        const int sw = slw + 24;
        const int sx = fx + fw + 16;
        fb.drawPill(sx, fy, sw, fh, !screen.eventSearchQuery.empty() ? Colors::Accent : Colors::PanelAlt);
        fb.drawText(sx + 12, fy + (fh - slh) / 2, searchLabel, !screen.eventSearchQuery.empty() ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
        screen.touchButtons.push_back({ 2002, sx, fy, sw, fh }); // Touch button for search

        // Total count on far right
        std::string countStr = std::to_string(totalCount) + " events";
        int cw, ch;
        fb.measureText(countStr, cw, ch, TextStyle::Body);
        fb.drawText(hX + hW - cw - 24, hY + (hH - ch) / 2, countStr, Colors::TextDim, TextStyle::Body);

        // =========================================================================
        // LEFT COLUMN: EVENT LIST (x: 32, y: 86, w: 530, h: 560)
        // =========================================================================
        const int lX = 32, lY = 86, lW = 530, lH = 560;
        fb.drawSoftShadow(lX, lY, lW, lH, 16);
        fb.drawFilledRoundedRect(lX, lY, lW, lH, 16, Colors::Panel);
        fb.drawRoundedRect(lX, lY, lW, lH, 16, Colors::Border, 1);

        const int rowH = 74, rowGap = 5;
        const int listTop = lY + 10;

        if (totalCount == 0) {
            fb.drawText(lX + 40, lY + 100, "No events match your filter / search.", Colors::TextDim, TextStyle::Body);
        } else {
            for (int r = 0; r < visibleRows; ++r) {
                int itemIdx = screen.eventScrollOffset + r;
                if (itemIdx >= totalCount) break;

                size_t dbIdx = list[itemIdx];
                const auto* rec = db.getEvent(dbIdx);
                if (!rec) continue;

                const int ry = listTop + r * (rowH + rowGap);
                const int rx = lX + 10, rw = lW - 20;
                const bool focused = (itemIdx == screen.eventSelectedIndex);

                if (focused) {
                    fb.drawFilledRoundedRect(rx, ry, rw, rowH, 12, Colors::Primary);
                } else {
                    fb.drawFilledRoundedRect(rx, ry, rw, rowH, 12, Colors::PanelAlt);
                }

                // Mini sprite disc
                const int discR = 24;
                const int dcx = rx + 32, dcy = ry + rowH / 2;
                fb.drawFilledCircle(dcx, dcy, discR, focused ? Colors::AccentDim : Colors::Panel);

                Sprite* sp = SpriteManager::getIconSprite(rec->species, rec->form, rec->isShiny != 0);
                if (sp && sp->data) {
                    const int sz = 40;
                    fb.drawImageScaled(dcx - sz / 2, dcy - sz / 2, sp->width, sp->height, sz, sz, sp->data, sp->channels);
                }

                // Title
                const Color txtCol = focused ? Colors::PrimaryText : Colors::Text;
                fb.drawText(rx + 68, ry + 12, rec->title, txtCol, TextStyle::Body);

                // Sub-row: Gen Badge, Level, Shiny star, Compat dot
                const int subY = ry + 42;
                std::string genStr = "Gen " + std::to_string(rec->generation);
                std::string lvStr = "Lv." + std::to_string(rec->level);

                int gw, gh; fb.measureText(genStr, gw, gh, TextStyle::Caption);
                fb.drawPill(rx + 68, subY, gw + 14, 22, focused ? Colors::Panel : Colors::AccentDim);
                fb.drawText(rx + 75, subY + 3, genStr, focused ? Colors::Text : Colors::PrimaryText, TextStyle::Caption);

                fb.drawText(rx + 68 + gw + 22, subY + 3, lvStr, focused ? Colors::PrimaryText : Colors::TextDim, TextStyle::Caption);

                if (rec->isShiny) {
                    fb.drawText(rx + 68 + gw + 70, subY + 1, "* SHINY", Colors::ShinyStar, TextStyle::Caption);
                }

                // Compatibility indicator dot on right edge
                const bool compat = Events::EventDatabase::isCompatibleWithGame(*rec, screen.trainer.getGameGroup());
                const int dotX = rx + rw - 24, dotY = ry + rowH / 2;
                fb.drawFilledCircle(dotX, dotY, 6, compat ? Colors::Green : Colors::Accent);

                screen.touchButtons.push_back({ 3000 + itemIdx, rx, ry, rw, rowH });
            }

            // Scrollbar on list
            if (totalCount > visibleRows) {
                const int sbX = lX + lW - 8, sbY = lY + 16, sbH = lH - 32;
                fb.drawFilledRect(sbX, sbY, 4, sbH, Colors::Border);
                const int thumbH = std::max(20, sbH * visibleRows / totalCount);
                const int thumbY = sbY + (sbH - thumbH) * screen.eventScrollOffset / (totalCount - visibleRows);
                fb.drawFilledRoundedRect(sbX - 1, thumbY, 6, thumbH, 3, Colors::Primary);
            }
        }

        // =========================================================================
        // RIGHT COLUMN: EVENT SPOTLIGHT DETAILS (x: 580, y: 86, w: 668, h: 560)
        // =========================================================================
        const int sX = 580, sY = 86, sW = 668, sH = 560;
        fb.drawSoftShadow(sX, sY, sW, sH, 16);
        fb.drawFilledRoundedRect(sX, sY, sW, sH, 16, Colors::Panel);
        fb.drawRoundedRect(sX, sY, sW, sH, 16, Colors::Border, 1);

        if (screen.eventSelectedIndex >= 0 && screen.eventSelectedIndex < totalCount) {
            size_t selDbIdx = list[screen.eventSelectedIndex];
            const auto* sel = db.getEvent(selDbIdx);
            if (sel) {
                const bool compat = Events::EventDatabase::isCompatibleWithGame(*sel, screen.trainer.getGameGroup());

                // Header in details
                fb.drawFilledRoundedRect(sX + 16, sY + 16, sW - 32, 40, 10, compat ? Colors::AccentDim : Colors::PanelAlt);
                std::string compBanner = compat ? "[READY] Directly receivable into current game"
                                                : "[BANK] Can be deposited to Bank & cross-transferred";
                fb.drawText(sX + 32, sY + 26, compBanner, compat ? Colors::Green : Colors::Accent, TextStyle::Caption);

                // Big HD Sprite Display (left side of details card)
                const int spX = sX + 24, spY = sY + 68, spSize = 160;
                fb.drawFilledRoundedRect(spX, spY, spSize, spSize, 14, Colors::PanelAlt);
                fb.drawRoundedRect(spX, spY, spSize, spSize, 14, Colors::Border, 1);

                Sprite* hd = SpriteManager::getSprite(sel->species, sel->form, sel->isShiny != 0);
                if (hd && hd->data) {
                    fb.drawImageScaled(spX + 10, spY + 10, hd->width, hd->height, spSize - 20, spSize - 20, hd->data, hd->channels);
                } else {
                    Sprite* icon = SpriteManager::getIconSprite(sel->species, sel->form, sel->isShiny != 0);
                    if (icon && icon->data) {
                        fb.drawImageScaled(spX + 20, spY + 20, icon->width, icon->height, spSize - 40, spSize - 40, icon->data, icon->channels);
                    }
                }

                // Info block beside HD sprite (x: sX + 200)
                const int iX = sX + 200, iY = sY + 70;
                const char* spName = Trainer::getSpeciesName(sel->species);
                std::string nameStr = spName ? spName : ("Species #" + std::to_string(sel->species));
                fb.drawText(iX, iY, nameStr, Colors::Text, TextStyle::Heading);

                // Event Title subtext
                fb.drawText(iX, iY + 28, sel->title, Colors::Primary, TextStyle::Body);

                // Key Facts Grid
                const int gy = iY + 56;
                std::string facts1 = "Level: " + std::to_string(sel->level) + "  |  Gen: " + std::to_string(sel->generation);
                fb.drawText(iX, gy, facts1, Colors::TextDim, TextStyle::Body);

                const char* itName = (sel->generation == 3)
                    ? Names::getItemNameG3(sel->heldItem)
                    : Names::getItemName(sel->heldItem);
                std::string itemStr = (sel->heldItem > 0 && itName) ? itName : "None";
                fb.drawText(iX, gy + 24, "Held Item: ", Colors::TextDim, TextStyle::Body);
                int itemLblW = 0, itemLblH = 0;
                fb.measureText("Held Item: ", itemLblW, itemLblH, TextStyle::Body);
                int itemContentX = iX + itemLblW;
                if (sel->heldItem > 0)
                {
                    uint16_t spriteItemId = (sel->generation == 3) ? Names::itemG3ToModern(sel->heldItem) : sel->heldItem;
                    Sprite* itemSp = SpriteManager::getItemSprite(spriteItemId);
                    if (itemSp && itemSp->data)
                    {
                        constexpr int itemIconSize = 18;
                        fb.drawImageScaled(itemContentX, gy + 24 + 1, itemSp->width, itemSp->height,
                                           itemIconSize, itemIconSize, itemSp->data, itemSp->channels);
                        itemContentX += itemIconSize + 6;
                    }
                }
                fb.drawText(itemContentX, gy + 24, itemStr, (sel->heldItem > 0) ? Colors::Text : Colors::TextDim, TextStyle::Body);

                const char* natName = Trainer::getNatureName(sel->nature);
                std::string natStr = (sel->nature < 25 && natName) ? natName : "Random Nature";
                fb.drawText(iX, gy + 48, "Nature: " + natStr, Colors::TextDim, TextStyle::Body);

                // Ball
                const char* bName = Enums::getBallName(sel->ball);
                std::string ballName = (sel->ball > 0 && bName) ? bName : (sel->ball == 16 ? "Cherish Ball" : "Poké Ball");
                fb.drawText(iX, gy + 72, "Ball: ", Colors::TextDim, TextStyle::Body);
                int ballLblW = 0, ballLblH = 0;
                fb.measureText("Ball: ", ballLblW, ballLblH, TextStyle::Body);
                int ballContentX = iX + ballLblW;
                if (sel->ball > 0)
                {
                    Sprite* ballSp = SpriteManager::getBallSprite(sel->ball);
                    if (ballSp && ballSp->data)
                    {
                        constexpr int ballIconSize = 18;
                        fb.drawImageScaled(ballContentX, gy + 72 + 1, ballSp->width, ballSp->height,
                                           ballIconSize, ballIconSize, ballSp->data, ballSp->channels);
                        ballContentX += ballIconSize + 6;
                    }
                }
                fb.drawText(ballContentX, gy + 72, ballName, Colors::Text, TextStyle::Body);

                // ---------------------------------------------------------------------
                // Lower Details: OT, Trainer Info & Moves
                // ---------------------------------------------------------------------
                const int dy = sY + 244;
                fb.drawFilledRoundedRect(sX + 24, dy, sW - 48, 140, 12, Colors::PanelAlt);

                // Left column: OT, IDs, Met
                const int lx = sX + 40, ly = dy + 16;
                std::string otStr = sel->otName[0] != '\0' ? sel->otName : "(Your OT)";
                fb.drawText(lx, ly, "Original Trainer: " + otStr, Colors::Text, TextStyle::Body);
                fb.drawText(lx, ly + 24, "TID: " + std::to_string(sel->tid16) + "  |  SID: " + std::to_string(sel->sid16), Colors::TextDim, TextStyle::Caption);
                fb.drawText(lx, ly + 46, "Met Location ID: " + std::to_string(sel->metLocation), Colors::TextDim, TextStyle::Caption);
                std::string dateStr = "Date: 20" + std::to_string(sel->metYear) + "-" + std::to_string(sel->metMonth) + "-" + std::to_string(sel->metDay);
                fb.drawText(lx, ly + 68, dateStr, Colors::TextDim, TextStyle::Caption);
                fb.drawText(lx, ly + 90, sel->fateful ? "Fateful Encounter: Yes" : "Fateful Encounter: No", Colors::Accent, TextStyle::Caption);

                // Right column: Moveset box
                const int mx = sX + 340, my = dy + 16;
                fb.drawText(mx, my - 2, "Moveset:", Colors::Text, TextStyle::Caption);
                for (int m = 0; m < 4; ++m) {
                    uint16_t mv = sel->moves[m];
                    const char* mvName = (mv > 0) ? Names::getMoveName(mv) : "---";
                    std::string mStr = std::to_string(m + 1) + ". " + (mvName ? mvName : "---");
                    fb.drawText(mx, my + 20 + m * 24, mStr, (mv > 0) ? Colors::Text : Colors::TextDim, TextStyle::Body);
                }

                // Ribbons Row
                const int ry = dy + 152;
                fb.drawText(sX + 24, ry, "Event Ribbons:", Colors::TextDim, TextStyle::Caption);
                int rbx = sX + 130;
                if (sel->ribbonClassic) { fb.drawPill(rbx, ry - 4, 80, 22, Colors::Primary); fb.drawText(rbx + 8, ry - 1, "Classic", Colors::PrimaryText, TextStyle::Caption); rbx += 88; }
                if (sel->ribbonEvent)   { fb.drawPill(rbx, ry - 4, 80, 22, Colors::Primary); fb.drawText(rbx + 8, ry - 1, "Event", Colors::PrimaryText, TextStyle::Caption); rbx += 88; }
                if (sel->ribbonPremier) { fb.drawPill(rbx, ry - 4, 80, 22, Colors::Primary); fb.drawText(rbx + 8, ry - 1, "Premier", Colors::PrimaryText, TextStyle::Caption); rbx += 88; }
                if (sel->ribbonWishing) { fb.drawPill(rbx, ry - 4, 80, 22, Colors::Primary); fb.drawText(rbx + 8, ry - 1, "Wishing", Colors::PrimaryText, TextStyle::Caption); rbx += 88; }
                if (sel->ribbonSouvenir){ fb.drawPill(rbx, ry - 4, 80, 22, Colors::Primary); fb.drawText(rbx + 8, ry - 1, "Souvenir", Colors::PrimaryText, TextStyle::Caption); rbx += 88; }
                if (!sel->ribbonClassic && !sel->ribbonEvent && !sel->ribbonPremier && !sel->ribbonWishing && !sel->ribbonSouvenir) {
                    fb.drawText(rbx, ry, "None", Colors::TextDim, TextStyle::Caption);
                }

                // Action Buttons at bottom of details
                const int by = sY + sH - 58;
                const int bw = 190, bh = 44;
                if (compat) {
                    // Receive into Box
                    fb.drawPill(sX + 24, by, bw, bh, Colors::Primary);
                    fb.drawText(sX + 44, by + 12, "[A] Put in Box", Colors::PrimaryText, TextStyle::Body);
                    screen.touchButtons.push_back({ 1001, sX + 24, by, bw, bh });

                    // Receive into Party
                    fb.drawPill(sX + 24 + bw + 14, by, bw, bh, Colors::PanelAlt);
                    fb.drawText(sX + 44 + bw + 14, by + 12, "Put in Party", Colors::Text, TextStyle::Body);
                    screen.touchButtons.push_back({ 1002, sX + 24 + bw + 14, by, bw, bh });
                }

                // Receive into Bank (always available)
                const int bankBx = compat ? (sX + 24 + 2 * (bw + 14)) : (sX + 24);
                const int bankBw = compat ? (sW - 48 - 2 * (bw + 14)) : (sW - 48);
                fb.drawPill(bankBx, by, bankBw, bh, !compat ? Colors::Primary : Colors::PanelAlt);
                fb.drawText(bankBx + (bankBw - 130) / 2, by + 12, compat ? "Put in Bank" : "[A] Put in Bank", !compat ? Colors::PrimaryText : Colors::Text, TextStyle::Body);
                screen.touchButtons.push_back({ 1003, bankBx, by, bankBw, bh });
            }
        }

        // =========================================================================
        // FOOTER INSTRUCTIONS BAR
        // =========================================================================
        const int ftY = 660, ftH = 46;
        fb.drawFilledRoundedRect(hX, ftY, hW, ftH, 12, Colors::Panel);
        fb.drawRoundedRect(hX, ftY, hW, ftH, 12, Colors::Border, 1);

        const char* inst = "Arrows: Move  |  A: Receive Gift  |  X: Search  |  Y: Toggle Filter  |  L/R: Page  |  ZL/ZR: ±25  |  B: Back";
        int iw, ih;
        fb.measureText(inst, iw, ih, TextStyle::Caption);
        fb.drawText(hX + (hW - iw) / 2, ftY + (ftH - ih) / 2, inst, Colors::TextDim, TextStyle::Caption);
    }

} // namespace Panels
} // namespace UI