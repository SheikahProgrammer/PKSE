#include <algorithm>
#include <string>
#include <vector>

#include "UI/Panels/EventGalleryPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "UI/ScreenChrome.h"
#include "UI/Dialogs/DialogFrame.h"
#include "UI/Dialogs/EditControls.h"
#include "Events/EventDatabase.h"
#include "Events/KeyItemEventService.h"
#include "Events/MysteryGiftInbox.h"
#include "Names/MoveNames.h"
#include "Names/ItemNames.h"
#include "Enums/Ball.h"
#include "Trainer/Trainer.h"

namespace UI {
namespace Panels {

    void drawEventGallery(TrainerViewScreen& screen, PKSEFramebuffer& fb) {
        screen.touchButtons.clear();
        // PKForge's Mystery Gift world: one plum popup over the dimmed screen.
        drawPopupFrame(fb, Pksm::GiftPink);

        // =========================================================================
        // TOP HEADER BAR & TABS
        // =========================================================================
        const int hX = 32, hY = 20, hW = 1216, hH = 54;
        fb.drawFilledRoundedRect(hX, hY, hW, hH, 14, Colors::Panel);
        fb.drawRoundedRect(hX, hY, hW, hH, 14, Colors::Border, 1);

        // Title
        int tw, th;
        fb.measureText("Events", tw, th, TextStyle::Heading);
        fb.drawText(hX + 20, hY + (hH - th) / 2, "Events", Colors::Text, TextStyle::Heading);

        // Sub-tabs: 0 = Distributions, 1 = Key Item Events, 2 = Save Wonder Cards
        const int tabY = hY + 10, tabH = 34;
        const int tab0X = hX + 130, tab0W = 160;
        const int tab1X = tab0X + tab0W + 12, tab1W = 175;
        const int tab2X = tab1X + tab1W + 12, tab2W = 180;

        // Tab 0
        fb.drawPill(tab0X, tabY, tab0W, tabH, screen.eventTabMode == 0 ? Colors::Primary : Colors::PanelAlt);
        fb.drawText(tab0X + 14, tabY + 8, "[ZL] Distributions", screen.eventTabMode == 0 ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
        screen.touchButtons.push_back({ 2100, tab0X, tabY, tab0W, tabH });

        // Tab 1
        fb.drawPill(tab1X, tabY, tab1W, tabH, screen.eventTabMode == 1 ? Colors::Primary : Colors::PanelAlt);
        fb.drawText(tab1X + 16, tabY + 8, "Key Item Events", screen.eventTabMode == 1 ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
        screen.touchButtons.push_back({ 2101, tab1X, tabY, tab1W, tabH });

        // Tab 2
        fb.drawPill(tab2X, tabY, tab2W, tabH, screen.eventTabMode == 2 ? Colors::Primary : Colors::PanelAlt);
        fb.drawText(tab2X + 14, tabY + 8, "[ZR] Wonder Cards", screen.eventTabMode == 2 ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
        screen.touchButtons.push_back({ 2102, tab2X, tabY, tab2W, tabH });

        // =========================================================================
        // MODE 0: DISTRIBUTION GALLERY (Default)
        // =========================================================================
        if (screen.eventTabMode == 0) {
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

            // Filter button [Y]
            const int fy = hY + 10, fh = 34;
            std::string filterLabel = screen.eventFilterCurrentGame ? "[Y] Filter: Current" : "[Y] Filter: All";
            int flw, flh;
            fb.measureText(filterLabel, flw, flh, TextStyle::Caption);
            const int fw = flw + 20;
            const int fx = tab2X + tab2W + 20;
            fb.drawPill(fx, fy, fw, fh, screen.eventFilterCurrentGame ? Colors::Primary : Colors::PanelAlt);
            fb.drawText(fx + 10, fy + (fh - flh) / 2, filterLabel, screen.eventFilterCurrentGame ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
            screen.touchButtons.push_back({ 2001, fx, fy, fw, fh });

            // Search button [X]
            std::string searchLabel = screen.eventSearchQuery.empty() ? "[X] Search" : ("[X] Search: " + screen.eventSearchQuery);
            int slw, slh;
            fb.measureText(searchLabel, slw, slh, TextStyle::Caption);
            const int sw = slw + 20;
            const int sx = fx + fw + 10;
            fb.drawPill(sx, fy, sw, fh, !screen.eventSearchQuery.empty() ? Colors::Accent : Colors::PanelAlt);
            fb.drawText(sx + 10, fy + (fh - slh) / 2, searchLabel, !screen.eventSearchQuery.empty() ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
            screen.touchButtons.push_back({ 2002, sx, fy, sw, fh });

            // Total count on far right
            std::string countStr = std::to_string(totalCount) + " events";
            int cw, ch;
            fb.measureText(countStr, cw, ch, TextStyle::Body);
            fb.drawText(hX + hW - cw - 20, hY + (hH - ch) / 2, countStr, Colors::TextDim, TextStyle::Body);

            // Left Column: Event List (x: 32, y: 86, w: 530, h: 560)
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
                        fb.drawSpriteFit(dcx - sz / 2, dcy - sz / 2, sz, sz,
                                         sp->width, sp->height, sp->data, sp->channels);
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

                    // Compatibility dot
                    const bool compat = Events::EventDatabase::isCompatibleWithGame(*rec, screen.trainer.getGameGroup());
                    const int dotX = rx + rw - 24, dotY = ry + rowH / 2;
                    fb.drawFilledCircle(dotX, dotY, 6, compat ? Colors::Green : Colors::Accent);

                    screen.touchButtons.push_back({ 3000 + itemIdx, rx, ry, rw, rowH });
                }

                // Scrollbar
                if (totalCount > visibleRows) {
                    const int sbX = lX + lW - 8, sbY = lY + 16, sbH = lH - 32;
                    fb.drawFilledRect(sbX, sbY, 4, sbH, Colors::Border);
                    const int thumbH = std::max(20, sbH * visibleRows / totalCount);
                    const int thumbY = sbY + (sbH - thumbH) * screen.eventScrollOffset / (totalCount - visibleRows);
                    fb.drawFilledRoundedRect(sbX - 1, thumbY, 6, thumbH, 3, Colors::Primary);
                }
            }

            // Right Column: Event Details (x: 580, y: 86, w: 668, h: 560)
            const int sX = 580, sY = 86, sW = 668, sH = 560;
            fb.drawSoftShadow(sX, sY, sW, sH, 16);
            fb.drawFilledRoundedRect(sX, sY, sW, sH, 16, Colors::Panel);
            fb.drawRoundedRect(sX, sY, sW, sH, 16, Colors::Border, 1);

            if (screen.eventSelectedIndex >= 0 && screen.eventSelectedIndex < totalCount) {
                size_t selDbIdx = list[screen.eventSelectedIndex];
                const auto* sel = db.getEvent(selDbIdx);
                if (sel) {
                    const bool compat = Events::EventDatabase::isCompatibleWithGame(*sel, screen.trainer.getGameGroup());

                    fb.drawFilledRoundedRect(sX + 16, sY + 16, sW - 32, 40, 10, compat ? Colors::AccentDim : Colors::PanelAlt);
                    std::string compBanner = compat ? "[READY] Directly receivable into current game"
                                                    : "[BANK] Can be deposited to Bank & cross-transferred";
                    fb.drawText(sX + 32, sY + 26, compBanner, compat ? Colors::Green : Colors::Accent, TextStyle::Caption);

                    // Big HD Sprite
                    const int spX = sX + 24, spY = sY + 68, spSize = 160;
                    fb.drawFilledRoundedRect(spX, spY, spSize, spSize, 14, Colors::PanelAlt);
                    fb.drawRoundedRect(spX, spY, spSize, spSize, 14, Colors::Border, 1);

                    Sprite* hd = SpriteManager::getSprite(sel->species, sel->form, sel->isShiny != 0);
                    if (hd && hd->data) {
                        fb.drawImageScaled(spX + 10, spY + 10, hd->width, hd->height, spSize - 20, spSize - 20, hd->data, hd->channels);
                    } else {
                        Sprite* icon = SpriteManager::getIconSprite(sel->species, sel->form, sel->isShiny != 0);
                        if (icon && icon->data) {
                            fb.drawSpriteFit(spX + 20, spY + 20, spSize - 40, spSize - 40,
                                             icon->width, icon->height, icon->data, icon->channels);
                        }
                    }

                    // Info block
                    const int iX = sX + 200, iY = sY + 70;
                    const char* spName = Trainer::getSpeciesName(sel->species);
                    std::string nameStr = spName ? spName : ("Species #" + std::to_string(sel->species));
                    fb.drawText(iX, iY, nameStr, Colors::Text, TextStyle::Heading);
                    fb.drawText(iX, iY + 28, sel->title, Colors::Primary, TextStyle::Body);

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
                    if (sel->heldItem > 0) {
                        uint16_t spriteItemId = (sel->generation == 3) ? Names::itemG3ToModern(sel->heldItem) : sel->heldItem;
                        Sprite* itemSp = SpriteManager::getItemSprite(spriteItemId);
                        if (itemSp && itemSp->data) {
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

                    // Ball (sanitized)
                    uint8_t displayBall = (sel->ball > 0 && sel->ball <= 37) ? sel->ball : 16;
                    const char* bName = Enums::getBallName(displayBall);
                    std::string ballName = (bName && std::string(bName) != "(unknown)") ? bName : "Cherish Ball";
                    fb.drawText(iX, gy + 72, "Ball: ", Colors::TextDim, TextStyle::Body);
                    int ballLblW = 0, ballLblH = 0;
                    fb.measureText("Ball: ", ballLblW, ballLblH, TextStyle::Body);
                    int ballContentX = iX + ballLblW;
                    Sprite* ballSp = SpriteManager::getBallSprite(displayBall);
                    if (ballSp && ballSp->data) {
                        constexpr int ballIconSize = 18;
                        fb.drawImageScaled(ballContentX, gy + 72 + 1, ballSp->width, ballSp->height,
                                           ballIconSize, ballIconSize, ballSp->data, ballSp->channels);
                        ballContentX += ballIconSize + 6;
                    }
                    fb.drawText(ballContentX, gy + 72, ballName, Colors::Text, TextStyle::Body);

                    // Lower details
                    const int dy = sY + 244;
                    fb.drawFilledRoundedRect(sX + 24, dy, sW - 48, 140, 12, Colors::PanelAlt);

                    const int lx = sX + 40, ly = dy + 16;
                    std::string otStr = sel->otName[0] != '\0' ? sel->otName : "(Your OT)";
                    fb.drawText(lx, ly, "Original Trainer: " + otStr, Colors::Text, TextStyle::Body);
                    fb.drawText(lx, ly + 24, "TID: " + std::to_string(sel->tid16) + "  |  SID: " + std::to_string(sel->sid16), Colors::TextDim, TextStyle::Caption);
                    fb.drawText(lx, ly + 46, "Met Location ID: " + std::to_string(sel->metLocation), Colors::TextDim, TextStyle::Caption);
                    std::string dateStr = "Date: 20" + std::to_string(sel->metYear) + "-" + std::to_string(sel->metMonth) + "-" + std::to_string(sel->metDay);
                    fb.drawText(lx, ly + 68, dateStr, Colors::TextDim, TextStyle::Caption);
                    fb.drawText(lx, ly + 90, sel->fateful ? "Fateful Encounter: Yes" : "Fateful Encounter: No", Colors::Accent, TextStyle::Caption);

                    const int mx = sX + 340, my = dy + 16;
                    fb.drawText(mx, my - 2, "Moveset:", Colors::Text, TextStyle::Caption);
                    for (int m = 0; m < 4; ++m) {
                        uint16_t mv = sel->moves[m];
                        const char* mvName = (mv > 0) ? Names::getMoveName(mv) : "---";
                        std::string mStr = std::to_string(m + 1) + ". " + (mvName ? mvName : "---");
                        fb.drawText(mx, my + 20 + m * 24, mStr, (mv > 0) ? Colors::Text : Colors::TextDim, TextStyle::Body);
                    }

                    // Ribbons
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

                    // Buttons
                    const int by = sY + sH - 58;
                    const int bw = 190, bh = 44;
                    if (compat) {
                        fb.drawPill(sX + 24, by, bw, bh, Colors::Primary);
                        fb.drawText(sX + 44, by + 12, "[A] Put in Box", Colors::PrimaryText, TextStyle::Body);
                        screen.touchButtons.push_back({ 1001, sX + 24, by, bw, bh });

                        fb.drawPill(sX + 24 + bw + 14, by, bw, bh, Colors::PanelAlt);
                        fb.drawText(sX + 44 + bw + 14, by + 12, "Put in Party", Colors::Text, TextStyle::Body);
                        screen.touchButtons.push_back({ 1002, sX + 24 + bw + 14, by, bw, bh });
                    }

                    const int bankBx = compat ? (sX + 24 + 2 * (bw + 14)) : (sX + 24);
                    const int bankBw = compat ? (sW - 48 - 2 * (bw + 14)) : (sW - 48);
                    fb.drawPill(bankBx, by, bankBw, bh, !compat ? Colors::Primary : Colors::PanelAlt);
                    fb.drawText(bankBx + (bankBw - 130) / 2, by + 12, compat ? "Put in Bank" : "[A] Put in Bank", !compat ? Colors::PrimaryText : Colors::Text, TextStyle::Body);
                    screen.touchButtons.push_back({ 1003, bankBx, by, bankBw, bh });
                }
            }

            // Footer
            const int ftY = 660, ftH = 46;
            const char* inst = "ZL/ZR: Tab  |  Arrows: Move  |  A: Receive Gift  |  X: Search  |  Y: Toggle Filter  |  B: Back";
            drawPopupHints(fb, hX, ftY, hW, ftH, inst);
            return;
        }

        // =========================================================================
        // MODE 1: KEY ITEM EVENTS (PKForge KeyItemEventsEditor)
        // =========================================================================
        if (screen.eventTabMode == 1) {
            auto keyEvents = Events::KeyItemEventService::getEvents(screen.trainer);
            const int totalCount = static_cast<int>(keyEvents.size());

            if (screen.keyEventSelectedIndex >= totalCount) {
                screen.keyEventSelectedIndex = std::max(0, totalCount - 1);
            }
            constexpr int visibleRows = 6;
            if (screen.keyEventSelectedIndex < screen.keyEventScrollOffset) {
                screen.keyEventScrollOffset = screen.keyEventSelectedIndex;
            } else if (screen.keyEventSelectedIndex >= screen.keyEventScrollOffset + visibleRows) {
                screen.keyEventScrollOffset = screen.keyEventSelectedIndex - visibleRows + 1;
            }

            const std::string subtitle = "Item + event flags, as the real distribution writes them";
            int sw, sh; fb.measureText(subtitle, sw, sh, TextStyle::Caption);
            fb.drawText(hX + hW - sw - 24, hY + (hH - sh) / 2, subtitle, Colors::TextDim, TextStyle::Caption);

            const int lX = 32, lY = 86, lW = 530, lH = 560;
            fb.drawSoftShadow(lX, lY, lW, lH, 16);
            fb.drawFilledRoundedRect(lX, lY, lW, lH, 16, Colors::Panel);
            fb.drawRoundedRect(lX, lY, lW, lH, 16, Colors::Border, 1);

            const int rowH = 82, rowGap = 6;
            const int listTop = lY + 12;
            const bool gen3Ids = screen.trainer.getGameGroup() == GameVersion::RSE ||
                                 screen.trainer.getGameGroup() == GameVersion::FRLG;

            if (totalCount == 0) {
                const auto lines = wrapTextToWidth(fb,
                    "Key-item events are mapped for Crystal, Ruby/Sapphire, Emerald, FireRed/LeafGreen, Diamond/Pearl, "
                    "Platinum, HeartGold/SoulSilver, Black/White and Omega Ruby/Alpha Sapphire.", lW - 60, TextStyle::Body);
                int ly = lY + 40;
                for (const auto &line : lines) { fb.drawText(lX + 30, ly, line, Colors::TextDim, TextStyle::Body); ly += 30; }
            } else {
                for (int r = 0; r < visibleRows; ++r) {
                    const int itemIdx = screen.keyEventScrollOffset + r;
                    if (itemIdx >= totalCount) break;

                    const auto& ev = keyEvents[itemIdx];
                    const int ry = listTop + r * (rowH + rowGap);
                    const int rx = lX + 10, rw = lW - 20;
                    const bool focused = (itemIdx == screen.keyEventSelectedIndex);

                    fb.drawFilledRoundedRect(rx, ry, rw, rowH, 12, focused ? Colors::Primary : Colors::PanelAlt);

                    // Item icon (falls back to PKForge's "key" pixel icon)
                    const int dcx = rx + 34, dcy = ry + rowH / 2;
                    Sprite* sp = ev.itemId ? SpriteManager::getItemSprite(gen3Ids ? Names::itemG3ToModern(ev.itemId) : ev.itemId)
                                           : nullptr;
                    if (!sp || !sp->data) sp = SpriteManager::getUiIcon("key");
                    if (sp && sp->data) {
                        const int isz = 36;
                        fb.drawSpriteFit(dcx - isz / 2, dcy - isz / 2, isz, isz,
                                         sp->width, sp->height, sp->data, sp->channels);
                    }

                    const Color txtCol = focused ? Colors::PrimaryText : Colors::Text;
                    fb.drawText(rx + 68, ry + 14, ev.title, txtCol, TextStyle::Body);
                    fb.drawText(rx + 68, ry + 46, ev.destination, focused ? Colors::PrimaryText : Colors::TextDim,
                                TextStyle::Caption);

                    // State, PKForge wording; signal colours only for Ready / Incomplete.
                    const std::string label = Events::keyItemStateLabel(ev.state);
                    const Color stCol = ev.state == Events::KeyItemEventState::Enabled   ? Pksm::Legal
                                      : ev.state == Events::KeyItemEventState::Partial   ? Pksm::GiftRed
                                                                                          : Colors::TextDim;
                    int bw, bh; fb.measureText(label, bw, bh, TextStyle::Caption);
                    fb.drawText(rx + rw - bw - 20, ry + 16, label, focused && stCol == Colors::TextDim ? Colors::PrimaryText : stCol,
                                TextStyle::Caption);

                    screen.touchButtons.push_back({ 4000 + itemIdx, rx, ry, rw, rowH });
                }

                if (totalCount > visibleRows) {
                    const int sbX = lX + lW - 8, sbY = lY + 16, sbH = lH - 32;
                    fb.drawFilledRect(sbX, sbY, 4, sbH, Colors::Border);
                    const int thumbH = std::max(20, sbH * visibleRows / totalCount);
                    const int thumbY = sbY + (sbH - thumbH) * screen.keyEventScrollOffset / (totalCount - visibleRows);
                    fb.drawFilledRoundedRect(sbX - 1, thumbY, 6, thumbH, 3, Colors::Accent);
                }
            }

            const int sX = 580, sY = 86, sW = 668, sH = 560;
            fb.drawSoftShadow(sX, sY, sW, sH, 16);
            fb.drawFilledRoundedRect(sX, sY, sW, sH, 16, Colors::Panel);
            fb.drawRoundedRect(sX, sY, sW, sH, 16, Colors::Border, 1);

            if (screen.keyEventSelectedIndex >= 0 && screen.keyEventSelectedIndex < totalCount) {
                const auto& sel = keyEvents[screen.keyEventSelectedIndex];
                fb.drawCardBand(sX, sY, sW, 50);
                fb.drawText(sX + 22, fb.textYCenteredOn(sY + 28, TextStyle::Heading), sel.title, Colors::Text, TextStyle::Heading);

                // Item box
                const int spX = sX + 24, spY = sY + 72, spSize = 120;
                fb.drawFilledRoundedRect(spX, spY, spSize, spSize, 6, Colors::PanelAlt);
                fb.drawRoundedRect(spX, spY, spSize, spSize, 6, Colors::Border, 1);
                Sprite* sp = sel.itemId ? SpriteManager::getItemSprite(gen3Ids ? Names::itemG3ToModern(sel.itemId) : sel.itemId)
                                        : nullptr;
                if (!sp || !sp->data) sp = SpriteManager::getUiIcon("key");
                if (sp && sp->data)
                    fb.drawSpriteFit(spX + 20, spY + 20, 80, 80, sp->width, sp->height, sp->data, sp->channels);

                // The PKForge detail lines.
                const int iX = sX + 166, textW = sW - 190;
                int iy = sY + 76;
                auto line = [&](const std::string &text, Color color) {
                    for (const auto &l : wrapTextToWidth(fb, text, textW, TextStyle::Body)) {
                        fb.drawText(iX, iy, l, color, TextStyle::Body);
                        iy += 28;
                    }
                    iy += 4;
                };
                line(sel.itemName + "  \xE2\x86\x92  " + sel.destination, Colors::Accent);
                line(sel.needsItem
                         ? std::string("Item in bag: ") + (sel.hasItem ? "yes" : "no") + "  \xC2\xB7  Event flags: " + (sel.eventArmed ? "yes" : "no")
                         : std::string("Event flags: ") + (sel.eventArmed ? "yes" : "no") + "  \xC2\xB7  the " + sel.itemName + " is handed over in game",
                     Colors::Text);
                iy = std::max(iy, spY + spSize + 16);
                const int fullX = sX + 24, fullW = sW - 48;
                for (const auto &l : wrapTextToWidth(fb, Events::keyItemStateDetail(sel.state), fullW, TextStyle::Body)) {
                    fb.drawText(fullX, iy, l, Colors::TextDim, TextStyle::Body);
                    iy += 28;
                }
                if (!sel.prerequisite.empty()) {
                    iy += 6;
                    for (const auto &l : wrapTextToWidth(fb, "Note: " + sel.prerequisite, fullW, TextStyle::Body)) {
                        fb.drawText(fullX, iy, l, Colors::Warning, TextStyle::Body);
                        iy += 28;
                    }
                }

                // Actions: Enable unless already Ready, Disable whenever there is something to take back.
                const bool canEnable = sel.state != Events::KeyItemEventState::Enabled;
                const bool canDisable = sel.hasItem || sel.eventArmed;
                const int by = sY + sH - 70, bw = 260, bh = TouchTargetMin;
                int bx = sX + 24;
                if (canEnable) {
                    drawGlyphButton(fb, bx, by, bw, bh, "A", "Enable event", Colors::PanelAlt, Colors::Text,
                                    screen.pendingTapButtonId == 1101);
                    if (isPkForgeTheme())
                        fb.drawRoundedRect(bx, by, bw, bh, 4, Pksm::Legal, 2);
                    screen.touchButtons.push_back({ 1101, bx, by, bw, bh });
                    bx += bw + 16;
                }
                if (canDisable) {
                    drawGlyphButton(fb, bx, by, bw, bh, "X", "Disable event", Colors::PanelAlt, Colors::Text,
                                    screen.pendingTapButtonId == 1102);
                    if (isPkForgeTheme())
                        fb.drawRoundedRect(bx, by, bw, bh, 4, Pksm::GiftRed, 2);
                    screen.touchButtons.push_back({ 1102, bx, by, bw, bh });
                }

                if (screen.keyEventConfirmActive) {
                    drawPopupHints(fb, hX, 660, hW, 46, "A: Confirm  |  B: Cancel");
                    PopIn pop(fb, "keyEventConfirm");
                    const bool enabling = screen.keyEventConfirmEnable;
                    std::string body;
                    if (enabling)
                        body = sel.needsItem ? "Adds the " + sel.itemName + " and sets the event flags."
                                             : std::string("Sets the event flags.");
                    else
                        body = sel.needsItem ? "Removes the " + sel.itemName + " and clears the event flags. Shown/caught history is kept."
                                             : std::string("Clears the event flags. Received/caught history is kept.");
                    body += " Nothing is written until you save.";
                    constexpr int dW = 620, dH = 270;
                    const int dX = (fb.getWidth() - dW) / 2, dY = (fb.getHeight() - dH) / 2;
                    int cy = Dialogs::drawDialogFrame(fb, dX, dY, dW, dH, enabling ? "Enable event?" : "Disable event?",
                                                      enabling ? Colors::Text : Color(0xF0, 0x68, 0x68));
                    fb.beginTyping(); // the message types out; the buttons follow it
                    for (const auto &l : wrapTextToWidth(fb, body, dW - 56, TextStyle::Body)) {
                        fb.drawText(dX + 28, cy, l, Colors::Text, TextStyle::Body);
                        cy += 28;
                    }
                    screen.touchButtons.clear();
                    const int cbW = 200, cbH = TouchTargetMin, cbY = dY + dH - cbH - 18;
                    Dialogs::drawEditChoiceButton(screen, fb, dX + dW - cbW * 2 - 36, cbY, cbW, cbH, "B", "Cancel", 0, Colors::PanelAlt);
                    Dialogs::drawEditChoiceButton(screen, fb, dX + dW - cbW - 20, cbY, cbW, cbH, "A",
                                                  enabling ? "Enable" : "Disable", 1,
                                                  enabling ? Colors::PanelAlt : Colors::Red, enabling ? Colors::Text : Colors::White);
                    return;
                }
            }

            const int ftY = 660, ftH = 46;
            drawPopupHints(fb, hX, ftY, hW, ftH, "ZL/ZR: Tab  |  Arrows: Move  |  A: Enable event  |  X: Disable event  |  B: Back");
            return;
        }

        // =========================================================================
        // MODE 2: SAVE FILE WONDER CARD INBOX & REDEMPTION RECORDS
        // =========================================================================
        if (screen.eventTabMode == 2) {
            auto inboxCards = Events::MysteryGiftInbox::getInboxCards(screen.trainer);
            const int totalCount = static_cast<int>(inboxCards.size());

            if (screen.inboxSelectedIndex >= totalCount) {
                screen.inboxSelectedIndex = std::max(0, totalCount - 1);
            }
            constexpr int visibleRows = 7;
            if (screen.inboxSelectedIndex < screen.inboxScrollOffset) {
                screen.inboxScrollOffset = screen.inboxSelectedIndex;
            } else if (screen.inboxSelectedIndex >= screen.inboxScrollOffset + visibleRows) {
                screen.inboxScrollOffset = screen.inboxSelectedIndex - visibleRows + 1;
            }

            // Header status
            std::string albumStr = Events::MysteryGiftInbox::isInboxSupported(screen.trainer)
                ? (std::to_string(totalCount) + " Wonder Card Slots in Save")
                : "No Mystery Gift Album Supported";
            int aw, ah; fb.measureText(albumStr, aw, ah, TextStyle::Body);
            fb.drawText(hX + hW - aw - 24, hY + (hH - ah) / 2, albumStr, Colors::TextDim, TextStyle::Body);

            // Left Column: Card Slots (x: 32, y: 86, w: 530, h: 560)
            const int lX = 32, lY = 86, lW = 530, lH = 560;
            fb.drawSoftShadow(lX, lY, lW, lH, 16);
            fb.drawFilledRoundedRect(lX, lY, lW, lH, 16, Colors::Panel);
            fb.drawRoundedRect(lX, lY, lW, lH, 16, Colors::Border, 1);

            const int rowH = 72, rowGap = 5;
            const int listTop = lY + 10;

            if (totalCount == 0) {
                fb.drawText(lX + 40, lY + 100, "No Mystery Gift cards are stored in this save.", Colors::TextDim, TextStyle::Body);
            } else {
                for (int r = 0; r < visibleRows; ++r) {
                    int itemIdx = screen.inboxScrollOffset + r;
                    if (itemIdx >= totalCount) break;

                    const auto& card = inboxCards[itemIdx];
                    const int ry = listTop + r * (rowH + rowGap);
                    const int rx = lX + 10, rw = lW - 20;
                    const bool focused = (itemIdx == screen.inboxSelectedIndex);

                    fb.drawFilledRoundedRect(rx, ry, rw, rowH, 12, focused ? Colors::Primary : Colors::PanelAlt);

                    // Icon disc
                    const int discR = 22;
                    const int dcx = rx + 30, dcy = ry + rowH / 2;
                    fb.drawFilledCircle(dcx, dcy, discR, focused ? Colors::AccentDim : Colors::Panel);

                    if (card.species > 0) {
                        Sprite* sp = SpriteManager::getIconSprite(card.species, 0, false);
                        if (sp && sp->data) {
                            const int sz = 36;
                            fb.drawSpriteFit(dcx - sz / 2, dcy - sz / 2, sz, sz,
                                             sp->width, sp->height, sp->data, sp->channels);
                        }
                    }

                    // Title
                    std::string cTitle = card.empty ? "(Empty Slot)" : (card.title.empty() ? ("Card #" + std::to_string(card.cardId)) : card.title);
                    fb.drawText(rx + 64, ry + 12, cTitle, focused ? Colors::PrimaryText : (card.empty ? Colors::TextDim : Colors::Text), TextStyle::Body);

                    // Subtext: Slot & Card ID
                    std::string sub = "Slot " + std::to_string(card.slotIndex + 1);
                    if (!card.empty) sub += "  |  ID: #" + std::to_string(card.cardId);
                    fb.drawText(rx + 64, ry + 42, sub, focused ? Colors::PrimaryText : Colors::TextDim, TextStyle::Caption);

                    // Status pill on right
                    if (!card.empty) {
                        std::string st = card.used ? "CLAIMED" : "AVAILABLE";
                        Color stCol = card.used ? Colors::Border : Colors::Green;
                        int bw, bh; fb.measureText(st, bw, bh, TextStyle::Caption);
                        const int bx = rx + rw - bw - 20, by = ry + 14;
                        fb.drawPill(bx, by, bw + 14, 22, stCol);
                        fb.drawText(bx + 7, by + 3, st, Colors::PrimaryText, TextStyle::Caption);
                    }

                    screen.touchButtons.push_back({ 5000 + itemIdx, rx, ry, rw, rowH });
                }

                // Scrollbar
                if (totalCount > visibleRows) {
                    const int sbX = lX + lW - 8, sbY = lY + 16, sbH = lH - 32;
                    fb.drawFilledRect(sbX, sbY, 4, sbH, Colors::Border);
                    const int thumbH = std::max(20, sbH * visibleRows / totalCount);
                    const int thumbY = sbY + (sbH - thumbH) * screen.inboxScrollOffset / (totalCount - visibleRows);
                    fb.drawFilledRoundedRect(sbX - 1, thumbY, 6, thumbH, 3, Colors::Primary);
                }
            }

            // Right Column: Card Details & Delivery Man Toggle (x: 580, y: 86, w: 668, h: 560)
            const int sX = 580, sY = 86, sW = 668, sH = 560;
            fb.drawSoftShadow(sX, sY, sW, sH, 16);
            fb.drawFilledRoundedRect(sX, sY, sW, sH, 16, Colors::Panel);
            fb.drawRoundedRect(sX, sY, sW, sH, 16, Colors::Border, 1);

            fb.drawCardBand(sX, sY, sW, 50);
            fb.drawText(sX + 22, fb.textYCenteredOn(sY + 28, TextStyle::Heading), "Mystery gift inbox", Colors::Text, TextStyle::Heading);

            // Selected Card Details
            const int detY = sY + 76;
            if (screen.inboxSelectedIndex >= 0 && screen.inboxSelectedIndex < totalCount) {
                const auto& sel = inboxCards[screen.inboxSelectedIndex];
                fb.drawFilledRoundedRect(sX + 24, detY, sW - 48, 220, 12, Colors::PanelAlt);

                if (!sel.empty) {
                    if (sel.species > 0) {
                        Sprite* sp = SpriteManager::getSprite(sel.species, 0, false);
                        if (!sp || !sp->data) sp = SpriteManager::getIconSprite(sel.species, 0, false);
                        if (sp && sp->data) {
                            fb.drawSpriteFit(sX + 44, detY + 20, 90, 90, sp->width, sp->height, sp->data, sp->channels);
                        }
                    }

                    const int cx = (sel.species > 0) ? (sX + 150) : (sX + 44);
                    fb.drawText(cx, detY + 20, sel.title.empty() ? ("Card #" + std::to_string(sel.cardId)) : sel.title, Colors::Text, TextStyle::Heading);
                    fb.drawText(cx, detY + 50, "Wonder Card ID: #" + std::to_string(sel.cardId), Colors::Primary, TextStyle::Body);
                    if (sel.species > 0) {
                        const char* sName = Trainer::getSpeciesName(sel.species);
                        fb.drawText(cx, detY + 76, "Species: " + std::string(sName ? sName : "Unknown"), Colors::TextDim, TextStyle::Body);
                        if (sel.level > 0) fb.drawText(cx, detY + 100, "Level: " + std::to_string(sel.level), Colors::TextDim, TextStyle::Body);
                    }
                    fb.drawText(cx, detY + 128, sel.receivedRecord ? "History entry" : (sel.used ? "Gift used" : "Gift unused"),
                                sel.used ? Colors::TextDim : Colors::Green, TextStyle::Body);
                } else {
                    fb.drawText(sX + 44, detY + 40, "(Empty Card Slot)", Colors::TextDim, TextStyle::Heading);
                    fb.drawText(sX + 44, detY + 74, "This album slot is currently empty.", Colors::TextDim, TextStyle::Body);
                }
            }

            fb.drawText(sX + 24, sY + sH - 50, "Read-only: PKSE does not alter cards or received flags.", Colors::TextDim, TextStyle::Caption);

            // Footer
            const int ftY = 660, ftH = 46;
            const char* inst = "ZL/ZR: Tab  |  Arrows: Move  |  B: Back";
            drawPopupHints(fb, hX, ftY, hW, ftH, inst);
            return;
        }

    }

} // namespace Panels
} // namespace UI