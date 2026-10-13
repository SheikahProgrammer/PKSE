#ifndef UI_SCREEN_CHROME_H
#define UI_SCREEN_CHROME_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "UI/PKSEFramebuffer.h"
#include "UI/Common.h"
#include "UI/TouchInput.h"

namespace UI
{

    constexpr int CHROME_CORNER_RADIUS = 18; // curve on the content-facing edge of both bars
    constexpr int HEADER_HEIGHT = 64;
    constexpr int NAV_BAR_HEIGHT = 46;

    // Draw (or, with measureOnly, just measure) one controller badge, shaped like the real button:
    // face buttons are round, shoulders are rounded rectangles, +/- are round with a drawn bar, and
    // the d-pad is a cross with the unused axis dimmed. `cy` is the badge's vertical CENTRE.
    // Returns the width consumed, or 0 if the token isn't a button PKSE knows how to draw.
    inline int buttonGlyph(PKSEFramebuffer &framebuffer, int badgeX, int centerY, const std::string &buttonToken,
                           bool measureOnly)
    {
        // Mid-grey badge with the bar colour as ink. Both are theme colours, so the pair inverts
        // itself: dark letter on light grey in the dark theme, white letter on grey in the light one.
        // PKForge (PksmPaint.HintBar): LogoCyan key discs with LogoVoid ink.
        const Color fillColor = isPkForgeTheme() ? Pksm::LogoCyan : Colors::TextDim;
        const Color inkColor = isPkForgeTheme() ? Pksm::LogoVoid : Colors::Panel;
        constexpr int faceButtonRadius = 12; // face-button radius

        auto centred = [&](const std::string &s, int boxX, int boxWidth)
        {
            int textWidth, th;
            framebuffer.measureText(s, textWidth, th, TextStyle::Caption);
            framebuffer.drawText(boxX + (boxWidth - textWidth) / 2, centerY - th / 2, s, inkColor, TextStyle::Caption);
        };

        // Face buttons.
        if (buttonToken.size() == 1 &&
            (buttonToken[0] == 'A' || buttonToken[0] == 'B' || buttonToken[0] == 'X' || buttonToken[0] == 'Y'))
        {
            if (!measureOnly)
            {
                framebuffer.drawFilledCircle(badgeX + faceButtonRadius, centerY, faceButtonRadius, fillColor);
                centred(buttonToken, badgeX, faceButtonRadius * 2);
            }
            return faceButtonRadius * 2;
        }

        // Plus / Minus. The bars are drawn rather than typed -- Nunito's '+' and '-' are far too
        // thin to read at badge size, and '-' sits at x-height instead of centred.
        if (buttonToken == "+" || buttonToken == "Plus" || buttonToken == "-" || buttonToken == "Minus")
        {
            if (!measureOnly)
            {
                framebuffer.drawFilledCircle(badgeX + faceButtonRadius, centerY, faceButtonRadius, fillColor);
                framebuffer.drawFilledRoundedRect(badgeX + faceButtonRadius - 6, centerY - 1, 13, 3, 1, inkColor);
                if (buttonToken == "+" || buttonToken == "Plus")
                    framebuffer.drawFilledRoundedRect(badgeX + faceButtonRadius - 1, centerY - 6, 3, 13, 1, inkColor);
            }
            return faceButtonRadius * 2;
        }

        // Shoulders, and the slash pairs the hints use ("L/R", "ZL/ZR"): rounded rects sized to text.
        if (buttonToken == "L" || buttonToken == "R" || buttonToken == "ZL" || buttonToken == "ZR" ||
            buttonToken == "L/R" || buttonToken == "ZL/ZR")
        {
            int textWidth, th;
            framebuffer.measureText(buttonToken, textWidth, th, TextStyle::Caption);
            const int badgeWidth = textWidth + 14, h = 22;
            if (!measureOnly)
            {
                framebuffer.drawFilledRoundedRect(badgeX, centerY - h / 2, badgeWidth, h, 7, fillColor);
                centred(buttonToken, badgeX, badgeWidth);
            }
            return badgeWidth;
        }

        // D-pad. "Up/Down" and "Left/Right" dim the axis they don't use, so the badge itself says
        // which way the stick moves rather than relying on the label to explain it.
        if (buttonToken == "Arrows" || buttonToken == "D-Pad" || buttonToken == "Up/Down" ||
            buttonToken == "Left/Right")
        {
            constexpr int crossSize = 24, armThickness = 9; // across the cross, and one arm's thickness
            if (!measureOnly)
            {
                const Color dim(fillColor.red, fillColor.green, fillColor.blue, 70);
                const bool verticalOnly = (buttonToken == "Up/Down"), hOnly = (buttonToken == "Left/Right");
                const int verticalArmX = badgeX + (crossSize - armThickness) / 2, vy = centerY - crossSize / 2;
                const int horizontalArmX = badgeX, hy = centerY - armThickness / 2;
                // Dim arm first, so the solid one wins where they overlap in the middle.
                if (verticalOnly)
                {
                    framebuffer.drawFilledRoundedRect(horizontalArmX, hy, crossSize, armThickness, 3, dim);
                    framebuffer.drawFilledRoundedRect(verticalArmX, vy, armThickness, crossSize, 3, fillColor);
                }
                else if (hOnly)
                {
                    framebuffer.drawFilledRoundedRect(verticalArmX, vy, armThickness, crossSize, 3, dim);
                    framebuffer.drawFilledRoundedRect(horizontalArmX, hy, crossSize, armThickness, 3, fillColor);
                }
                else
                {
                    framebuffer.drawFilledRoundedRect(verticalArmX, vy, armThickness, crossSize, 3, fillColor);
                    framebuffer.drawFilledRoundedRect(horizontalArmX, hy, crossSize, armThickness, 3, fillColor);
                }
            }
            return crossSize;
        }

        // A single d-pad direction: a rounded-square badge with a geometric triangle arrow. Used by
        // the edit dialogs' -1 / +1 steps (the shoulders take the +/-10 and +/-100 steps).
        if (buttonToken == "Left" || buttonToken == "Right" || buttonToken == "Up" || buttonToken == "Down")
        {
            constexpr int badgeSize = 22;
            if (!measureOnly)
            {
                framebuffer.drawFilledRoundedRect(badgeX, centerY - badgeSize / 2, badgeSize, badgeSize, 6, fillColor);
                const std::string arrowGlyph = buttonToken == "Left"    ? "\xE2\x97\x80"  // ◀
                                        : buttonToken == "Right" ? "\xE2\x96\xB6"  // ▶
                                        : buttonToken == "Up"    ? "\xE2\x96\xB2"  // ▲
                                                         : "\xE2\x96\xBC"; // ▼
                // Measure AND draw the arrow at Caption size -- drawSymbol otherwise defaults to Body,
                // which overflowed this 22px badge and mis-centred the glyph (the ◀ was clipping away).
                int textWidth, th;
                framebuffer.measureText(arrowGlyph, textWidth, th, TextStyle::Caption);
                framebuffer.drawSymbol(badgeX + (badgeSize - textWidth) / 2, centerY - th / 2, arrowGlyph, inkColor,
                                       TextStyle::Caption);
            }
            return badgeSize;
        }
        return 0; // not a button we have a badge for
    }

