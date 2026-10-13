#ifndef UI_SETTINGS_VIEW_H
#define UI_SETTINGS_VIEW_H

#include <string>
#include <vector>

#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"

namespace UI
{
    /**
     * The app's settings list: every row is a global (settings.cfg), none belongs to one game, so
     * it lives on the top-level menu beside the Poképark rather than inside a save.
     *
     * The row numbers below are the ONE list read by both draw() (labels, values, badges) and
     * activate() (what A does). Private copies of these numbers drifted apart once before, and the
     * badges then lit for their neighbour's setting. Add a row here and in draw()'s tables together.
     */
    namespace SettingsView
    {
        constexpr int ROW_AUTO_BACKUP = 0;
        constexpr int ROW_THEME = 1;
        constexpr int ROW_SPRITE_STYLE = 2;
        constexpr int ROW_SD_OVERRIDE = 3;
        constexpr int ROW_TEXT_SPEED = 4;
        constexpr int ROW_ALLOW_ILLEGAL = 5;
        constexpr int ROW_AUTO_LEGALIZE = 6;
        constexpr int ROW_MOVE_WARNING = 7;
        constexpr int ROW_INJECT_TO_GAME = 8;
        constexpr int ROW_DEBUG_LOGGING = 9;
        constexpr int ROW_LANGUAGE = 10; // must stay LAST: hiding it trims the end of the table
        constexpr int ROW_COUNT = 11;

        /// Rows actually shown (the Language row is hidden until it is selectable).
        int visibleRowCount();

        /// Draws the list into the box (x, y, w, h); each row's tap target goes into `touch` with the
        /// row index as its id.
        void draw(PKSEFramebuffer &framebuffer, int x, int y, int w, int h, int selectedRow,
                  std::vector<TouchButton> &touch);

        /// Toggles / cycles `row`, saves settings.cfg, and returns a line to tell the user (may be empty).
        std::string activate(int row);
    }
}

#endif
