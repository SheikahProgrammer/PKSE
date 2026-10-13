#include "Utils/Settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "Globals.h"
#include "Names/NameLanguage.h"
#include "UI/Common.h"
#include "UI/SpriteManager.h"
#include "Utils/Logger.h"

namespace Utils
{
    static std::string settingsPath()
    {
        return BASE_SAVE_DIRECTORY + "/settings.cfg";
    }

    void loadSettings()
    {
        // The language default is the CONSOLE's, not English: a French player should get French
        // names without configuring anything, which is the whole point of reading it here. An
        // explicit `language=` line below then overrides it, so a choice once made is kept even if
        // it happens to equal the system language. A console set to a language the Pokemon games
        // have never shipped in (Dutch, Portuguese, Russian) falls back to English, as they do.
        Names::setDisplayLanguage(Names::systemLanguageIndex());

        // No config yet -> keep the compiled-in defaults, but still fall through to the language
        // pin below. Returning early here skipped it, which is the FIRST run on every console --
        // exactly the case the pin exists for.
        // The PKForge look is the default. A config written before it existed (no `uiStyle=` line)
        // is moved onto it once; after that, whatever theme the player picks is kept.
        UI::applyTheme(UI::ThemeMode::RetroDS);
        bool sawUiStyle = false;

        FILE *f = fopen(settingsPath().c_str(), "r");
        char line[128];
        while (f && fgets(line, sizeof(line), f))
        {
            // Trim the trailing newline / carriage return.
            char *nl = strpbrk(line, "\r\n");
            if (nl)
                *nl = '\0';

            char *eq = strchr(line, '=');
            if (!eq)
                continue;
            *eq = '\0';
            const char *key = line;
            const char *value = eq + 1;

            if (strcmp(key, "theme") == 0)
            {
                if (strcmp(value, "pkforge") == 0 || strcmp(value, "retrods") == 0)
                    UI::applyTheme(UI::ThemeMode::RetroDS);
                else if (strcmp(value, "light") == 0)
                    UI::applyTheme(UI::ThemeMode::Light);
                else
                    UI::applyTheme(UI::ThemeMode::Dark);
            }
            else if (strcmp(key, "uiStyle") == 0)
            {
                sawUiStyle = true;
            }
            else if (strcmp(key, "autoBackup") == 0)
            {
                g_autoBackupEnabled = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "allowIllegal") == 0)
            {
                g_allowIllegalEdits = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "autoLegalize") == 0)
            {
                g_autoLegalizeTransfers = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "moveWarn") == 0 || strcmp(key, "lgpeMoveWarn") == 0)
            {
                // "lgpeMoveWarn" is the old key for the same toggle, still read so an existing
                // settings.cfg doesn't silently revert the user's choice to the default. Only the
                // new key is written back, so it ages out on the first save.
                g_moveWarn = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "injectToGame") == 0)
            {
                g_injectToGameSave = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "textSpeed") == 0)
            {
                const int speed = atoi(value);
                g_textSpeed = speed < 0 ? 0 : (speed > 3 ? 3 : speed);
            }
            else if (strcmp(key, "debugLogging") == 0)
            {
                g_debugLogging = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "spriteStyle") == 0)
            {
                if (strcmp(value, "pixel") == 0)
                    UI::g_spriteStyle = UI::SpriteStyle::PixelArt;
                else if (strcmp(value, "modern") == 0)
                    UI::g_spriteStyle = UI::SpriteStyle::ModernHD;
                else if (strcmp(value, "hybrid") == 0)
                    UI::g_spriteStyle = UI::SpriteStyle::Hybrid;
                else
                    UI::g_spriteStyle = UI::SpriteStyle::Auto;
            }
            else if (strcmp(key, "sdSpriteOverride") == 0)
            {
                UI::g_sdSpriteOverride = (strcmp(value, "0") != 0);
            }
            else if (strcmp(key, "language") == 0)
            {
                // Stored as the PKHeX resource suffix ("fr", "zh-Hant") rather than an index, so
                // the file stays readable and a future reordering of the table cannot silently
                // repoint an existing config at a different language.
                for (size_t languageIndex = 0; languageIndex < Names::LANGUAGE_COUNT; ++languageIndex)
                {
                    if (strcmp(value, Names::nameLanguageSuffix(languageIndex)) == 0)
                    {
                        Names::setDisplayLanguage(languageIndex);
                        break;
                    }
                }
            }
        }
        if (f)
            fclose(f);
        if (!sawUiStyle)
            UI::applyTheme(UI::ThemeMode::RetroDS);

        // Pin English while the language is not user-selectable. Everything above still runs --
        // the console is still asked, an existing `language=` line is still honoured -- so
        // re-enabling the feature is deleting this block and nothing else. See NameLanguage.h for
        // why a manual override was the wrong control and what replaces it.
        if (!Names::DISPLAY_LANGUAGE_SELECTABLE)
            Names::setDisplayLanguage(Names::LANGUAGE_INDEX_ENGLISH);
    }

    void saveSettings()
    {
        // Make sure the base dir exists (it normally does once a backup has been taken).
        mkdir(BASE_SAVE_DIRECTORY.c_str(), 0777);

        FILE *f = fopen(settingsPath().c_str(), "w");
        if (!f)
        {
            logErrorToFile("Failed to write settings file", settingsPath().c_str());
            return;
        }
        const char *themeStr = "dark";
        if (UI::g_themeMode == UI::ThemeMode::RetroDS)
            themeStr = "pkforge";
        else if (UI::g_themeMode == UI::ThemeMode::Light)
            themeStr = "light";
        fprintf(f, "theme=%s\n", themeStr);
        fprintf(f, "uiStyle=pkforge\n");

        const char *styleStr = "auto";
        if (UI::g_spriteStyle == UI::SpriteStyle::PixelArt)
            styleStr = "pixel";
        else if (UI::g_spriteStyle == UI::SpriteStyle::ModernHD)
            styleStr = "modern";
        else if (UI::g_spriteStyle == UI::SpriteStyle::Hybrid)
            styleStr = "hybrid";
        fprintf(f, "spriteStyle=%s\n", styleStr);
        fprintf(f, "sdSpriteOverride=%d\n", UI::g_sdSpriteOverride ? 1 : 0);

        fprintf(f, "autoBackup=%d\n", g_autoBackupEnabled ? 1 : 0);
        fprintf(f, "allowIllegal=%d\n", g_allowIllegalEdits ? 1 : 0);
        fprintf(f, "autoLegalize=%d\n", g_autoLegalizeTransfers ? 1 : 0);
        fprintf(f, "moveWarn=%d\n", g_moveWarn ? 1 : 0);
        fprintf(f, "injectToGame=%d\n", g_injectToGameSave ? 1 : 0);
        fprintf(f, "debugLogging=%d\n", g_debugLogging ? 1 : 0);
        fprintf(f, "textSpeed=%d\n", g_textSpeed);
        fprintf(f, "language=%s\n", Names::nameLanguageSuffix(Names::displayLanguageIndex()));
        fclose(f);
    }
}
