#include "UI/SettingsView.h"

#include "Globals.h"
#include "Names/NameLanguage.h"
#include "UI/SpriteManager.h"
#include "Utils/Settings.h"

namespace UI
{
    namespace SettingsView
    {
        static_assert(ROW_LANGUAGE == ROW_COUNT - 1,
                      "Hiding the Language row works by trimming the LAST row -- a row added after it "
                      "would be hidden instead. Move Language back to the end, or hide it by index.");

        int visibleRowCount() { return Names::DISPLAY_LANGUAGE_SELECTABLE ? ROW_COUNT : ROW_LANGUAGE; }

        static const char *textSpeedName(int speed)
        {
            switch (speed)
            {
            case 0: return "Slow";
            case 2: return "Fast";
            case 3: return "Instant";
            default: return "Mid";
            }
        }

        void draw(PKSEFramebuffer &framebuffer, int x, int y, int w, int h, int selectedRow,
                  std::vector<TouchButton> &touch)
        {
            framebuffer.drawFilledRoundedRect(x, y, w, h, 16, Colors::Panel);
            constexpr int headerHeight = 46;
            framebuffer.drawCardBand(x, y, w, headerHeight);
            framebuffer.drawText(x + 22, y + (headerHeight - framebuffer.lineHeight(TextStyle::Heading)) / 2, "Settings",
                                 Colors::Text, TextStyle::Heading);
            touch.clear();

            // The injection row names its actual scope. It does NOT govern saving a cart-loaded session
            // back to the cart -- that is always allowed. It only unlocks writing an OLDER BACKUP over
            // the live save, which is the case that can roll a game backwards.
            const char *labels[ROW_COUNT] = {"Auto-Backup on Load",
                                             "Theme",
                                             "Sprite Style",
                                             "SD Sprite Override",
                                             "Text Speed",
                                             "Allow Illegal Values",
                                             "Auto-Legalize Unsupported Transfers",
                                             "Bank Storage Move Warning",
                                             "Allow Inject Backups to Game Save",
                                             "Enable Debug Logging",
                                             "Language"};
            std::string spriteStyle = "Auto (Theme)";
            if (g_spriteStyle == SpriteStyle::PixelArt)
                spriteStyle = "Retro DS (Pixel)";
            else if (g_spriteStyle == SpriteStyle::ModernHD)
                spriteStyle = "Modern (3D HOME)";
            else if (g_spriteStyle == SpriteStyle::Hybrid)
                spriteStyle = "Hybrid (Box/Detail)";
            const std::string values[ROW_COUNT] = {
                g_autoBackupEnabled ? "On" : "Off",
                g_themeMode == ThemeMode::Dark ? "Dark" : (g_themeMode == ThemeMode::Light ? "Light" : "PKForge"),
                spriteStyle,
                g_sdSpriteOverride ? "On" : "Off",
                textSpeedName(g_textSpeed),
                g_allowIllegalEdits ? "On" : "Off",
                g_autoLegalizeTransfers ? "On" : "Off",
                g_moveWarn ? "On" : "Off",
                g_injectToGameSave ? "On" : "Off",
                g_debugLogging ? "On" : "Off",
                Names::nameLanguageLabel(Names::displayLanguageIndex()),
            };
            // Whether each row reads "On" (its pill lights up), and which two can damage real data.
            // Columns of the SAME table, so a row's label, value and colour travel together.
            const bool rowIsOn[ROW_COUNT] = {g_autoBackupEnabled, false, false, g_sdSpriteOverride, false,
                                             g_allowIllegalEdits, g_autoLegalizeTransfers, g_moveWarn,
                                             g_injectToGameSave, g_debugLogging, false};
            const bool rowIsDestructive[ROW_COUNT] = {false, false, false, false, false, true,
                                                      false, false, true,  false, false};

            const int rows = visibleRowCount();
            constexpr int rowsTop = 12, footerH = 22;
            const int rowGap = 5;
            const int rowH = std::min(44, (h - headerHeight - rowsTop - footerH) / rows - rowGap);
            const int rowW = std::min(760, w - 48);
            const int rowX = x + (w - rowW) / 2;
            int rectY = y + headerHeight + rowsTop;
            for (int index = 0; index < rows; ++index)
            {
                const bool selected = selectedRow == index;
                framebuffer.drawSoftShadow(rowX, rectY, rowW, rowH, 12);
                framebuffer.drawFilledRoundedRect(rowX, rectY, rowW, rowH, 12, selected ? Colors::Selected : Colors::PanelAlt);
                if (selected)
                    framebuffer.drawRoundedRect(rowX, rectY, rowW, rowH, 12, Colors::Accent, 2);
                framebuffer.drawText(rowX + 24, framebuffer.textYCenteredOn(rectY + rowH / 2), labels[index], Colors::Text);
                int valueWidth, vh;
                framebuffer.measureText(values[index], valueWidth, vh, TextStyle::Body);
                const int pillW = valueWidth + 36, pillH = std::min(32, rowH - 8);
                const int pillX = rowX + rowW - pillW - 20, pillY = rectY + (rowH - pillH) / 2;
                Color pillFill = Colors::Background, pillText = Colors::Text;
                if (rowIsOn[index])
                {
                    pillFill = rowIsDestructive[index] ? Colors::Danger : Colors::Primary;
                    pillText = rowIsDestructive[index] ? Colors::White : Colors::PrimaryText;
                }
                framebuffer.drawPill(pillX, pillY, pillW, pillH, pillFill);
                framebuffer.drawText(pillX + (pillW - valueWidth) / 2, framebuffer.textYCenteredOn(pillY + pillH / 2),
                                     values[index], pillText);
                touch.push_back({index, rowX, rectY, rowW, rowH});
                rectY += rowH + rowGap;
            }
        }

