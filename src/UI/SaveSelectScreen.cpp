#include <cstring>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

#include "Globals.h"
#include "UI/SaveSelectScreen.h"
#include "Utils/ResumeState.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"
#include "UI/ListSearch.h"
#include "UI/SystemIcons.h"
#include "UI/SpriteManager.h"
#include "UI/Pokepark.h"
#include "UI/TouchInput.h"
#include "Enums/GameVersion.h"
#include "Utils/Logger.h"
#include "Utils/FileUtilities.h"
#include "Save/GetSaveFileContents.h"
#include "Save/SaveFileName.h"   // the one rule for what looks like a loose save
#include "Save/TitleFolder.h"    // the one rule for what a title is called, on screen and on the card

using namespace Utils;
using namespace Enums;

namespace UI
{
    // Layout (1280x720).
    constexpr int HEADER_Y = 82; // top of the user header card
    constexpr int HEADER_H = 104;
    constexpr int AVATAR = 84;
    constexpr int CHIP = 56; // small per-user switcher avatar
    constexpr int TILE_W = 184;
    constexpr int TILE_H = 200;
    constexpr int ICON = 140;
    constexpr int CARD_GAP = 24;
    constexpr int MAX_COLS = 5;
    constexpr int GRID_Y = 208;
    // Rows of tiles that fit between the grid's top and the nav bar: (720 - 56 - 208) = 456px of band
    // against a 224px row pitch. Anything past this scrolls rather than being drawn off the bottom edge.
    constexpr int VISIBLE_ROWS = 2;

    // The save picker's own place (Utils::Resume): the user and title last opened. Matched by
    // identity, not position -- a user added, or a game gaining its first save, shifts the lists.
    static constexpr const char *PICKER_RESUME_SECTION = "picker";

    static std::string userResumeKey(const SaveSelectScreen::UserEntry &user)
    {
        char uid[40];
        snprintf(uid, sizeof(uid), "%016lX%016lX", static_cast<unsigned long>(user.accountUid.uid[0]),
                 static_cast<unsigned long>(user.accountUid.uid[1]));
        // The Tico / Citra entries are not accounts (their uid is zero), so the name tells them apart.
        return std::string(uid) + ":" + user.name;
    }

    static std::string titleResumeKey(const SaveSelectScreen::TitleEntry &title)
    {
        if (!title.filePath.empty())
            return "file:" + title.filePath;
        char id[24];
        snprintf(id, sizeof(id), "%016lX", static_cast<unsigned long>(title.titleId));
        return id;
    }

    SaveSelectScreen::SaveSelectScreen()
    {
        loadUsers();
        loadTicoSaves();
        PokeparkManager::instance().loadState(); // the Poképark card names how many live there

        const Utils::Resume::Fields fields = Utils::Resume::get(PICKER_RESUME_SECTION);
        const std::string wantUser = Utils::Resume::textOf(fields, "user");
        const std::string wantTitle = Utils::Resume::textOf(fields, "title");
        for (int u = 0; u < static_cast<int>(users.size()); ++u)
        {
            if (userResumeKey(users[u]) != wantUser)
                continue;
            setUser(u);
            for (int t = 0; t < static_cast<int>(users[u].titles.size()); ++t)
            {
                if (titleResumeKey(users[u].titles[t]) == wantTitle)
                {
                    titleIndex = t;
                    scrollSelectionIntoView();
                    break;
                }
            }
            break;
        }
    }

