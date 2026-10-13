#include "UI/Dialogs/CatchDialog.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "Trainer/Trainer.h"
#include "UI/Common.h"
#include "UI/Dialogs/DialogFrame.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/ScreenChrome.h"
#include "UI/SpriteManager.h"
#include "UI/TrainerViewScreen.h"

namespace UI
{
    namespace Dialogs
    {
        void drawCatchChooser(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            auto &chooser = screen.catchChooser;
            screen.touchButtons.clear();
            constexpr int w = 1000, h = 580;
            const int x = (fb.getWidth() - w) / 2, y = (fb.getHeight() - h) / 2 - 6;
            const std::string name = Trainer::getSpeciesName(chooser.species);
            const int top = drawDialogFrame(fb, x, y, w, h, "How did you get " + name + "?", Colors::Text);

            // Left: the ways this game gives it, then "Custom".
            const int listX = x + 20, listW = 600, rowH = 62, listTop = top - 6, visible = 7;
            const int rows = static_cast<int>(chooser.options.size()) + 1;
            const int first = std::clamp(chooser.selected - visible / 2, 0, std::max(0, rows - visible));
            const float shift = easedListShift(fb, "catch.list", first, rowH);
            fb.setClipRect(listX - 4, listTop, listW + 8, visible * rowH);
            for (int i = std::max(0, first - 1); i < std::min(rows, first + visible + 1); ++i)
            {
                const int ry = listTop + (i - first) * rowH + static_cast<int>(std::lround(shift));
                const bool selected = i == chooser.selected;
                const bool custom = i >= static_cast<int>(chooser.options.size());
                if (selected)
                    fb.pkSelectedButton(listX, ry, listW, rowH - 6, 4);
                else
                    fb.drawFilledRoundedRect(listX, ry, listW, rowH - 6, 8, Colors::PanelAlt);
                std::string title, line;
                if (custom)
                {
                    title = "Custom";
                    line = "No catch history -- you set everything (may not be legal)";
                }
                else
                {
                    const auto &o = chooser.options[i];
                    title = o.title;
                    line = o.method;
                    if (o.origin != Legality::CatchOption::Origin::Hatched)
                        line += o.minMetLevel == o.maxMetLevel
                                    ? "  ·  Lv. " + std::to_string(o.minMetLevel)
                                    : "  ·  Lv. " + std::to_string(o.minMetLevel) + "-" + std::to_string(o.maxMetLevel);
                    if (!o.note.empty())
                        line += "  ·  " + o.note;
                }
                fb.drawText(listX + 16, ry + 6, title, Colors::Text, TextStyle::Body);
                const auto lines = wrapTextToWidth(fb, line, listW - 32, TextStyle::Caption);
                if (!lines.empty())
                    fb.drawText(listX + 16, ry + 32, lines[0] + (lines.size() > 1 ? "..." : ""), Colors::TextDim,
                                TextStyle::Caption);
                if (ry >= listTop && ry + rowH <= listTop + visible * rowH)
                    screen.touchButtons.push_back({7000 + i, listX, ry, listW, rowH - 6});
            }
            fb.clearClip();
            if (chooser.options.empty())
                fb.drawText(listX + 4, listTop + rowH + 10,
                            "This game has no catch, gift or Egg for " + name + ".", Colors::TextDim, TextStyle::Caption);

            // Right: the chosen one, and what it will be.
            const int px = x + 650, pw = w - 650 - 20;
            if (Sprite *sprite = SpriteManager::getSprite(chooser.species, chooser.form, chooser.shiny); sprite && sprite->data)
                fb.drawSpriteFit(px + (pw - 150) / 2, listTop, 150, 150, sprite->width, sprite->height, sprite->data,
                                 sprite->channels);
            int ty = listTop + 166;
            if (chooser.pickVersion)
            {
                std::string game = "?";
                for (const auto &choice : Legality::gameChoicesForGroup(screen.trainer.getGameGroup()))
                    if (choice.version == chooser.version)
                        game = choice.name;
                fb.drawText(px, ty, "Game", Colors::TextDim, TextStyle::Caption);
                fb.drawText(px + 120, ty - 2, game + "  (Y)", Colors::Accent, TextStyle::Body);
                ty += 34;
            }
            auto row = [&](const std::string &label, const std::string &value, Color color = Colors::Text) {
                fb.drawText(px, ty, label, Colors::TextDim, TextStyle::Caption);
                fb.drawText(px + 120, ty - 2, value, color, TextStyle::Body);
                ty += 34;
            };
            if (chooser.selected < static_cast<int>(chooser.options.size()))
            {
                const auto &o = chooser.options[chooser.selected];
                const bool hatched = o.origin == Legality::CatchOption::Origin::Hatched;
                const uint8_t now = std::max<uint8_t>(std::max(chooser.metLevel, o.minLevel), chooser.level);
                if (hatched)
                    row("Hatched at", "Lv. " + std::to_string(o.minMetLevel));
                else
                    row("Met at", (chooser.metLevel > o.minMetLevel ? "< " : "  ") + std::string("Lv. ") +
                                      std::to_string(chooser.metLevel) + (chooser.metLevel < o.maxMetLevel ? " >" : ""));
                row("Level now", "Lv. " + std::to_string(now) + "  (L/R)" +
                                     (o.origin == Legality::CatchOption::Origin::Evolved && now == o.minLevel ? "  evolved" : ""));
                row("Ball", !hatched && o.row.fixedBall ? std::string("Its own (fixed)") : std::string("Poké Ball"));
                const bool shinyNow = o.shinyForced || (chooser.shiny && o.shinyAllowed);
                row("Shiny", !o.shinyAllowed ? "Never (locked)" : o.shinyForced ? "Always" : shinyNow ? "Yes  (X)" : "No  (X)",
                    shinyNow ? Colors::ShinyStar : Colors::Text);
                ty += 8;
                const std::string about =
                    "Knows the moves one at that level would. Raise the level, IVs and the rest in the editor after.";
                for (const auto &l : wrapTextToWidth(fb, about, pw, TextStyle::Caption))
                {
                    fb.drawText(px, ty, l, Colors::TextDim, TextStyle::Caption);
                    ty += 20;
                }
            }
            else
            {
                for (const auto &l : wrapTextToWidth(fb, "A level-1 Pokémon met here today, to edit freely. Use it for "
                                                         "event-only Pokémon this game has no encounter for.",
                                                     pw, TextStyle::Caption))
                {
                    fb.drawText(px, ty, l, Colors::TextDim, TextStyle::Caption);
                    ty += 20;
                }
            }
        }
    }
}