    inline int buttonGlyphWidth(PKSEFramebuffer &framebuffer, const std::string &buttonToken)
    {
        return buttonGlyph(framebuffer, 0, 0, buttonToken, true);
    }

    // A pressable button that carries its controller badge ON the button (glyph + label, centred),
    // instead of relying on a separate "A: Confirm" guide line below it. `fill` lets destructive
    // actions stay red; `textColor` keeps the label legible on that fill. Screen-independent (does
    // NOT register a touch target) so any screen can use it and wire its own hit region --
    // drawEditChoiceButton wraps this for the TrainerViewScreen dialogs.
    //
    // `pressed` draws the button HELD: an accent ring and an accent tint over the caller's fill. A
    // tapped dialog button has no cursor resting on it the way a menu row does, so without this the
    // frame between a touch and the action it stands for would show nothing at all -- and that frame
    // now always exists, because a tap is deferred by one (see TrainerViewScreen::armTap). The ring
    // is drawn INSIDE the button's own rectangle, so a held button occupies exactly the space an
    // unheld one does and nothing around it shifts. The tint goes OVER the caller's fill rather than
    // replacing it, so a destructive button stays recognisably red while it is held.
    inline void drawGlyphButton(PKSEFramebuffer &framebuffer, int boxX, int boxY, int boxWidth, int boxHeight,
                                const std::string &glyph, const std::string &label,
                                Color fillColor = Colors::PanelAlt, Color textColor = Colors::Text,
                                bool pressed = false)
    {
        framebuffer.endTyping(); // a dialog's buttons wait for its message (see beginTyping)
        if (isPkForgeTheme())
        {
            // A destructive action keeps its red as the selected-button accent (UiTokens.AccentDanger).
            const bool danger = fillColor.red > 150 && fillColor.green < 110;
            if (pressed)
                framebuffer.pkSelectedButton(boxX, boxY, boxWidth, boxHeight, 4,
                                             danger ? Color(0xA6, 0x52, 0x63) : Pksm::HeaderBlue);
            else
                framebuffer.pkBlackButton(boxX, boxY, boxWidth, boxHeight, 4);
            if (danger && !pressed)
                framebuffer.drawRoundedRect(boxX + 1, boxY + 1, boxWidth - 2, boxHeight - 2, 4, Pksm::Illegal, 2);
            const int glyphWidth = buttonGlyphWidth(framebuffer, glyph);
            int labelWidth, lh;
            framebuffer.measureText(label, labelWidth, lh);
            const int glyphX = boxX + (boxWidth - (glyphWidth + 10 + labelWidth)) / 2;
            buttonGlyph(framebuffer, glyphX, boxY + boxHeight / 2, glyph, false);
            framebuffer.drawText(glyphX + glyphWidth + 10, framebuffer.textYCenteredOn(boxY + boxHeight / 2), label,
                                 Pksm::Ink);
            return;
        }
        framebuffer.drawFilledRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, fillColor);
        if (pressed)
        {
            Color pressTint = Colors::Primary;
            pressTint.alpha = 110;
            framebuffer.drawFilledRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, pressTint);
            framebuffer.drawRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, Colors::Primary, 3);
        }
        else
        {
            framebuffer.drawRoundedRect(boxX, boxY, boxWidth, boxHeight, 8, Colors::Border, 1);
        }
        const int glyphWidth = buttonGlyphWidth(framebuffer, glyph);
        int labelWidth, lh;
        framebuffer.measureText(label, labelWidth, lh);
        const int glyphX = boxX + (boxWidth - (glyphWidth + 10 + labelWidth)) / 2;
        buttonGlyph(framebuffer, glyphX, boxY + boxHeight / 2, glyph, false);
        framebuffer.drawText(glyphX + glyphWidth + 10, boxY + (boxHeight - lh) / 2, label, textColor);
    }

    //
    // Every screen already publishes a contextual hint string, so making the badges tappable gives
    // HOME-style on-screen action buttons everywhere at once. A tap resolves to that button's press
    // and the screen ORs it into padGetButtonsDown, so no existing handler changes -- there is one
    // input path, not two, and a tap can never diverge from what the physical button does.
    //
    // Only badges standing for exactly ONE button get a hit region. "L/R", "ZL/ZR" and the d-pad stay
    // informational: a single badge can't say whether you meant L or R, and splitting one in half
    // would leave each half far below a usable touch target.
    struct NavHit
    {
        int hitX, hitY, hitWidth, hitHeight;
        uint64_t button;
    };
    inline std::vector<NavHit> g_navHits;

    inline uint64_t navButtonFor(const std::string &buttonToken)
    {
        if (buttonToken == "A")
            return HidNpadButton_A;
        if (buttonToken == "B")
            return HidNpadButton_B;
        if (buttonToken == "X")
            return HidNpadButton_X;
        if (buttonToken == "Y")
            return HidNpadButton_Y;
        if (buttonToken == "+" || buttonToken == "Plus")
            return HidNpadButton_Plus;
        if (buttonToken == "-" || buttonToken == "Minus")
            return HidNpadButton_Minus;
        if (buttonToken == "L")
            return HidNpadButton_L;
        if (buttonToken == "R")
            return HidNpadButton_R;
        if (buttonToken == "ZL")
            return HidNpadButton_ZL;
        if (buttonToken == "ZR")
            return HidNpadButton_ZR;
        return 0; // multi-button or directional badge: informational only
    }

    // Hit-test a fresh tap against the badges captured during the PREVIOUS frame's draw (same
    // one-frame-late contract as TrainerViewScreen::touchedButtonId). Returns a mask to fold into
    // buttonsDown, or 0. Edge-triggered, so it behaves exactly like padGetButtonsDown.
    inline uint64_t navTouchButton(const TouchInput &touch)
    {
        if (!touch.justPressed())
            return 0;
        for (const NavHit &hit : g_navHits)
        {
            if (touch.x() >= hit.hitX && touch.x() < hit.hitX + hit.hitWidth &&
                touch.y() >= hit.hitY && touch.y() < hit.hitY + hit.hitHeight)
                return hit.button;
        }
        return 0;
    }

    /**
     * Breaks `text` into lines no wider than `maxLineWidth` when drawn in `style`: at spaces where it
     * can, and between characters where one word is wider than the whole line.
     *
     * MEASURED, NOT COUNTED. A character budget is wrong twice over: Nunito is proportional, so seven
     * narrow letters and seven wide ones differ by half their width, and a Japanese nickname or place
     * name has no spaces to break at at all. Breaking inside a word is the last resort, and it cuts on
     * a UTF-8 boundary -- never through a multi-byte character, which would draw as nothing.
     *
     * A run of spaces collapses to the one break it stands for; the text has no other whitespace the
     * layout should keep.
     */
    inline std::vector<std::string> wrapTextToWidth(PKSEFramebuffer &framebuffer, const std::string &text,
                                                    int maxLineWidth, TextStyle style)
    {
        const auto fits = [&](const std::string &candidate)
        {
            int candidateWidth = 0, candidateHeight = 0;
            framebuffer.measureText(candidate, candidateWidth, candidateHeight, style);
            return candidateWidth <= maxLineWidth;
        };
        // The byte just past the UTF-8 character that starts at `characterStart`.
        const auto nextCharacterEnd = [](const std::string &word, size_t characterStart)
        {
            size_t characterEnd = characterStart + 1;
            while (characterEnd < word.size() && (static_cast<unsigned char>(word[characterEnd]) & 0xC0) == 0x80)
                ++characterEnd;
            return characterEnd;
        };

        std::vector<std::string> lines;
        std::string currentLine;
        size_t wordStart = 0;
        while (wordStart < text.size())
        {
            size_t wordEnd = text.find(' ', wordStart);
            if (wordEnd == std::string::npos)
                wordEnd = text.size();
            std::string word = text.substr(wordStart, wordEnd - wordStart);
            wordStart = wordEnd + 1;
            if (word.empty())
                continue;

            const std::string joined = currentLine.empty() ? word : currentLine + " " + word;
            if (fits(joined))
            {
                currentLine = joined;
                continue;
            }
            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine.clear();
            }
            // A WORD WIDER THAN THE WHOLE LINE. Take the longest prefix that fits, one character at a
            // time; if not even one character fits, take it anyway, since a line that never advances
            // would loop forever.
            while (!fits(word))
            {
                size_t cut = 0;
                while (cut < word.size())
                {
                    const size_t candidateCut = nextCharacterEnd(word, cut);
                    if (!fits(word.substr(0, candidateCut)))
                        break;
                    cut = candidateCut;
                }
                if (cut == 0)
                    cut = nextCharacterEnd(word, 0);
                lines.push_back(word.substr(0, cut));
                word.erase(0, cut);
            }
            currentLine = word;
        }
        if (!currentLine.empty())
            lines.push_back(currentLine);
        return lines;
    }

    // Lay out a "Btn: Label  |  Btn: Label" hint as badge+label pairs, centred within [x, x+w] on
    // `cy`. A segment with no colon (e.g. "HOLDING") is a state marker and renders as accent text.
    // Shared by the screen nav bar and the dialog footer so the two always match.
    inline void drawNavHints(PKSEFramebuffer &framebuffer, int hintsX, int hintsWidth, int centerY,
                             const std::string &hint)
    {
        struct HintSegment
        {
            std::string buttonToken, label;
            int glyphWidth, labelWidth;
        };

        // Whoever draws last owns the taps, so an open modal's footer replaces the nav bar behind it
        // rather than leaving the background live. Clearing here (not in drawNavBar) also means the
        // list can't grow across frames if a screen ever draws a footer without a nav bar.
        g_navHits.clear();

        auto trim = [](const std::string &s)
        {
            const size_t firstNonSpace = s.find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos)
                return std::string();
            return s.substr(firstNonSpace, s.find_last_not_of(" \t") - firstNonSpace + 1);
        };

        std::vector<HintSegment> segments;
        for (size_t hintIndex = 0; hintIndex <= hint.size();)
        {
            const size_t barPosition = hint.find('|', hintIndex);
            const std::string token = trim(
                hint.substr(hintIndex, barPosition == std::string::npos ? std::string::npos : barPosition - hintIndex));
            if (!token.empty())
            {
                HintSegment segment{};
                const size_t colon = token.find(':');
                if (colon == std::string::npos)
                {
                    segment.label = token;
                }
                else
                {
                    segment.buttonToken = trim(token.substr(0, colon));
                    segment.label = trim(token.substr(colon + 1));
                }
                segment.glyphWidth =
                    segment.buttonToken.empty() ? 0 : buttonGlyphWidth(framebuffer, segment.buttonToken);
                // A button we have no badge for still has to be readable: fall back to plain text
                // rather than silently dropping the button name and leaving a bare verb.
                if (segment.glyphWidth == 0 && !segment.buttonToken.empty())
                    segment.label = segment.buttonToken + ": " + segment.label;
                int textHeight;
                framebuffer.measureText(segment.label, segment.labelWidth, textHeight, TextStyle::Caption);
                segments.push_back(segment);
            }
            if (barPosition == std::string::npos)
                break;
            hintIndex = barPosition + 1;
        }
        if (segments.empty())
            return;

        constexpr int glyphGap = 8, maxItemGap = 26, minItemGap = 10, horizontalPadding = 20;
        int fixed = 0;
        for (const HintSegment &segment : segments)
            fixed += segment.glyphWidth + (segment.glyphWidth ? glyphGap : 0) + segment.labelWidth;

        const int count = static_cast<int>(segments.size());
        int itemGap = maxItemGap;
        if (count > 1)
            itemGap = std::clamp((hintsWidth - horizontalPadding * 2 - fixed) / (count - 1), minItemGap, maxItemGap);

        int centerX = hintsX + std::max(horizontalPadding, (hintsWidth - (fixed + itemGap * (count - 1))) / 2);
        for (const HintSegment &segment : segments)
        {
            const int segmentX = centerX;
            if (segment.glyphWidth)
            {
                buttonGlyph(framebuffer, centerX, centerY, segment.buttonToken, false);
                centerX += segment.glyphWidth + glyphGap;
            }
            int textWidth, textHeight;
            framebuffer.measureText(segment.label, textWidth, textHeight, TextStyle::Caption);
            framebuffer.drawText(centerX, centerY - textHeight / 2, segment.label,
                                 segment.glyphWidth ? (isPkForgeTheme() ? Pksm::Ink : Colors::Text) : Colors::Accent,
                                 TextStyle::Caption);
            centerX += segment.labelWidth;
            // The badge AND its label are one tap target -- aiming at a 24px circle is unreasonable,
            // and the label is the part that says what will happen. Height is TouchTargetMin rather
            // than the bar height (46), so the target stays fingertip-sized.
            const uint64_t button = segment.glyphWidth ? navButtonFor(segment.buttonToken) : 0;
            if (button)
                g_navHits.push_back(
                    {segmentX, centerY - TouchTargetMin / 2, centerX - segmentX, TouchTargetMin, button});
            centerX += itemGap;
        }
    }

    // PKForge DsChrome: the dark title rail carrying the centred wordmark over a cyan rule.
    constexpr int PK_TITLE_RAIL_HEIGHT = 30;

    // Battery level for the status strip, refreshed every few seconds (-1 = unknown, drawn full).
    int statusBatteryPercent();

    // Top chrome. PKForge theme: DsChrome.TitleBar ("PKSE" centred on the void rail, cyan rule)
    // over DsChrome.StatusStrip (the save on the left, status words after a dot, battery right).
    // Other themes: the "PKSE" + subtitle sheet that curves along its bottom edge.
    inline void drawTitleBar(PKSEFramebuffer &framebuffer, const std::string &subtitle,
                             const std::string &status = "")
    {
        const int screenWidth = framebuffer.getWidth();
        if (isPkForgeTheme())
        {
            framebuffer.drawFilledRect(0, 0, screenWidth, PK_TITLE_RAIL_HEIGHT, Pksm::LogoVoid);
            framebuffer.drawFilledRect(20, PK_TITLE_RAIL_HEIGHT - 3, screenWidth - 40, 2, Pksm::LogoCyan);
            int tw, th;
            framebuffer.measureText("PKSE", tw, th, TextStyle::Body);
            framebuffer.drawText((screenWidth - tw) / 2, framebuffer.textYCenteredOn(PK_TITLE_RAIL_HEIGHT / 2 - 2),
                                 "PKSE", Color(255, 255, 255));

            const int stripY = PK_TITLE_RAIL_HEIGHT, stripH = HEADER_HEIGHT - PK_TITLE_RAIL_HEIGHT;
            framebuffer.drawFilledRect(0, stripY, screenWidth, stripH, Pksm::Paper);
            const int midY = stripY + stripH / 2;
            int x = 16;
            if (!subtitle.empty())
            {
                framebuffer.drawText(x, framebuffer.textYCenteredOn(midY), subtitle, Pksm::Ink);
                int sw, sh;
                framebuffer.measureText(subtitle, sw, sh);
                x += sw + 15;
            }
            if (!status.empty())
            {
                framebuffer.drawFilledRoundedRect(x, midY - 3, 6, 6, 1, Pksm::InkSoft);
                x += 6 + 15;
                framebuffer.drawText(x, framebuffer.textYCenteredOn(midY), status, Pksm::InkSoft);
            }
            framebuffer.pkBattery(screenWidth - 16 - 24, midY - 6, statusBatteryPercent());
            return;
        }
        framebuffer.drawSoftShadow(0, -40, screenWidth, HEADER_HEIGHT + 40, CHROME_CORNER_RADIUS);
        framebuffer.drawFilledRoundedRect(0, -CHROME_CORNER_RADIUS, screenWidth,
                                          HEADER_HEIGHT + CHROME_CORNER_RADIUS, CHROME_CORNER_RADIUS, Colors::Panel);
        framebuffer.drawText(20, 8, "PKSE", Colors::Accent, TextStyle::Title);
        int boxWidth, bh;
        framebuffer.measureText("PKSE", boxWidth, bh, TextStyle::Title);
        // Short accent underline beneath the wordmark. A full-width rule cannot work against a curved
        // sheet: a straight line would cut across the corners.
        framebuffer.drawFilledRoundedRect(20, 52, boxWidth, 3, 2, Colors::Accent);
        std::string line = subtitle;
        if (!status.empty())
            line += (line.empty() ? "" : "   ") + status;
        if (!line.empty())
            framebuffer.drawText(20 + boxWidth + 16, 24, line, Colors::TextDim, TextStyle::Body);
    }

    // Bottom nav bar: a sheet that curves along its top edge, carrying the controller badges.
    inline void drawNavBar(PKSEFramebuffer &framebuffer, const std::string &hint)
    {
        const int screenWidth = framebuffer.getWidth(), barY = framebuffer.getHeight() - NAV_BAR_HEIGHT;
        if (isPkForgeTheme())
        {
            // PksmPaint.HintBar: a device panel (inset 3) carrying the cyan key discs and pale labels.
            framebuffer.pkPanel(3, barY + 1, screenWidth - 9, NAV_BAR_HEIGHT - 6);
            drawNavHints(framebuffer, 0, screenWidth, barY + NAV_BAR_HEIGHT / 2 - 1, hint);
            return;
        }
        framebuffer.drawSoftShadow(0, barY, screenWidth, NAV_BAR_HEIGHT + 40, CHROME_CORNER_RADIUS);
        framebuffer.drawFilledRoundedRect(0, barY, screenWidth, NAV_BAR_HEIGHT + CHROME_CORNER_RADIUS,
                                          CHROME_CORNER_RADIUS, Colors::Panel);
        drawNavHints(framebuffer, 0, screenWidth, barY + NAV_BAR_HEIGHT / 2, hint);
    }

    // PKForge popups (Living Dex, Mystery Gift, editors) sit over the dimmed screen as one device
    // panel. Draws the scrim + panel in the PKForge theme; other themes keep their full-screen look.
    inline void drawPopupFrame(PKSEFramebuffer &framebuffer, Color body = Pksm::Paper)
    {
        if (!isPkForgeTheme())
            return;
        framebuffer.drawFilledRect(0, 0, framebuffer.getWidth(), framebuffer.getHeight(), Color(0x14, 0x12, 0x1D, 0xCC));
        framebuffer.pkPanel(12, 6, framebuffer.getWidth() - 24, framebuffer.getHeight() - 14, body);
    }

    // A popup's own hint bar: PKForge's HintBar panel with cyan key discs; elsewhere the plain
    // caption line these popups always had. `hint` uses the nav bar's "Btn: Label | ..." format.
    inline void drawPopupHints(PKSEFramebuffer &framebuffer, int barX, int barY, int barWidth, int barHeight,
                               const std::string &hint)
    {
        if (isPkForgeTheme())
        {
            framebuffer.pkPanel(barX, barY, barWidth, barHeight);
            drawNavHints(framebuffer, barX, barWidth, barY + barHeight / 2, hint);
            return;
        }
        framebuffer.drawFilledRoundedRect(barX, barY, barWidth, barHeight, 12, Colors::Panel);
        framebuffer.drawRoundedRect(barX, barY, barWidth, barHeight, 12, Colors::Border, 1);
        int iw, ih;
        framebuffer.measureText(hint, iw, ih, TextStyle::Caption);
        framebuffer.drawText(barX + (barWidth - iw) / 2, barY + (barHeight - ih) / 2, hint, Colors::TextDim,
                             TextStyle::Caption);
    }

    // A HOME-style selectable list tile: rounded (stadium), soft shadow, amber when
    // selected. `accent` marks a special/primary row (indigo-tinted when not selected, e.g. the
    // "Create New Backup" action); `enabled` false dims it (e.g. a "no saves" placeholder).
    inline void drawHomeTile(PKSEFramebuffer &framebuffer, int homeTileX, int homeTileY, int homeTileWidth,
                             int homeTileHeight, const std::string &label, bool selected, bool accent = false,
                             bool enabled = true)
    {
        if (isPkForgeTheme())
        {
            // DsFolderButton: the resting menu button, or the cobalt selected look.
            if (selected)
                framebuffer.pkSelectedButton(homeTileX, homeTileY, homeTileWidth, homeTileHeight, 4);
            else
                framebuffer.pkBlackButton(homeTileX, homeTileY, homeTileWidth, homeTileHeight, 4);
            const Color ink = !enabled ? Pksm::InkSoft : (accent && !selected) ? Pksm::LogoCyan : Pksm::Ink;
            framebuffer.drawText(homeTileX + 30, framebuffer.textYCenteredOn(homeTileY + homeTileHeight / 2), label, ink);
            return;
        }
        framebuffer.drawSoftShadow(homeTileX, homeTileY, homeTileWidth, homeTileHeight, homeTileHeight / 2);
        const Color fillColor = selected ? Colors::Primary : Colors::PanelAlt;
        framebuffer.drawPill(homeTileX, homeTileY, homeTileWidth, homeTileHeight, fillColor);
        // An accent (primary-action) row is a normal tile with accent text + a thin accent outline —
        // NOT a filled highlight, which would read as a false selection next to the real (amber) one.
        if (accent && !selected)
            framebuffer.drawPillBorder(homeTileX, homeTileY, homeTileWidth, homeTileHeight, Colors::Accent, 2);
        const Color textColor2 = !enabled   ? Colors::TextDim
                          : selected ? Colors::PrimaryText
                          : accent   ? Colors::Accent
                                     : Colors::Text;
        int labelX = homeTileX + 28;
        if (selected)
        {
            framebuffer.drawSymbol(homeTileX + 20, homeTileY + homeTileHeight / 2 - 12, "\xE2\x96\xB6",
                                   Colors::PrimaryText);
            labelX = homeTileX + 48;
        }
        int labelWidth, lh;
        framebuffer.measureText(label, labelWidth, lh, TextStyle::Body);
        framebuffer.drawText(labelX, homeTileY + (homeTileHeight - lh) / 2, label, textColor2, TextStyle::Body);
    }

    // A thin scrollbar thumb on a scrolling viewport's right edge, drawn ONLY when the content
    // overflows. All pixels: `x` is the thumb's left edge, [trackY, trackY + trackH] the viewport,
    // contentH the full content height, scroll the current offset. Item lists pass contentH =
    // totalItems * rowH and scroll = firstItem * rowH. One helper so every scrolling surface in the
    // app gets the same thumb the details editor uses.
    /**
     * Smooth scrolling for a list drawn a row at a time from `first`.
     *
     * The list's window eases to `first` on a spring (MonSummaryView's 180ms CubicOut feel) instead
     * of jumping a row per press. Returns the pixels to ADD to every row's y this frame. Draw rows
     * first-1 .. first+visible, clipped to the list, so neither edge shows a gap while it moves --
     * and register tap targets only for rows fully inside it.
     */
    inline float easedListShift(PKSEFramebuffer &framebuffer, const char *id, int first, int pitch)
    {
        const float target = static_cast<float>(first * pitch);
        return target - framebuffer.spring(id, target, 600.0f, 49.0f, target);
    }

    /// The same for a paged view: pixels to add to x this frame, so a new page slides in from the
    /// side it was paged toward rather than replacing the old one in place.
    inline float easedPageShift(PKSEFramebuffer &framebuffer, const char *id, int page, int width)
    {
        const float target = static_cast<float>(page * width);
        return (target - framebuffer.spring(id, target, 420.0f, 41.0f, target)) * 0.35f;
    }

    inline void drawScrollbar(PKSEFramebuffer &framebuffer, int trackX, int trackY, int trackH, int contentH,
                              int scroll)
    {
        if (contentH <= trackH || trackH <= 0)
            return;
        const int maxS = contentH - trackH;
        int scrollOffset = scroll;
        if (scrollOffset < 0)
            scrollOffset = 0;
        if (scrollOffset > maxS)
            scrollOffset = maxS;
        const int thumbH = std::max(24, trackH * trackH / contentH);
        const int thumbY = trackY + (trackH - thumbH) * scrollOffset / maxS;
        framebuffer.drawFilledRoundedRect(trackX, thumbY, 3, thumbH, 2, Colors::Border);
    }
}

#endif