    void SaveSelectScreen::loadUsers()
    {
        users.clear();

        AccountUid userIds[ACC_USER_LIST_SIZE];
        s32 userCount = 0;
        Result resultCode = accountListAllUsers(userIds, ACC_USER_LIST_SIZE, &userCount);
        if (R_FAILED(resultCode))
        {
            logErrorToFile("SaveSelect: failed to list users");
            userCount = 0;
        }

        for (s32 userIndex = 0; userIndex < userCount; userIndex++)
        {
            UserEntry entry;
            entry.accountUid = userIds[userIndex];

            AccountProfile profile;
            AccountProfileBase base;
            if (R_SUCCEEDED(accountGetProfile(&profile, userIds[userIndex])))
            {
                if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &base)))
                {
                    entry.name = std::string(base.nickname);
                }
                else
                {
                    entry.name = "Unknown User";
                }
                accountProfileClose(&profile);
            }
            else
            {
                entry.name = "Unknown User";
            }

            loadTitlesForUser(entry);
            users.push_back(std::move(entry));
        }

        // BACKUP FOLDERS CARRY THE TITLE ID, so anything written under the older naming has to be moved or
        // it simply stops being listed -- on every user's card, not just the one where the collision was
        // found. Run after EVERY user is loaded, because the backup tree is shared between users and
        // whether a legacy folder is ambiguous depends on every title the console has, not just this one's.
        // See Save::migrateLegacyTitleFolders: it refuses the ambiguous ones rather than guessing, and a
        // failure leaves the old folder exactly where it was, which is what makes it safe to attempt on
        // every launch.
        std::vector<Save::InstalledTitle> installedTitles;
        for (const UserEntry &user : users)
        {
            for (const TitleEntry &title : user.titles)
            {
                bool alreadyCounted = false;
                for (const Save::InstalledTitle &counted : installedTitles)
                    alreadyCounted = alreadyCounted || counted.titleId == title.titleId;
                if (!alreadyCounted)
                    installedTitles.push_back({title.titleId, title.gameVersion});
            }
        }
        Save::migrateLegacyTitleFolders(installedTitles);

        if (users.empty())
        {
            UserEntry defaultUser;
            defaultUser.name = "No users found";
            memset(&defaultUser.accountUid, 0, sizeof(AccountUid));
            users.push_back(std::move(defaultUser));
        }
    }

    namespace
    {
        /**
         * Identifies a Pokemon save PKSE knows, from its CONTENT, for a title id it does not.
         *
         * A Switch save is found by title id, so `getGameVersion()` is a hand-kept list -- and a
         * hand-kept list of Nintendo's title ids goes stale the moment a title ships that nobody
         * has reported. That is not hypothetical: FireRed and LeafGreen ship a SEPARATE TITLE PER
         * LANGUAGE (the GBA originals have no in-game language option), so listing only the
         * English pair made a French LeafGreen owner see "0 saves found" -- the save was there,
         * readable, and simply never offered. Every other Pokemon title on Switch is one
         * worldwide id, which is why Gen 3 is the only family this can currently discover.
         *
         * Returns the GROUP, never a specific game: a Gen 3 save carries no byte saying whether it
         * is FireRed or LeafGreen, so a probed save is named for the pair, exactly as an unnamed
         * Red/Blue save is. A title id PKSE recognises still takes the fast path above and is
         * named exactly -- this only runs for ids it does not.
         *
         * Every failure is silent and non-fatal: an id that is not a Pokemon title is the normal
         * case, and a console's other games must cost nothing but a mount that immediately fails
         * or finds nothing.
         */
        Enums::GameVersion probeUnknownSave(AccountUid userUid, u64 titleId)
        {
            if (R_FAILED(fsdevMountSaveData("probe", titleId, userUid)))
                return Enums::GameVersion::Invalid;
            // One implementation of "what is in this directory", shared with the loader -- see
            // Save::detectGroupInDirectory. A mounted save and a backup folder are both just
            // directories, and two probes that had to agree would be one more pair of lists that
            // eventually do not.
            const Enums::GameVersion found = Save::detectGroupInDirectory("probe:");
            fsdevUnmountDevice("probe");
            return found;
        }
    }

    /**
     * List the Pokemon saves this user has, by enumerating SAVE DATA -- not installed titles.
     *
     * The distinction is the whole point. Walking `nsListApplicationRecord` and probing each Pokemon
     * title for a mountable save asks "which games are installed, and do they have saves?" when the
     * only question a save editor cares about is "which saves exist?" -- and the two differ in a case
     * that is not rare at all:
     *
     *   A game played from a CARTRIDGE has no application record when the cart is out. Its save
     *   lives on internal storage and is perfectly editable, but the title vanishes from the picker
     *   the moment the cart is swapped for another game. An archived or partly-uninstalled title
     *   does the same.
     *
     * Enumerating save data finds those, because the OS lists the save whether or not anything is
     * currently installed to play it. It is also cheaper: no mount/unmount probe per title, which is
     * both slow and a devoptab slot churn.
     */
    // Scan one save-data space and append this user's Pokemon saves. Returns false only if the
    // space could not be opened at all; an empty space is a perfectly normal success.
    bool SaveSelectScreen::scanSaveSpace(UserEntry &user, int spaceId, int &scanned, int &forUser)
    {
        FsSaveDataInfoReader reader;
        Result resultCode = fsOpenSaveDataInfoReader(&reader, static_cast<FsSaveDataSpaceId>(spaceId));
        if (R_FAILED(resultCode))
        {
            char messageBuffer[112];
            snprintf(messageBuffer, sizeof(messageBuffer), "SaveSelect: cannot read save space %d (rc=0x%08X)", spaceId,
                     (unsigned)resultCode);
            logInfoToFile(messageBuffer);
            return false;
        }

        FsSaveDataInfo info[24];
        s64 readCount = 0;
        while (R_SUCCEEDED(fsSaveDataInfoReaderRead(&reader, info, 24, &readCount)) && readCount > 0)
        {
            for (s64 infoIndex = 0; infoIndex < readCount; infoIndex++)
            {
                scanned++;
                // Account saves only -- system/temporary/cache entries are not a player's save file.
                if (info[infoIndex].save_data_type != FsSaveDataType_Account)
                    continue;
                if (memcmp(&info[infoIndex].uid, &user.accountUid, sizeof(AccountUid)) != 0)
                    continue;
                forUser++;

                const u64 titleId = info[infoIndex].application_id;
                GameVersion gameVersion = getGameVersion(titleId);
                if (gameVersion == GameVersion::Invalid)
                {
                    // Not an id PKSE knows -- which is the normal case for every non-Pokemon game
                    // on the console, and ALSO the case for a Pokemon title whose id nobody has
                    // reported yet. Ask the bytes before giving up; see probeUnknownSave.
                    gameVersion = probeUnknownSave(user.accountUid, titleId);
                    if (gameVersion == GameVersion::Invalid)
                        continue;
                    char probeBuffer[128];
                    snprintf(probeBuffer, sizeof(probeBuffer),
                             "SaveSelect: %016llX unknown id, identified by content as %s",
                             (unsigned long long)titleId, getGameVersionName(gameVersion).c_str());
                    logInfoToFile(probeBuffer);
                }

                // One tile per game. A title can report more than one save entry (save_data_index),
                // and scanning two spaces can see the same save twice -- listing a game twice would
                // be worse than useless.
                bool alreadyListed = false;
                for (const auto &titleEntry : user.titles)
                {
                    if (titleEntry.titleId == titleId)
                    {
                        alreadyListed = true;
                        break;
                    }
                }
                if (alreadyListed)
                    continue;

                char messageBuffer[128];
                snprintf(messageBuffer, sizeof(messageBuffer), "SaveSelect: %s (%016llX) -> listed",
                         getGameVersionName(gameVersion).c_str(), (unsigned long long)titleId);
                logInfoToFile(messageBuffer);

                TitleEntry titleEntry;
                titleEntry.titleId = titleId;
                // BOTH NAMES COME FROM Save/TitleFolder.h, and they are deliberately different
                // strings: the label has 184 pixels to be readable in and the folder has to be
                // UNIQUE PER TITLE ID. Building either from the GameVersion alone is what let every
                // Gen 3 SKU share one backup directory -- see the note in that header.
                titleEntry.gameVersion = gameVersion;
                titleEntry.label = Save::titleDisplayLabel(titleId, gameVersion);
                titleEntry.name = Save::titleDisplayName(titleId, gameVersion);
                titleEntry.folder = Save::titleFolderName(titleId, gameVersion);
                user.titles.push_back(std::move(titleEntry));
            }
        }
        fsSaveDataInfoReaderClose(&reader);
        return true;
    }

    void SaveSelectScreen::loadTitlesForUser(UserEntry &user)
    {
        int scanned = 0, forUser = 0;

        // `All` is the pseudo-space the header blesses for this reader, and it is what should
        // normally answer. Fall back to the concrete spaces if it is refused, because "found
        // nothing" is precisely the failure this function exists to stop producing.
        if (!scanSaveSpace(user, FsSaveDataSpaceId_All, scanned, forUser))
        {
            scanSaveSpace(user, FsSaveDataSpaceId_User, scanned, forUser);
            scanSaveSpace(user, FsSaveDataSpaceId_SdUser, scanned, forUser);
        }

        // Counts make a missing title diagnosable from the log alone: how many saves the console
        // reported in total, how many belong to this user, and how many were Pokemon titles.
        char summary[192];
        snprintf(summary, sizeof(summary),
                 "SaveSelect: %d save entries on console, %d for %s, %d Pokemon titles listed",
                 scanned, forUser, user.name.c_str(), (int)user.titles.size());
        logInfoToFile(summary);
    }

    const SaveSelectScreen::UserEntry *SaveSelectScreen::currentUser() const
    {
        if (users.empty())
            return nullptr;
        return &users[userIndex];
    }

    int SaveSelectScreen::titleCount() const
    {
        const UserEntry *u = currentUser();
        return u ? static_cast<int>(u->titles.size()) : 0;
    }

    int SaveSelectScreen::titleColumns() const
    {
        int count = titleCount();
        if (count <= 0)
            return 1;
        if (isPkForgeTheme())
            return count; // the shelf is one row; Up/Down then fall through to the trainer switch
        return count < MAX_COLS ? count : MAX_COLS;
    }

    // Rows of tiles, at MAX_COLS per row.
    int SaveSelectScreen::titleRows() const
    {
        const int count = titleCount();
        if (count <= 0)
            return 0;
        const int cols = titleColumns();
        return (count + cols - 1) / cols;
    }

    /**
     * Move the scroll window as LITTLE as possible to keep the selected tile on screen.
     *
     * `scrollRow` is deliberately STATE, not something recomputed from `titleIndex` each frame. The
     * derived version centred the selection -- `first = selRow - VISIBLE_ROWS/2` -- which reads as
     * paging: with three rows, selecting the bottom row shows rows 1-2, and moving back up to the
     * middle row snapped the view to rows 0-1 even though the middle row was *already visible*.
     * Every vertical move repainted the whole grid.
     *
     * Scrolling only when the selection would otherwise fall outside the window means the view
     * holds still while the cursor moves inside it, and shifts by exactly one row at the edges.
     * With three rows that is: nothing at all between the top two rows, one row down when you enter
     * the bottom row, and one row back up only when you leave the top row.
     */
    void SaveSelectScreen::scrollSelectionIntoView()
    {
        const int rows = titleRows();
        if (rows <= VISIBLE_ROWS)
        {
            scrollRow = 0;
            return;
        } // everything fits; never offset

        const int selRow = titleIndex / titleColumns();
        if (selRow < scrollRow)
            scrollRow = selRow;
        else if (selRow >= scrollRow + VISIBLE_ROWS)
            scrollRow = selRow - VISIBLE_ROWS + 1;

        // Clamp for the case the row count shrank under us (switching to a user with fewer saves).
        if (scrollRow > rows - VISIBLE_ROWS)
            scrollRow = rows - VISIBLE_ROWS;
        if (scrollRow < 0)
            scrollRow = 0;
    }

    void SaveSelectScreen::setUser(int index)
    {
        if (users.empty())
            return;
        userIndex = (index % (int)users.size() + (int)users.size()) % (int)users.size();
        titleIndex = 0;
        scrollRow = 0; // a different user has a different title count; start at the top
    }

    void SaveSelectScreen::selectCurrentTitle()
    {
        const UserEntry *u = currentUser();
        if (!u || titleIndex < 0 || titleIndex >= static_cast<int>(u->titles.size()))
            return;
        Utils::Resume::put(PICKER_RESUME_SECTION,
                           {{"user", userResumeKey(*u)}, {"title", titleResumeKey(u->titles[titleIndex])}});
        if (!u->titles[titleIndex].filePath.empty())
        {
            selectedFilePath = u->titles[titleIndex].filePath;
            fileSelected = true;
            return;
        }
        selectedUserUid = u->accountUid;
        selectedTitleId = u->titles[titleIndex].titleId;
        selectedTitleName = u->titles[titleIndex].name;
        selectedTitleFolder = u->titles[titleIndex].folder;
        titleSelected = true;
    }

    void SaveSelectScreen::update(const PadState &pad, const TouchInput &touch)
    {
        // A tap on a nav-bar badge becomes that button's press, so every handler below is
        // reached identically whether the user pressed the button or tapped its on-screen badge.
        u64 buttonsDown = padGetButtonsDown(&pad) | navTouchButton(touch);

        // A tap recorded earlier fires once its selection has actually been on screen. See armTap().
        if (pendingTapButton != 0 && --pendingTapFrames <= 0)
        {
            buttonsDown |= pendingTapButton;
            pendingTapButton = 0;
        }

        // The browser owns input entirely while it is open -- otherwise the grid underneath would
        // move under a cursor the user cannot see, and B would exit the screen instead of the
        // dialog. Same rule every other overlay in the app follows.
        if (fileBrowser.active)
        {
            handleFileBrowserInput(buttonsDown, touch);
            return;
        }
        if (buttonsDown & HidNpadButton_Y)
        {
            openFileBrowser();
            return;
        }
        // Touch (tap targets were captured last draw()).
        if (touch.justPressed())
        {
            const int touchX = touch.x(), ty = touch.y();
            bool handled = false;
            for (const auto &r : userRects)
            {
                if (touchX >= r.hitX && touchX < r.hitX + r.hitWidth && ty >= r.hitY && ty < r.hitY + r.hitHeight)
                {
                    setUser(r.entryIndex);
                    focusZone = 0;
                    handled = true;
                    break;
                }
            }
            if (!handled)
            {
                for (const auto &r : titleRects)
                {
                    if (touchX >= r.hitX && touchX < r.hitX + r.hitWidth && ty >= r.hitY && ty < r.hitY + r.hitHeight)
                    {
                        titleIndex = r.entryIndex;      // draws selected this frame
                        focusZone = 1;
                        armTap(HidNpadButton_A);        // ...and opens on the next
                        return;
                    }
                }
            }
        }

        // The cards: tapped, or picked with A while focused; X / Minus reach them from any theme.
        if (touch.justPressed())
            for (const auto &r : cardRects)
                if (touch.x() >= r.hitX && touch.x() < r.hitX + r.hitWidth && touch.y() >= r.hitY &&
                    touch.y() < r.hitY + r.hitHeight)
                {
                    hub = r.entryIndex == 0 ? HubRequest::Pokepark : HubRequest::Settings;
                    return;
                }
        if (buttonsDown & HidNpadButton_X)
        {
            hub = HubRequest::Settings;
            return;
        }
        if (buttonsDown & HidNpadButton_Minus)
        {
            hub = HubRequest::Pokepark;
            return;
        }

        // Switch users with the shoulder buttons (only when there's more than one).
        if (users.size() > 1)
        {
            if (buttonsDown & HidNpadButton_L)
                setUser(userIndex - 1);
            if (buttonsDown & HidNpadButton_R)
                setUser(userIndex + 1);
        }

        // The PKForge shelf has three rows of focus: trainers, games, cards. Up/Down move between
        // them; Left/Right act on whichever row is focused.
        if (isPkForgeTheme())
        {
            if ((buttonsDown & HidNpadButton_Up) && focusZone > 0)
                focusZone = (focusZone == 2 || users.size() > 1) ? focusZone - 1 : focusZone;
            else if ((buttonsDown & HidNpadButton_Down) && focusZone < 2)
                ++focusZone;
            buttonsDown &= ~(HidNpadButton_Up | HidNpadButton_Down);
            if (focusZone == 0)
            {
                if (buttonsDown & HidNpadButton_Left)
                    setUser(userIndex - 1);
                if (buttonsDown & HidNpadButton_Right)
                    setUser(userIndex + 1);
                if (buttonsDown & HidNpadButton_A)
                    focusZone = 1;
                buttonsDown &= ~(HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_A);
            }
            else if (focusZone == 2)
            {
                if (buttonsDown & (HidNpadButton_Left | HidNpadButton_Right))
                    cardIndex = 1 - cardIndex;
                if (buttonsDown & HidNpadButton_A)
                {
                    hub = cardIndex == 0 ? HubRequest::Pokepark : HubRequest::Settings;
                    return;
                }
                buttonsDown &= ~(HidNpadButton_Left | HidNpadButton_Right | HidNpadButton_A);
            }
        }

        // Title grid navigation. titleCount() already resolves the current user.
        int count = titleCount();
        if (count > 0)
        {
            int cols = titleColumns();
            if (buttonsDown & HidNpadButton_Left)
                titleIndex = (titleIndex - 1 + count) % count;
            if (buttonsDown & HidNpadButton_Right)
                titleIndex = (titleIndex + 1) % count;
            if (buttonsDown & HidNpadButton_Down)
            {
                // A partial last row still has to be reachable. Straight down when that column
                // exists below; otherwise fall to the final tile rather than refusing to move --
                // with twelve titles the bottom row is just two wide, and Down from the right-hand
                // columns would otherwise do nothing at all.
                if (titleIndex + cols < count)
                    titleIndex += cols;
                else if (titleIndex / cols < (count - 1) / cols)
                    titleIndex = count - 1;
            }
            if ((buttonsDown & HidNpadButton_Up) && titleIndex - cols >= 0)
                titleIndex -= cols;
            if (buttonsDown & HidNpadButton_A)
                selectCurrentTitle();
            // One place, after every way the selection can move -- stick, D-pad or a tap.
            scrollSelectionIntoView();
        }

        if (buttonsDown & HidNpadButton_Plus)
            exitRequested = true;
    }

    void SaveSelectScreen::draw(PKSEFramebuffer &framebuffer)
    {
        titleRects.clear();
        userRects.clear();
        cardRects.clear();

        framebuffer.clear(Colors::Background);
        drawTitleBar(framebuffer, "Pokémon Save Editor", "v" + VERSION_STRING);

        if (isPkForgeTheme())
        {
            drawShelf(framebuffer);
            return;
        }

        const UserEntry *u = currentUser();

        framebuffer.drawCard(24, HEADER_Y, framebuffer.getWidth() - 48, HEADER_H);

        const int avatarX = 44, avY = HEADER_Y + (HEADER_H - AVATAR) / 2;
        if (u)
        {
            const IconImage &av = SystemIcons::userIcon(u->accountUid);
            if (av.valid())
                framebuffer.drawImageScaled(avatarX, avY, av.width, av.height, AVATAR, AVATAR, av.data, 4);
            else
                framebuffer.drawFilledRoundedRect(avatarX, avY, AVATAR, AVATAR, 10, Colors::Selected);
            framebuffer.drawRoundedRect(avatarX, avY, AVATAR, AVATAR, 10, Colors::Accent, 2);
        }

        const int nameX = avatarX + AVATAR + 20;
        if (u)
        {
            int nameLineHeight = framebuffer.lineHeight(TextStyle::Title);
            framebuffer.drawText(nameX, avY + 6, u->name, Colors::Text, TextStyle::Title);
            int saveCount = (int)u->titles.size();
            std::string subtitleText =
                std::to_string(saveCount) + (saveCount == 1 ? " Pokémon save" : " Pokémon saves");
            framebuffer.drawText(nameX, avY + 6 + nameLineHeight + 4, subtitleText, Colors::TextDim,
                                 TextStyle::Caption);
        }

        // Per-user switcher chips (only when more than one account).
        if (users.size() > 1)
        {
            int userCountOnScreen = (int)users.size();
            int totalW = userCountOnScreen * CHIP + (userCountOnScreen - 1) * 12;
            int startX = framebuffer.getWidth() - 40 - totalW;
            int chipY = HEADER_Y + (HEADER_H - CHIP) / 2;
            for (int index = 0; index < userCountOnScreen; index++)
            {
                int centerX = startX + index * (CHIP + 12);
                const IconImage &av = SystemIcons::userIcon(users[index].accountUid);
                if (av.valid())
                    framebuffer.drawImageScaled(centerX, chipY, av.width, av.height, CHIP, CHIP, av.data, 4);
                else
                    framebuffer.drawFilledRoundedRect(centerX, chipY, CHIP, CHIP, 8, Colors::Selected);
                if (index == userIndex)
                    framebuffer.drawRoundedRect(centerX, chipY, CHIP, CHIP, 8, Colors::Primary, 3);
                else
                    framebuffer.drawRoundedRect(centerX, chipY, CHIP, CHIP, 8, Colors::Border, 1);
                userRects.push_back({centerX, chipY, CHIP, CHIP, index});
            }
        }

        int count = titleCount();
        if (count == 0)
        {
            const char *message = "No Pokémon saves found for this user";
            int messageWidth, mh;
            framebuffer.measureText(message, messageWidth, mh, TextStyle::Body);
            framebuffer.drawText((framebuffer.getWidth() - messageWidth) / 2, GRID_Y + 120, message, Colors::TextDim,
                                 TextStyle::Body);
        }
        else
        {
            const int cols = titleColumns();
            const int rows = titleRows();
            const int first = scrollRow;
            const int gridW = cols * TILE_W + (cols - 1) * CARD_GAP;
            int startX = (framebuffer.getWidth() - gridW) / 2;
            if (startX < 40)
                startX = 40;

            // Only the rows inside the window are drawn, and only those get tap targets -- an
            // off-screen tile must not be tappable through whatever is covering it.
            const int firstIdx = first * cols;
            const int lastIdx = (first + VISIBLE_ROWS) * cols; // exclusive

            for (int index = firstIdx; index < count && index < lastIdx; index++)
            {
                int columnIndex = index % cols, row = (index / cols) - first;
                int tileX = startX + columnIndex * (TILE_W + CARD_GAP);
                int tileY = GRID_Y + row * (TILE_H + CARD_GAP);
                bool selectedIndex = (index == titleIndex);

                framebuffer.drawSoftShadow(tileX, tileY, TILE_W, TILE_H, 16);
                framebuffer.drawFilledRoundedRect(tileX, tileY, TILE_W, TILE_H, 16,
                                                  selectedIndex ? Colors::PanelAlt : Colors::Panel);
                if (selectedIndex)
                    framebuffer.drawRoundedRect(tileX, tileY, TILE_W, TILE_H, 16, Colors::Primary, 3);
                else
                    framebuffer.drawRoundedRect(tileX, tileY, TILE_W, TILE_H, 16, Colors::Border, 1);

                int iconX = tileX + (TILE_W - ICON) / 2;
                int iconY = tileY + 16;
                const IconImage *pIc = nullptr;
                if (!u->titles[index].iconPath.empty())
                {
                    const IconImage &fileIc = SystemIcons::fileIcon(u->titles[index].iconPath);
                    if (fileIc.valid())
                        pIc = &fileIc;
                }
                if (!pIc)
                {
                    const IconImage &sysIc = SystemIcons::titleIcon(u->titles[index].titleId);
                    if (sysIc.valid())
                        pIc = &sysIc;
                }
                if (pIc && pIc->valid())
                    framebuffer.drawImageScaled(iconX, iconY, pIc->width, pIc->height, ICON, ICON, pIc->data, 4);
                else
                    framebuffer.drawFilledRoundedRect(iconX, iconY, ICON, ICON, 10, Colors::PanelAlt);

                // Short label centered under the icon.
                const std::string &label = u->titles[index].label;
                int labelWidth, lh;
                framebuffer.measureText(label, labelWidth, lh, TextStyle::Caption);
                framebuffer.drawText(tileX + (TILE_W - labelWidth) / 2, iconY + ICON + 8, label,
                            selectedIndex ? Colors::Text : Colors::TextDim, TextStyle::Caption);

                titleRects.push_back({tileX, tileY, TILE_W, TILE_H, index});
            }

            // Scrollbar to the right of the grid, drawn only when the rows overflow -- same thumb
            // the backup list and the details editor use.
            constexpr int ROW_PITCH = TILE_H + CARD_GAP;
            drawScrollbar(framebuffer, startX + gridW + 12, GRID_Y, VISIBLE_ROWS * ROW_PITCH, rows * ROW_PITCH,
                          first * ROW_PITCH);
        }

        // ONE nav bar, naming whatever owns input. A second bar drawn over this, plus a third strip inside
        // the browser card, is two visible rows of controller badges saying different things about the same
        // dialog. The bar is chosen here and drawn once, the same way TrainerViewScreen's instruction chain
        // does it.
        if (fileBrowser.active)
            drawNavBar(framebuffer,
                       "Up/Down: Move  |  L/R: Page  |  A: Open  |  Y: Search  |  X: All Files  |  B: Up / Close");
        else if (users.size() > 1)
            drawNavBar(framebuffer,
                       "A: Select  |  L/R: Switch User  |  Y: Open Save File  |  -: Poképark  |  X: Settings  |  +: Exit");
        else
            drawNavBar(framebuffer, "A: Select  |  Y: Open Save File  |  -: Poképark  |  X: Settings  |  +: Exit");

        // Drawn last so it sits above the grid.
        if (fileBrowser.active)
            Dialogs::drawFileBrowser(fileBrowser, framebuffer, browserTaps);
    }

    // ---- The PKForge shelf ------------------------------------------------------------------------

    // Kit.EraColor: the cartridge plastic per console era.
    static Color eraColor(int generation)
    {
        switch (generation)
        {
        case 1:
        case 2: return Color(0x9A, 0xA6, 0xB2);
        case 3: return Color(0x8B, 0x7B, 0xD8);
        case 4: return Color(0x5E, 0x86, 0xD4);
        case 5: return Color(0x4E, 0x5A, 0x68);
        case 6: return Color(0x4F, 0xB6, 0xDB);
        case 7: return Color(0xEE, 0x9A, 0x4A);
        case 8: return Color(0xD4, 0x5C, 0x6D);
        case 9: return Color(0x9A, 0x6A, 0xD8);
        default: return Pksm::InkSoft;
        }
    }

    // Kit.ConsoleCode.
    static const char *consoleCode(int generation)
    {
        switch (generation)
        {
        case 1:
        case 2: return "GB";
        case 3: return "GBA";
        case 4:
        case 5: return "DS";
        case 6:
        case 7: return "3DS";
        case 8:
        case 9: return "Switch";
        default: return "";
        }
    }

    static std::string lowered(const std::string &text)
    {
        std::string out = text;
        for (char &c : out)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    // A loose save's console, read off where the emulators keep it -- the scanners do not record a
    // game version for these, so the folder is the only thing that knows.
    static std::string loosePlatform(const std::string &path)
    {
        const std::string p = lowered(path);
        if (p.find("nintendo 3ds") != std::string::npos || p.find("/3ds") != std::string::npos)
            return "3DS";
        if (p.find("/nds") != std::string::npos || p.find("melonds") != std::string::npos)
            return "DS";
        if (p.find("/gbc") != std::string::npos)
            return "GBC";
        if (p.find("/gba") != std::string::npos)
            return "GBA";
        if (p.find("/gb/") != std::string::npos)
            return "GB";
        return "";
    }

    // Which emulator owns a loose save, by the same folders loadTicoSaves() walks.
    static std::string looseEmulator(const std::string &path)
    {
        const std::string p = lowered(path);
        if (p.find("/tico/system/3ds") != std::string::npos)
            return "Azahar in Tico";
        if (p.find("/tico/") != std::string::npos)
            return "Tico";
        if (p.find("melonds") != std::string::npos)
            return "melonDS";
        if (p.find("/retroarch/") != std::string::npos)
            return "RetroArch";
        return "SD card";
    }

    static int platformGeneration(const std::string &platform)
    {
        if (platform == "GB" || platform == "GBC")
            return 2;
        if (platform == "GBA")
            return 3;
        if (platform == "DS")
            return 4;
        if (platform == "3DS")
            return 6;
        return 0;
    }

    static int titleGeneration(const SaveSelectScreen::TitleEntry &title)
    {
        if (title.gameVersion == Enums::GameVersion::Invalid)
            return title.filePath.empty() ? 8 : platformGeneration(loosePlatform(title.filePath));
        return Enums::getVersionGeneration(Enums::getGroupRepVersion(title.gameVersion));
    }

    // The console as the shelf names it: the save's own folder when it says, else the era's family.
    static std::string titleConsole(const SaveSelectScreen::TitleEntry &title);

    static Color darker(Color c, float by)
    {
        return Color(static_cast<uint8_t>(c.red * (1 - by)), static_cast<uint8_t>(c.green * (1 - by)),
                     static_cast<uint8_t>(c.blue * (1 - by)));
    }

    // `text` cut to fit `maxWidth`, ending in an ellipsis when it had to be (the shelf's TailTruncation).
    static std::string fitWidth(PKSEFramebuffer &framebuffer, const std::string &text, int maxWidth, TextStyle style)
    {
        int w, h;
        framebuffer.measureText(text, w, h, style);
        if (w <= maxWidth)
            return text;
        // Shorten one UTF-8 code point at a time (never splitting an accented letter) until the
        // text plus its ellipsis fits.
        size_t end = text.size();
        while (end > 0)
        {
            do
                --end;
            while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80);
            const std::string candidate = text.substr(0, end) + "...";
            framebuffer.measureText(candidate, w, h, style);
            if (w <= maxWidth)
                return candidate;
        }
        return "...";
    }

    // `text` cut in the MIDDLE to fit, so a path keeps both where it starts and the file it ends on.
    static std::string fitMiddle(PKSEFramebuffer &framebuffer, const std::string &text, int maxWidth, TextStyle style)
    {
        int w, h;
        framebuffer.measureText(text, w, h, style);
        if (w <= maxWidth || text.size() < 8)
            return fitWidth(framebuffer, text, maxWidth, style);
        size_t keepHead = text.size() / 2, keepTail = text.size() - keepHead;
        while (keepHead + keepTail > 4)
        {
            // Trim whichever side is longer, on code-point boundaries.
            if (keepHead >= keepTail)
            {
                do
                    --keepHead;
                while (keepHead > 0 && (static_cast<unsigned char>(text[keepHead]) & 0xC0) == 0x80);
            }
            else
            {
                do
                    --keepTail;
                while (keepTail > 0 &&
                       (static_cast<unsigned char>(text[text.size() - keepTail]) & 0xC0) == 0x80);
            }
            const std::string candidate = text.substr(0, keepHead) + "..." + text.substr(text.size() - keepTail);
            framebuffer.measureText(candidate, w, h, style);
            if (w <= maxWidth)
                return candidate;
        }
        return fitWidth(framebuffer, text, maxWidth, style);
    }

    std::string titleConsole(const SaveSelectScreen::TitleEntry &title)
    {
        if (title.filePath.empty())
            return "Switch";
        const std::string platform = loosePlatform(title.filePath);
        return platform.empty() ? std::string(consoleCode(titleGeneration(title))) : platform;
    }

    static const IconImage *titleIconFor(const SaveSelectScreen::TitleEntry &title)
    {
        if (!title.iconPath.empty())
        {
            const IconImage &fileIc = SystemIcons::fileIcon(title.iconPath);
            if (fileIc.valid())
                return &fileIc;
        }
        const IconImage &sysIc = SystemIcons::titleIcon(title.titleId);
        return sysIc.valid() ? &sysIc : nullptr;
    }

    void SaveSelectScreen::drawShelf(PKSEFramebuffer &framebuffer)
    {
        const int screenW = framebuffer.getWidth();
        const UserEntry *u = currentUser();

        // -- Trainer tabs: one menu button per user, the current one cobalt. ------------------------
        constexpr int tabsY = HEADER_HEIGHT + 14, tabH = 56, avatar = 40;
        framebuffer.drawText(24, framebuffer.textYCenteredOn(tabsY + tabH / 2, TextStyle::Heading), "Trainer",
                             Pksm::Ink, TextStyle::Heading);
        int tabX = 176;
        for (int index = 0; index < static_cast<int>(users.size()); ++index)
        {
            const UserEntry &user = users[index];
            const bool current = index == userIndex;
            int nameW, nameH;
            framebuffer.measureText(user.name, nameW, nameH, TextStyle::Body);
            const int tabW = std::min(260, avatar + nameW + 40);
            if (tabX + tabW > screenW - 24)
                break; // more users than fit: the shoulder buttons still reach them
            const float pop = framebuffer.spring(("picker.user." + std::to_string(index)).c_str(),
                                                 current ? 1.04f : 1.0f, 900.0f, current ? 33.0f : 60.0f, 1.0f);
            framebuffer.pushScale(tabX + tabW / 2.0f, tabsY + tabH / 2.0f, pop);
            if (current)
                framebuffer.pkSelectedButton(tabX, tabsY, tabW, tabH, 4);
            else
                framebuffer.pkBlackButton(tabX, tabsY, tabW, tabH, 4);
            const int avX = tabX + 10, avY = tabsY + (tabH - avatar) / 2;
            const IconImage &av = SystemIcons::userIcon(user.accountUid);
            if (av.valid())
                framebuffer.drawImageScaled(avX, avY, av.width, av.height, avatar, avatar, av.data, 4);
            else
                framebuffer.drawFilledRoundedRect(avX, avY, avatar, avatar, 4, Pksm::LogoGrid);
            framebuffer.drawRoundedRect(avX, avY, avatar, avatar, 4, Pksm::LogoVoid, 2);
            framebuffer.drawText(avX + avatar + 12, framebuffer.textYCenteredOn(tabsY + tabH / 2),
                                 fitWidth(framebuffer, user.name, tabW - avatar - 34, TextStyle::Body),
                                 current ? Pksm::Ink : Pksm::InkSoft);
            framebuffer.popTransform();
            if (current && focusZone == 0)
                framebuffer.pkSelection(tabX - 4, tabsY - 4, tabW + 8, tabH + 8);
            userRects.push_back({tabX, tabsY, tabW, tabH, index});
            tabX += tabW + 10;
        }

        // -- "Games" + a count chip, like the shelf's header row. ---------------------------------
        constexpr int gamesY = tabsY + tabH + 18;
        framebuffer.drawText(24, gamesY, "Games", Pksm::Ink, TextStyle::Heading);
        const int count = titleCount();
        {
            const std::string chip = std::to_string(count) + (count == 1 ? " save" : " saves");
            int cw, ch;
            framebuffer.measureText(chip, cw, ch, TextStyle::Body);
            const int chipW = cw + 28, chipH = 36, chipX = screenW - 24 - chipW;
            framebuffer.pkBlackButton(chipX, gamesY - 2, chipW, chipH, 4);
            framebuffer.drawText(chipX + 14, framebuffer.textYCenteredOn(gamesY - 2 + chipH / 2), chip, Pksm::Ink);
        }

        // -- The cartridge shelf. -----------------------------------------------------------------
        constexpr int shelfY = gamesY + 44, tileW = 196, tileH = 214, tileGap = 16;
        constexpr int shelfX = 16;
        const int shelfW = screenW - 32;
        if (count == 0)
        {
            const char *message = "No Pokémon saves for this trainer.  Y opens a save file.";
            int mw, mh;
            framebuffer.measureText(message, mw, mh, TextStyle::Body);
            framebuffer.drawText((screenW - mw) / 2, shelfY + tileH / 2 - mh / 2, message, Pksm::InkSoft);
        }
        else
        {
            // Centre the chosen cart, within the ends of the row; the row glides there on a spring
            // (no overshoot) instead of jumping, and snaps when the trainer changes.
            const int contentW = count * tileW + (count - 1) * tileGap;
            const float maxScroll = static_cast<float>(std::max(0, contentW - shelfW));
            const float wanted = std::clamp(titleIndex * (tileW + tileGap) + tileW / 2.0f - shelfW / 2.0f, 0.0f,
                                            maxScroll);
            const float scroll = framebuffer.spring(("picker.shelf." + std::to_string(userIndex)).c_str(), wanted,
                                                    260.0f, 2.0f * std::sqrt(260.0f), wanted);
            const int originX = contentW < shelfW ? shelfX + (shelfW - contentW) / 2
                                                  : shelfX - static_cast<int>(std::lround(scroll));

            // Room above and below for the focus pop, so a scaled tile is not cut by the clip.
            framebuffer.setClipRect(shelfX, shelfY - 10, shelfW, tileH + 20);
            for (int index = 0; index < count; ++index)
            {
                const int tileX = originX + index * (tileW + tileGap);
                if (tileX + tileW < shelfX - 20 || tileX > shelfX + shelfW + 20)
                    continue;
                const TitleEntry &title = u->titles[index];
                const bool chosen = index == titleIndex;
                const bool selected = chosen && focusZone == 1; // the shelf has focus
                const int generation = titleGeneration(title);

                const float pop = framebuffer.spring(("picker.tile." + std::to_string(index)).c_str(),
                                                     selected ? 1.05f : 1.0f, 900.0f, selected ? 33.0f : 60.0f, 1.0f);
                framebuffer.pushScale(tileX + tileW / 2.0f, shelfY + tileH / 2.0f, pop);

                // Device panel, or the selected look: cobalt body, pale edge.
                if (selected)
                    framebuffer.pkSelectedButton(tileX, shelfY, tileW, tileH, 6);
                else
                    framebuffer.pkPanel(tileX, shelfY, tileW, tileH);
                if (chosen && !selected)
                    framebuffer.drawRoundedRect(tileX, shelfY, tileW, tileH, 6, Pksm::LogoCyan, 2); // chosen, not focused

                // The cartridge: era plastic, a dark contact strip, the label sticker with the art.
                constexpr int cartW = 132, cartH = 146;
                const int cartX = tileX + (tileW - cartW) / 2, cartY = shelfY + 14;
                const Color plastic = eraColor(generation);
                framebuffer.drawFilledRoundedRect(cartX + 3, cartY + 3, cartW, cartH, 7, Color(0x14, 0x12, 0x1D, 0x88));
                framebuffer.drawFilledRoundedRect(cartX, cartY, cartW, cartH, 7, plastic);
                framebuffer.drawRoundedRect(cartX, cartY, cartW, cartH, 7, darker(plastic, 0.25f), 2);
                framebuffer.drawFilledRect(cartX + 6, cartY + 2, cartW - 12, 12, Pksm::LogoVoid);
                const int stickerX = cartX + 9, stickerY = cartY + 19, stickerW = cartW - 18, stickerH = cartH - 28;
                framebuffer.drawFilledRoundedRect(stickerX, stickerY, stickerW, stickerH, 4, Pksm::PaperShade);
                framebuffer.drawRoundedRect(stickerX, stickerY, stickerW, stickerH, 4, Pksm::LogoVoid, 1);
                const int art = std::min(stickerW, stickerH) - 8;
                const int artX = stickerX + (stickerW - art) / 2, artY = stickerY + (stickerH - art) / 2;
                if (const IconImage *icon = titleIconFor(title))
                    framebuffer.drawImageScaled(artX, artY, icon->width, icon->height, art, art, icon->data, 4);
                else
                {
                    // No art: the console code as the identity mark.
                    const char *code = consoleCode(generation);
                    int cw, ch;
                    framebuffer.measureText(code, cw, ch, TextStyle::Heading);
                    framebuffer.drawText(stickerX + (stickerW - cw) / 2,
                                         framebuffer.textYCenteredOn(stickerY + stickerH / 2, TextStyle::Heading), code,
                                         plastic, TextStyle::Heading);
                }

                // Name, then what tells two saves apart: the console, and where a loose save lives.
                const int textW = tileW - 20;
                const std::string name = fitWidth(framebuffer, title.label, textW, TextStyle::Body);
                int nw, nh;
                framebuffer.measureText(name, nw, nh, TextStyle::Body);
                framebuffer.drawText(tileX + (tileW - nw) / 2, cartY + cartH + 14, name, Pksm::Ink);
                std::string detail = titleConsole(title);
                if (!title.filePath.empty())
                    detail += std::string(detail.empty() ? "" : " · ") + looseEmulator(title.filePath);
                detail = fitWidth(framebuffer, detail, textW, TextStyle::Caption);
                int dw, dh;
                framebuffer.measureText(detail, dw, dh, TextStyle::Caption);
                framebuffer.drawText(tileX + (tileW - dw) / 2, cartY + cartH + 14 + nh + 4, detail,
                                     selected ? Pksm::Ink : Pksm::InkSoft, TextStyle::Caption);

                framebuffer.popTransform();
                // Only the visible part of a tile is tappable (the clip hides the rest).
                const int hitX = std::max(tileX, shelfX), hitR = std::min(tileX + tileW, shelfX + shelfW);
                if (hitR > hitX)
                    titleRects.push_back({hitX, shelfY, hitR - hitX, tileH, index});
            }
            framebuffer.clearClip();

            // Edge chevrons when the row runs on past the screen.
            if (contentW > shelfW)
            {
                const int midY = shelfY + tileH / 2;
                if (scroll > 1.0f)
                    framebuffer.drawSymbol(shelfX + 2, midY - 12, "\xE2\x97\x80", Pksm::LogoCyan);
                if (scroll < maxScroll - 1.0f)
                    framebuffer.drawSymbol(shelfX + shelfW - 22, midY - 12, "\xE2\x96\xB6", Pksm::LogoCyan);
            }

            // -- The chosen save in full (the lower screen's "game library" card). ------------------
            // Four labelled fields in two columns, rather than one run-on line and a raw SD path: what
            // console, whose (or which emulator's) save it is, the file, and where it lives.
            const TitleEntry &title = u->titles[titleIndex];
            const int infoY = shelfY + tileH + 12, infoH = 104;
            framebuffer.pkPanel(16, infoY, screenW - 32, infoH);
            framebuffer.pkHeaderStrip(26, infoY + 8, screenW - 52, 32,
                                      fitWidth(framebuffer, title.name, screenW - 100, TextStyle::Body),
                                      Pksm::HeaderBlue, false, TextStyle::Body);

            const bool loose = !title.filePath.empty();
            std::string saveText, folderText;
            if (loose)
            {
                const size_t slash = title.filePath.find_last_of('/');
                saveText = slash == std::string::npos ? title.filePath : title.filePath.substr(slash + 1);
                folderText = slash == std::string::npos ? std::string() : title.filePath.substr(0, slash);
                if (folderText.rfind("sdmc:", 0) == 0)
                    folderText = folderText.substr(5);
                // Azahar names every save "main" under its title id; the id is what identifies it.
                const size_t titleAt = title.filePath.find("/title/");
                if (saveText == "main" && titleAt != std::string::npos)
                {
                    const std::string rest = title.filePath.substr(titleAt + 7); // 00040000/<id>/data/...
                    const size_t idSlash = rest.find('/'), idEnd = rest.find('/', idSlash + 1);
                    if (idSlash != std::string::npos && idEnd != std::string::npos)
                        saveText = "3DS title " + rest.substr(idSlash + 1, idEnd - idSlash - 1);
                }
                struct stat st;
                if (stat(title.filePath.c_str(), &st) == 0)
                {
                    char when[32];
                    const time_t modified = st.st_mtime;
                    if (strftime(when, sizeof(when), "%d %b %Y, %H:%M", localtime(&modified)) > 0)
                        saveText += std::string("  ·  ") + when;
                }
            }
            else
            {
                char id[24];
                snprintf(id, sizeof(id), "%016lX", static_cast<unsigned long>(title.titleId));
                saveText = std::string("Live save data  ·  ") + id;
                folderText = "/PKSE/" + title.folder + "  (backups)";
            }

            struct Field
            {
                const char *label;
                std::string value;
            };
            const Field fields[4] = {
                {"Console", titleConsole(title)},
                {loose ? "Emulator" : "Trainer", loose ? looseEmulator(title.filePath) : u->name},
                {"Save", saveText},
                {"Folder", folderText},
            };
            // Four fields in a row, label over value; the long two (file, folder) get the wider columns.
            const int fieldsX = 30, fieldsW = screenW - 60, fieldTop = infoY + 46;
            const int widths[4] = {fieldsW * 14 / 100, fieldsW * 20 / 100, fieldsW * 30 / 100, fieldsW * 36 / 100};
            int fx = fieldsX;
            for (int index = 0; index < 4; ++index)
            {
                const int valueW = widths[index] - 18;
                framebuffer.drawText(fx, fieldTop, fields[index].label, Pksm::InkSoft, TextStyle::Caption);
                const std::string value = index == 3 ? fitMiddle(framebuffer, fields[index].value, valueW, TextStyle::Body)
                                                     : fitWidth(framebuffer, fields[index].value, valueW, TextStyle::Body);
                framebuffer.drawText(fx, fieldTop + 22, value, Pksm::Ink);
                if (index < 3)
                    framebuffer.drawFilledRect(fx + widths[index] - 9, fieldTop + 2, 2, 44, Pksm::LogoGrid);
                fx += widths[index];
            }
        }

        // -- The places that belong to no one game (PKForge's home cards). ------------------------
        {
            constexpr int cardsY = HEADER_HEIGHT + 14 + 56 + 18 + 44 + 214 + 12 + 104 + 10;
            const int cardsH = framebuffer.getHeight() - NAV_BAR_HEIGHT - 8 - cardsY;
            const int cardW = (screenW - 32 - 12) / 2;
            const size_t residents = PokeparkManager::instance().getResidents().size();
            struct Card
            {
                const char *icon, *title;
                std::string subtitle;
            };
            const Card cards[2] = {
                {"park", "Poképark", residents == 0 ? std::string("A hangout for Pokémon from all your games")
                                                     : std::to_string(residents) + (residents == 1 ? " resident" : " residents") +
                                                           " from all your games"},
                {"settings", "Settings", "Theme, text speed, sprites, backups"},
            };
            for (int index = 0; index < 2; ++index)
            {
                const int cx = 16 + index * (cardW + 12);
                const bool focused = focusZone == 2 && cardIndex == index;
                const float pop = framebuffer.spring(index == 0 ? "picker.card.0" : "picker.card.1", focused ? 1.03f : 1.0f,
                                                     900.0f, focused ? 33.0f : 60.0f, 1.0f);
                framebuffer.pushScale(cx + cardW / 2.0f, cardsY + cardsH / 2.0f, pop);
                if (focused)
                    framebuffer.pkSelectedButton(cx, cardsY, cardW, cardsH, 6);
                else
                    framebuffer.pkBlackButton(cx, cardsY, cardW, cardsH, 6);
                if (Sprite *icon = SpriteManager::getUiIcon(cards[index].icon); icon && icon->data)
                    framebuffer.drawSpriteFit(cx + 20, cardsY + (cardsH - 52) / 2, 52, 52, icon->width, icon->height,
                                              icon->data, icon->channels);
                framebuffer.drawText(cx + 90, cardsY + cardsH / 2 - 30, cards[index].title, Pksm::Ink, TextStyle::Heading);
                framebuffer.drawText(cx + 90, cardsY + cardsH / 2 + 4,
                                     fitWidth(framebuffer, cards[index].subtitle, cardW - 110, TextStyle::Caption),
                                     focused ? Pksm::Ink : Pksm::InkSoft, TextStyle::Caption);
                framebuffer.popTransform();
                cardRects.push_back({cx, cardsY, cardW, cardsH, index});
            }
        }

        if (fileBrowser.active)
            drawNavBar(framebuffer,
                       "Up/Down: Move  |  L/R: Page  |  A: Open  |  Y: Search  |  X: All Files  |  B: Up / Close");
        else if (users.size() > 1)
            drawNavBar(framebuffer, "A: Open  |  L/R: Trainer  |  Up/Down: Row  |  Y: Save File  |  -: Poképark  |  X: Settings  |  +: Exit");
        else
            drawNavBar(framebuffer, "A: Open  |  Up/Down: Row  |  Y: Save File  |  -: Poképark  |  X: Settings  |  +: Exit");

        if (fileBrowser.active)
        {
            PopIn pop(framebuffer, "picker.fileBrowser");
            Dialogs::drawFileBrowser(fileBrowser, framebuffer, browserTaps);
        }
    }

    void SaveSelectScreen::openFileBrowser()
    {
        browseError.clear();
        // Start where a Game Boy save plausibly is. Emulators on the Switch keep saves beside
        // their ROMs, and PKSE's own backup tree is the other likely home; the browser falls
        // through to the first of these that exists.
        fileBrowser.open("Open a Save File",
                         {"sdmc:/PKSE/saves/", "sdmc:/PKSE/", "sdmc:/roms/", "sdmc:/retroarch/saves/",
                          "sdmc:/emulators/", "sdmc:/"},
                         Save::saveFileExtensions(), Save::saveFileExactNames());
        logEventToFile("OPENSAVE action=BROWSE dir=\"" + fileBrowser.directory + "\"");
    }

    bool SaveSelectScreen::armTap(u64 button)
    {
        pendingTapButton = button;
        pendingTapFrames = TAP_PRESS_FRAMES;
        return true;
    }

    void SaveSelectScreen::handleFileBrowserInput(u64 buttonsDown, const TouchInput &touch)
    {
        if (buttonsDown & HidNpadButton_Up)
            fileBrowser.move(-1);
        if (buttonsDown & HidNpadButton_Down)
            fileBrowser.move(1);
        if (buttonsDown & HidNpadButton_L)
            fileBrowser.move(-Dialogs::FileBrowserState::VISIBLE_ROWS);
        if (buttonsDown & HidNpadButton_R)
            fileBrowser.move(Dialogs::FileBrowserState::VISIBLE_ROWS);
        if (buttonsDown & HidNpadButton_Y)
        {
            // Search. refresh() re-lists the folder through the query, so an empty
            // query -- the shared "clear" gesture -- simply lists it all again.
            if (fileBrowser.search.promptForQuery("Files"))
            {
                fileBrowser.returnTo.clear();
                fileBrowser.refresh();
            }
            return;
        }
        if (buttonsDown & HidNpadButton_X)
        {
            fileBrowser.showAllFiles = !fileBrowser.showAllFiles;
            fileBrowser.refresh();
        }
        if (buttonsDown & HidNpadButton_B)
        {
            if (!fileBrowser.goUp())
                fileBrowser.close();
            return;
        }

        bool activate = (buttonsDown & HidNpadButton_A) != 0;
        if (touch.justPressed())
        {
            for (const auto &r : browserTaps)
            {
                if (touch.x() >= r.hitX && touch.x() < r.hitX + r.hitWidth && touch.y() >= r.hitY &&
                    touch.y() < r.hitY + r.hitHeight)
                {
                    fileBrowser.selectedIndex = r.buttonId; // draws selected this frame
                    fileBrowser.move(0);
                    armTap(HidNpadButton_A);                // ...and opens on the next
                    return;
                }
            }
        }
        if (!activate || !fileBrowser.activate())
            return;

        const std::string picked = fileBrowser.chosenPath;
        fileBrowser.chosenPath.clear();

        // Validate BEFORE leaving the picker. Handing an unrecognised file to the editor would
        // drop the user into a blank trainer view with no way to tell what went wrong, so the
        // refusal is reported here, in the browser, with the file still on screen.
        size_t length = 0;
        uint8_t *bytes = Utils::readAllBytes(picked.c_str(), &length);
        if (!bytes)
        {
            fileBrowser.status = "Couldn't read that file.";
            logEventToFile("OPENSAVE action=PICK file=\"" + picked + "\" result=UNREADABLE");
            return;
        }
        // The SAME probe chain the editor will run, so the picker and the opener cannot disagree
        // about what is a save.
        const std::vector<uint8_t> probe(bytes, bytes + length);
        delete[] bytes;
        if (!Save::isExternalSave(probe))
        {
            // Naming the size is what turns "it didn't work" into something the user can act on:
            // a save that is the right size but not recognised is a different problem from a file
            // that was never a save.
            fileBrowser.status = "Not a save PKSE can open (" + std::to_string(length) + " bytes).";
            logEventToFile("OPENSAVE action=PICK file=\"" + picked + "\" size=" + std::to_string(length) +
                           " result=UNRECOGNISED");
            return;
        }

        selectedFilePath = picked;
        fileSelected = true;
        fileBrowser.close();
        logEventToFile("OPENSAVE action=PICK file=\"" + picked + "\" size=" + std::to_string(length) + " result=OK");
    }

    // ---- 3DS / Citra / Tico Emulator integration ----

    static bool isPokemonGame(const std::string &name)
    {
        std::string lower = name;
        for (char &c : lower)
            c = std::tolower(static_cast<unsigned char>(c));
        if (lower.find("pokemon") != std::string::npos ||
            lower.find("pokémon") != std::string::npos ||
            lower.find("pkmn") != std::string::npos ||
            lower.find("pocket monster") != std::string::npos)
        {
            return true;
        }
        static const char *const s_pkmnKeywords[] = {
            "crystal clear", "polished crystal", "polishedcrystal",
            "darkviolet", "dark violet", "radical red", "emerald seaglass",
            "inclement emerald", "unbound", "gaia", "firered", "leafgreen",
            "heartgold", "soulsilver", "omegaruby", "alphasapphire",
            "ultra sun", "ultra moon", "ultrasun", "ultramoon", "stadium 2",
            "trading card", "tcg"};
        for (const char *kw : s_pkmnKeywords)
        {
            if (lower.find(kw) != std::string::npos)
                return true;
        }
        return false;
    }

    static const char *get3DSGameName(const std::string &tidStr)
    {
        if (tidStr == "00055d00" || tidStr == "00055e00" || tidStr == "00055c00")
            return "Pokemon X";
        if (tidStr == "00055f00" || tidStr == "00056000")
            return "Pokemon Y";
        if (tidStr == "0011c400" || tidStr == "0011c600")
            return "Pokemon Omega Ruby";
        if (tidStr == "0011c500" || tidStr == "0011c700")
            return "Pokemon Alpha Sapphire";
        if (tidStr == "00164800")
            return "Pokemon Sun";
        if (tidStr == "00175e00")
            return "Pokemon Moon";
        if (tidStr == "001b5000")
            return "Pokemon Ultra Sun";
        if (tidStr == "001b5100")
            return "Pokemon Ultra Moon";
        return nullptr;
    }

    static std::string formatCleanLabel(const std::string &raw)
    {
        std::string s = raw;
        if (s.rfind("Pokemon - ", 0) == 0) s = s.substr(10);
        else if (s.rfind("Pokemon ", 0) == 0) s = s.substr(8);
        else if (s.rfind("Pokémon - ", 0) == 0) s = s.substr(11);
        else if (s.rfind("Pokémon ", 0) == 0) s = s.substr(9);
        else if (s.rfind("3DS_ Pokemon ", 0) == 0) s = s.substr(13);
        else if (s.rfind("GBA_ Pokemon ", 0) == 0) s = s.substr(13);
        else if (s.rfind("NDS_ Pokemon - ", 0) == 0) s = s.substr(15);
        else if (s.rfind("NDS_ Pokemon ", 0) == 0) s = s.substr(13);
        else if (s.rfind("GBC_ ", 0) == 0) s = s.substr(5);

        size_t vPos = s.find(" Version");
        if (vPos != std::string::npos) s = s.substr(0, vPos);
        size_t pPos = s.find(" (");
        if (pPos != std::string::npos) s = s.substr(0, pPos);

        if (s.find("polishedcrystal") != std::string::npos || s.find("Polishedcrystal") != std::string::npos)
            return "Polished Crystal";
        if (s.find("darkviolet") != std::string::npos || s.find("Darkviolet") != std::string::npos)
            return "Dark Violet";
        if (s == "Firered" || s == "firered")
            return "FireRed";
        if (s == "Leafgreen" || s == "leafgreen")
            return "LeafGreen";
        if (s.find("Emerald Seaglass") != std::string::npos)
            return "Emerald Seaglass";

        return s;
    }

    static std::string resolveGameIcon(const std::string &platform, const std::string &label, const std::string &savePath)
    {
        std::string platLower = platform;
        for (char &c : platLower)
            c = std::tolower(static_cast<unsigned char>(c));

        struct stat st;

        // Extract stem from savePath filename
        size_t lastSlash = savePath.find_last_of('/');
        std::string saveStem = (lastSlash != std::string::npos) ? savePath.substr(lastSlash + 1) : savePath;
        size_t lastDot = saveStem.find_last_of('.');
        if (lastDot != std::string::npos)
            saveStem = saveStem.substr(0, lastDot);

        std::string lblLower = label;
        for (char &c : lblLower)
            c = std::tolower(static_cast<unsigned char>(c));

        std::string stemLower = saveStem;
        for (char &c : stemLower)
            c = std::tolower(static_cast<unsigned char>(c));

        // 1. PRIORITIZE COVERS (Authentic legendary box art)
        // 1a. Direct match in sdmc:/tico/assets/covers/<plat>/<label>.{jpg,png}
        std::string c1j = "sdmc:/tico/assets/covers/" + platLower + "/" + label + ".jpg";
        if (stat(c1j.c_str(), &st) == 0) return c1j;
        std::string c1p = "sdmc:/tico/assets/covers/" + platLower + "/" + label + ".png";
        if (stat(c1p.c_str(), &st) == 0) return c1p;

        // 1b. Match with "Pokemon " prefix in covers
        std::string c2j = "sdmc:/tico/assets/covers/" + platLower + "/Pokemon " + label + ".jpg";
        if (stat(c2j.c_str(), &st) == 0) return c2j;
        std::string c2p = "sdmc:/tico/assets/covers/" + platLower + "/Pokemon " + label + ".png";
        if (stat(c2p.c_str(), &st) == 0) return c2p;

        // 1c. Match by save filename stem in covers
        if (!saveStem.empty() && saveStem != "main" && saveStem != "save00")
        {
            std::string c3j = "sdmc:/tico/assets/covers/" + platLower + "/" + saveStem + ".jpg";
            if (stat(c3j.c_str(), &st) == 0) return c3j;
            std::string c3p = "sdmc:/tico/assets/covers/" + platLower + "/" + saveStem + ".png";
            if (stat(c3p.c_str(), &st) == 0) return c3p;
        }

        // 1d. Directory search in covers/<plat>/ for matching title substring
        std::string coverDir = "sdmc:/tico/assets/covers/" + platLower;
        DIR *dC = opendir(coverDir.c_str());
        if (dC)
        {
            struct dirent *ent;
            while ((ent = readdir(dC)) != nullptr)
            {
                if (ent->d_name[0] == '.')
                    continue;
                std::string fn = ent->d_name;
                std::string fnLower = fn;
                for (char &c : fnLower)
                    c = std::tolower(static_cast<unsigned char>(c));
                bool isImage = (fn.find(".jpg") != std::string::npos || fn.find(".png") != std::string::npos);
                if (isImage)
                {
                    if (!lblLower.empty() && fnLower.find(lblLower) != std::string::npos)
                    {
                        std::string match = coverDir + "/" + fn;
                        closedir(dC);
                        return match;
                    }
                    if (!stemLower.empty() && stemLower != "main" && stemLower != "save00" &&
                        fnLower.find(stemLower) != std::string::npos)
                    {
                        std::string match = coverDir + "/" + fn;
                        closedir(dC);
                        return match;
                    }
                }
            }
            closedir(dC);
        }

        // 2. FALLBACK TO ICONS (if no cover found)
        std::string p1 = "sdmc:/tico/assets/icons/" + platLower + "/" + label + ".png";
        if (stat(p1.c_str(), &st) == 0) return p1;
        std::string p1j = "sdmc:/tico/assets/icons/" + platLower + "/" + label + ".jpg";
        if (stat(p1j.c_str(), &st) == 0) return p1j;

        std::string p1b = "sdmc:/tico/assets/icons/" + platLower + "/Pokemon " + label + ".png";
        if (stat(p1b.c_str(), &st) == 0) return p1b;
        std::string p1bj = "sdmc:/tico/assets/icons/" + platLower + "/Pokemon " + label + ".jpg";
        if (stat(p1bj.c_str(), &st) == 0) return p1bj;

        if (!saveStem.empty() && saveStem != "main" && saveStem != "save00")
        {
            std::string p3 = "sdmc:/tico/assets/icons/" + platLower + "/" + saveStem + ".png";
            if (stat(p3.c_str(), &st) == 0) return p3;
            std::string p3j = "sdmc:/tico/assets/icons/" + platLower + "/" + saveStem + ".jpg";
            if (stat(p3j.c_str(), &st) == 0) return p3j;
        }

        std::string iconDir = "sdmc:/tico/assets/icons/" + platLower;
        DIR *dI = opendir(iconDir.c_str());
        if (dI)
        {
            struct dirent *ent;
            while ((ent = readdir(dI)) != nullptr)
            {
                if (ent->d_name[0] == '.')
                    continue;
                std::string fn = ent->d_name;
                std::string fnLower = fn;
                for (char &c : fnLower)
                    c = std::tolower(static_cast<unsigned char>(c));
                bool isImage = (fn.find(".png") != std::string::npos || fn.find(".jpg") != std::string::npos);
                if (isImage)
                {
                    if (!lblLower.empty() && fnLower.find(lblLower) != std::string::npos)
                    {
                        std::string match = iconDir + "/" + fn;
                        closedir(dI);
                        return match;
                    }
                    if (!stemLower.empty() && stemLower != "main" && stemLower != "save00" &&
                        fnLower.find(stemLower) != std::string::npos)
                    {
                        std::string match = iconDir + "/" + fn;
                        closedir(dI);
                        return match;
                    }
                }
            }
            closedir(dI);
        }

        // 4. Bundled RomFS fallback for vanilla and romhack games
        std::string l = label;
        for (char &c : l)
            c = std::tolower(static_cast<unsigned char>(c));

        if (l.find("firered") != std::string::npos || l.find("fire red") != std::string::npos || l.find("darkviolet") != std::string::npos || l.find("dark violet") != std::string::npos)
            return "romfs:/icons/firered.png";
        if (l.find("leafgreen") != std::string::npos || l.find("leaf green") != std::string::npos)
            return "romfs:/icons/leafgreen.png";
        if (l.find("emerald") != std::string::npos || l.find("seaglass") != std::string::npos)
            return "romfs:/icons/emerald.png";
        if (l.find("alpha sapphire") != std::string::npos || l.find("alphasapphire") != std::string::npos)
            return "romfs:/icons/alphasapphire.png";
        if (l.find("omega ruby") != std::string::npos || l.find("omegaruby") != std::string::npos)
            return "romfs:/icons/omegaruby.png";
        if (l.find("ruby") != std::string::npos)
            return "romfs:/icons/ruby.png";
        if (l.find("sapphire") != std::string::npos)
            return "romfs:/icons/sapphire.png";
        if (l.find("pokemon x") != std::string::npos || l == "x" || l == "pokemon - x")
            return "romfs:/icons/x.png";
        if (l.find("pokemon y") != std::string::npos || l == "y" || l == "pokemon - y")
            return "romfs:/icons/y.png";
        if (l.find("heartgold") != std::string::npos || l.find("heart gold") != std::string::npos)
            return "romfs:/icons/heartgold.png";
        if (l.find("soulsilver") != std::string::npos || l.find("soul silver") != std::string::npos)
            return "romfs:/icons/soulsilver.png";
        if (l.find("platinum") != std::string::npos)
            return "romfs:/icons/platinum.png";
        if (l.find("diamond") != std::string::npos)
            return "romfs:/icons/diamond.png";
        if (l.find("pearl") != std::string::npos)
            return "romfs:/icons/pearl.png";
        if (l.find("black 2") != std::string::npos || l.find("black2") != std::string::npos)
            return "romfs:/icons/black2.png";
        if (l.find("white 2") != std::string::npos || l.find("white2") != std::string::npos)
            return "romfs:/icons/white2.png";
        if (l.find("black") != std::string::npos)
            return "romfs:/icons/black.png";
        if (l.find("white") != std::string::npos)
            return "romfs:/icons/white.png";
        if (l.find("ultra sun") != std::string::npos || l.find("ultrasun") != std::string::npos)
            return "romfs:/icons/ultrasun.png";
        if (l.find("ultra moon") != std::string::npos || l.find("ultramoon") != std::string::npos)
            return "romfs:/icons/ultramoon.png";
        if (l.find("sun") != std::string::npos)
            return "romfs:/icons/sun.png";
        if (l.find("moon") != std::string::npos)
            return "romfs:/icons/moon.png";
        if (l.find("crystal") != std::string::npos || l.find("polished") != std::string::npos)
            return "romfs:/icons/crystal.png";
        if (l.find("gold") != std::string::npos)
            return "romfs:/icons/gold.png";
        if (l.find("silver") != std::string::npos)
            return "romfs:/icons/silver.png";
        if (l.find("yellow") != std::string::npos)
            return "romfs:/icons/yellow.png";
        if (l.find("red") != std::string::npos)
            return "romfs:/icons/red.png";
        if (l.find("blue") != std::string::npos)
            return "romfs:/icons/blue.png";
        if (l.find("stadium 2") != std::string::npos || l.find("stadium2") != std::string::npos)
            return "romfs:/icons/stadium2.png";
        if (l.find("trading card") != std::string::npos || l.find("tcg") != std::string::npos)
            return "romfs:/icons/tcg.png";

        return "romfs:/icons/tico.png";
    }

    void SaveSelectScreen::scanCitraDir(UserEntry &user, const std::string &basePath)
    {
        DIR *d0 = opendir(basePath.c_str());
        if (!d0)
            return;
        struct dirent *ent0;
        while ((ent0 = readdir(d0)) != nullptr)
        {
            if (ent0->d_name[0] == '.')
                continue;
            // Built as strings: a fixed buffer could cut a long emulator path short.
            const std::string path0 = basePath + "/" + ent0->d_name;
            DIR *d1 = opendir(path0.c_str());
            if (!d1)
                continue;
            struct dirent *ent1;
            while ((ent1 = readdir(d1)) != nullptr)
            {
                if (ent1->d_name[0] == '.')
                    continue;
                const std::string titleDir = path0 + "/" + ent1->d_name + "/title/00040000";
                DIR *dt = opendir(titleDir.c_str());
                if (!dt)
                    continue;
                struct dirent *entT;
                while ((entT = readdir(dt)) != nullptr)
                {
                    if (entT->d_name[0] == '.')
                        continue;
                    std::string tid = entT->d_name;
                    const std::string mainPathText = titleDir + "/" + tid + "/data/00000001/main";
                    const char *mainPath = mainPathText.c_str();
                    struct stat st;
                    if (stat(mainPath, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0x65600)
                    {
                        const char *gName = get3DSGameName(tid);
                        if (!gName)
                            continue;
                        std::string displayLabel = formatCleanLabel(gName);

                        bool duplicate = false;
                        for (auto &existing : user.titles)
                        {
                            if (existing.filePath == mainPath)
                            {
                                duplicate = true;
                                break;
                            }
                            if (existing.label == displayLabel)
                            {
                                // Canonical Citra save always supersedes any flat or loose save
                                existing.filePath = std::string(mainPath);
                                existing.iconPath = resolveGameIcon("3DS", displayLabel, mainPath);
                                duplicate = true;
                                break;
                            }
                        }
                        if (duplicate)
                            continue;

                        TitleEntry t;
                        t.titleId = 0;
                        t.filePath = std::string(mainPath);
                        t.label = displayLabel;
                        t.name = displayLabel;
                        t.folder = displayLabel;
                        t.iconPath = resolveGameIcon("3DS", displayLabel, mainPath);

                        char m[256];
                        snprintf(m, sizeof(m), "Citra: found 3DS save: %s (%s, %ld bytes, icon: %s)", mainPath,
                                 displayLabel.c_str(), (long)st.st_size, t.iconPath.c_str());
                        logInfoToFile(m);

                        user.titles.push_back(std::move(t));
                    }
                }
                closedir(dt);
            }
            closedir(d1);
        }
        closedir(d0);
    }

    void SaveSelectScreen::scanTicoDir(UserEntry &user, const std::string &dir, const std::string &platform)
    {
        DIR *d = opendir(dir.c_str());
        if (!d)
            return;
        struct dirent *ent;
        const bool is3DS = (platform == "3DS");

        while ((ent = readdir(d)) != nullptr)
        {
            std::string name = ent->d_name;
            if (name == "." || name == "..")
                continue;

            std::string fullPath = dir + "/" + name;
            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0)
                continue;

            if (S_ISDIR(st.st_mode))
            {
                if (name == "melonDS" || name == "states" || name == "cheats" || name[0] == '.')
                    continue;
                DIR *subD = opendir(fullPath.c_str());
                if (subD)
                {
                    struct dirent *subEnt;
                    while ((subEnt = readdir(subD)) != nullptr)
                    {
                        std::string subName = subEnt->d_name;
                        if (subName == "." || subName == "..")
                            continue;
                        std::string subFullPath = fullPath + "/" + subName;
                        struct stat subSt;
                        if (stat(subFullPath.c_str(), &subSt) != 0 || !S_ISREG(subSt.st_mode))
                            continue;

                        bool isSubSave = false;
                        std::string rawLabel = subName;
                        if (subName == "main")
                        {
                            isSubSave = true;
                            rawLabel = name;
                        }
                        else if (subName.size() > 4)
                        {
                            std::string ext = subName.substr(subName.size() - 4);
                            if (ext == ".sav" || ext == ".SAV" || ext == ".Sav" || ext == ".dsv")
                            {
                                isSubSave = true;
                                rawLabel = subName.substr(0, subName.size() - 4);
                            }
                        }
                        if (!isSubSave)
                            continue;

                        std::string displayLabel = formatCleanLabel(rawLabel);
                        if (!isPokemonGame(displayLabel) && !isPokemonGame(rawLabel) && !isPokemonGame(name))
                            continue;

                        bool duplicate = false;
                        for (const auto &existing : user.titles)
                        {
                            if (existing.filePath == subFullPath || existing.label == displayLabel)
                            {
                                duplicate = true;
                                break;
                            }
                        }
                        if (duplicate)
                            continue;

                        TitleEntry t;
                        t.titleId = 0;
                        t.filePath = subFullPath;
                        t.label = displayLabel;
                        t.name = displayLabel;
                        t.folder = displayLabel;
                        t.iconPath = resolveGameIcon(platform, displayLabel, subFullPath);

                        char m[256];
                        snprintf(m, sizeof(m), "Tico: found %s save: %s (size %ld, icon: %s)", platform.c_str(),
                                 subFullPath.c_str(), (long)subSt.st_size, t.iconPath.c_str());
                        logInfoToFile(m);

                        user.titles.push_back(std::move(t));
                    }
                    closedir(subD);
                }
                continue;
            }

            if (!S_ISREG(st.st_mode))
                continue;

            bool isSave = false;
            std::string rawLabel = name;

            if (name == "main")
            {
                isSave = true;
                size_t pSlash = dir.find_last_of('/');
                rawLabel = (pSlash != std::string::npos) ? dir.substr(pSlash + 1) : "3DS Save";
            }
            else if (name.size() > 4)
            {
                std::string ext = name.substr(name.size() - 4);
                if (ext == ".sav" || ext == ".SAV" || ext == ".Sav" || ext == ".dsv")
                {
                    isSave = true;
                    rawLabel = name.substr(0, name.size() - 4);
                }
                else if (is3DS && (ext == ".bin" || ext == ".BIN"))
                {
                    isSave = true;
                    rawLabel = name.substr(0, name.size() - 4);
                }
            }

            if (!isSave)
                continue;

            std::string displayLabel = formatCleanLabel(rawLabel);
            if (!isPokemonGame(displayLabel) && !isPokemonGame(rawLabel))
                continue;

            bool duplicate = false;
            for (const auto &existing : user.titles)
            {
                if (existing.filePath == fullPath || existing.label == displayLabel)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
                continue;

            TitleEntry t;
            t.titleId = 0;
            t.filePath = fullPath;
            t.label = displayLabel;
            t.name = displayLabel;
            t.folder = displayLabel;
            t.iconPath = resolveGameIcon(platform, displayLabel, fullPath);

            char m[256];
            snprintf(m, sizeof(m), "Tico: found %s save: %s (size %ld, icon: %s)", platform.c_str(), fullPath.c_str(),
                     (long)st.st_size, t.iconPath.c_str());
            logInfoToFile(m);

            user.titles.push_back(std::move(t));
        }
        closedir(d);
    }

    void SaveSelectScreen::loadTicoSaves()
    {
        UserEntry tico;
        tico.name = "Tico Emulator";
        memset(&tico.accountUid, 0xFF, sizeof(AccountUid)); // sentinel UID (0xFF...FF)

        // 1. Scan Azahar / Citra 3DS simulated SD card saves FIRST.
        // Citra/Azahar saves to sdmc:/tico/system/3ds/sdmc/Nintendo 3DS/.../title/00040000/<tid>/data/00000001/main.
        // Prioritizing this ensures the authentic live 3DS save is always targeted over loose or stale files.
        scanCitraDir(tico, "sdmc:/tico/system/3ds/sdmc/Nintendo 3DS");

        // 2. Scan standard Tico save directories (GBA, GBC, NDS, 3DS)
        scanTicoDir(tico, "sdmc:/tico/saves/gba", "GBA");
        scanTicoDir(tico, "sdmc:/tico/saves/gbc", "GBC");
        scanTicoDir(tico, "sdmc:/tico/saves/nds", "NDS");
        scanTicoDir(tico, "sdmc:/tico/saves/3ds", "3DS");

        // 3. Scan Tico ROM directories
        scanTicoDir(tico, "sdmc:/tico/roms/gba", "GBA");
        scanTicoDir(tico, "sdmc:/tico/roms/gbc", "GBC");
        scanTicoDir(tico, "sdmc:/tico/roms/nds", "NDS");
        scanTicoDir(tico, "sdmc:/tico/roms/3ds", "3DS");

        // 4. Scan RetroArch NDS ROMs & Standalone melonDS directories
        scanTicoDir(tico, "sdmc:/retroarch/ROMS/NDS", "NDS");
        scanTicoDir(tico, "sdmc:/retroarch/ROMS/NDS/melonDS", "NDS");
        scanTicoDir(tico, "sdmc:/retroarch/cores/savefiles/melonDS", "NDS");

        if (!tico.titles.empty())
        {
            char m[128];
            snprintf(m, sizeof(m), "Tico: %d emulator saves found", (int)tico.titles.size());
            logInfoToFile(m);

            if (users.size() == 1 && users[0].titles.empty())
            {
                users.clear();
            }
            users.push_back(std::move(tico));
        }
    }
}