        std::string activate(int row)
        {
            std::string status;
            switch (row)
            {
            case ROW_AUTO_BACKUP:
                g_autoBackupEnabled = !g_autoBackupEnabled;
                break;
            case ROW_THEME:
                applyTheme(g_themeMode == ThemeMode::RetroDS ? ThemeMode::Dark
                           : g_themeMode == ThemeMode::Dark  ? ThemeMode::Light
                                                             : ThemeMode::RetroDS);
                if (g_spriteStyle == SpriteStyle::Auto)
                    SpriteManager::clearCache();
                break;
            case ROW_SPRITE_STYLE:
                if (g_spriteStyle == SpriteStyle::Auto)
                    SpriteManager::setSpriteStyle(SpriteStyle::Hybrid);
                else if (g_spriteStyle == SpriteStyle::Hybrid)
                    SpriteManager::setSpriteStyle(SpriteStyle::PixelArt);
                else if (g_spriteStyle == SpriteStyle::PixelArt)
                    SpriteManager::setSpriteStyle(SpriteStyle::ModernHD);
                else
                    SpriteManager::setSpriteStyle(SpriteStyle::Auto);
                break;
            case ROW_SD_OVERRIDE:
                SpriteManager::setSdOverride(!g_sdSpriteOverride);
                break;
            case ROW_TEXT_SPEED:
                g_textSpeed = (g_textSpeed + 1) % 4;
                status = std::string("Messages now type at ") + textSpeedName(g_textSpeed) + " speed.";
                break;
            case ROW_ALLOW_ILLEGAL:
                g_allowIllegalEdits = !g_allowIllegalEdits;
                break;
            case ROW_AUTO_LEGALIZE:
                // Rebuilding a pokemon the destination could never have produced REWRITES its met data
                // and can drop moves, so it is opt-in. It never alters a level, and never touches a
                // transfer the games themselves support.
                g_autoLegalizeTransfers = !g_autoLegalizeTransfers;
                status = g_autoLegalizeTransfers ? "Transfers with no official route will be rebuilt as legal natives."
                                                 : "Transfers will be left as-is for you to correct by hand.";
                break;
            case ROW_MOVE_WARNING:
                // One switch for EVERY bank transfer warning, in every generation.
                g_moveWarn = !g_moveWarn;
                status = g_moveWarn ? "Lossy bank transfers will ask before converting."
                                    : "Lossy bank transfers will convert without asking.";
                break;
            case ROW_INJECT_TO_GAME:
                // The master lock for writing an older backup into the real game save. Turning it on
                // only makes that destination available; it never writes anything by itself.
                g_injectToGameSave = !g_injectToGameSave;
                status = g_injectToGameSave ? "An older backup can now be written over your live game save."
                                            : "Backups can no longer be written over your live game save.";
                break;
            case ROW_DEBUG_LOGGING:
                g_debugLogging = !g_debugLogging;
                status = g_debugLogging ? "Debug logging on. Logs are written to sdmc:/PKSE/logs."
                                        : "Debug logging off. No new log files will be written.";
                break;
            case ROW_LANGUAGE:
            {
                // Unreachable while the row is hidden; kept because it is the working feature and the
                // constant is the only thing switched off.
                const size_t next = (Names::displayLanguageIndex() + 1) % Names::LANGUAGE_COUNT;
                Names::setDisplayLanguage(next);
                status = std::string("Names now shown in ") + Names::nameLanguageLabel(next) + ".";
                break;
            }
            default:
                return "";
            }
            Utils::saveSettings();
            return status;
        }
    }
}
