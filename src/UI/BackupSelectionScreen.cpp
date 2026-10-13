#include <cmath>
#include <algorithm>
#include <cstdio>

#include "UI/BackupSelectionScreen.h"
#include "UI/Common.h"
#include "UI/ScreenChrome.h"
#include "UI/TouchInput.h"
#include "UI/SystemIcons.h"
#include "UI/Dialogs/DialogFrame.h"
#include "Utils/HelperUtilities.h"
#include "Utils/FileUtilities.h"
#include "Globals.h"

using namespace Utils;

namespace UI
{
    // UI Layout constants
    // Where things go. The PKForge theme shows the backups as a popup window over the grid -- the
    // game on the left, its saves on the right -- like PKForge's save menu; the other themes keep the
    // full-width card. Draw and touch both read this, so a tap always lands on what is drawn.
    struct BackupLayout
    {
        int cardX, cardY, cardW, cardH; // the card / window
        int listX, listW, listTop;      // the rows
        int rowH, visible;              // row pitch, rows before scrolling
    };

    static BackupLayout backupLayout()
    {
        if (isPkForgeTheme())
        {
            constexpr int w = 960, h = 576, x = (1280 - w) / 2, y = HEADER_HEIGHT + 14;
            return {x, y, w, h, x + 280, w - 280 - 22, y + 62, 56, 8};
        }
        constexpr int x = 40, y = 84, w = 1200;
        return {x, y, w, 720 - y - NAV_BAR_HEIGHT - 12, x + 14, w - 28, y + 62, 62, 8};
    }

    // Scroll window: keep the selected row visible (centered when scrolling).
    static int firstVisibleRow(int selectedIndex, int total)
    {
        const int visible = backupLayout().visible;
        if (total <= visible)
            return 0;
        int first = selectedIndex - visible / 2;
        if (first < 0)
            first = 0;
        if (first > total - visible)
            first = total - visible;
        return first;
    }

    BackupSelectionScreen::BackupSelectionScreen(u64 titleId, const std::string &titleName,
                                                 const std::string &titleFolder)
        : titleId(titleId), titleName(titleName), selectedIndex(0), backupSelected(false),
          createNewBackup(false), goBack(false), showDeleteConfirmation(false), deleteConfirmationIndex(-1)
    {
        // THE PATH TAKES THE FOLDER, NOT THE DISPLAY NAME. One string for both is how every Gen 3 language
        // SKU ends up sharing one backup directory -- see Save/TitleFolder.h.
        char gameDirBuf[512];
        snprintf(gameDirBuf, sizeof(gameDirBuf), "%s/%s", BASE_SAVE_DIRECTORY.c_str(), titleFolder.c_str());
        gameDirectory = gameDirBuf;

        loadBackups();
    }

    void BackupSelectionScreen::loadBackups()
    {
        std::vector<std::string> backupDirs = listBackupDirectories(gameDirectory.c_str());

        // Add the "Load from Title" action. With auto-backup ON this creates a new timestamped
        // backup; with it OFF the load reuses a single "Working" copy (see UIManager::handleBackupSelection),
        // so label it honestly rather than always promising a new backup.
        BackupInfo newBackupOption;
        newBackupOption.timestamp = "";
        newBackupOption.displayName = g_autoBackupEnabled
                                          ? "Load from Title (Create New Backup)"
                                          : "Load from Title (Working Copy)";
        backups.push_back(newBackupOption);

        for (const auto &timestamp : backupDirs)
        {
            BackupInfo info;
            info.timestamp = timestamp;
            info.displayName = formatTimestamp(timestamp);
            backups.push_back(info);
        }
    }

    std::string BackupSelectionScreen::formatTimestamp(const std::string &timestamp)
    {
        // Convert YYYYMMDD_HHMMSS to readable format: YYYY-MM-DD HH:MM:SS
        if (timestamp.length() != 15)
            return timestamp;

        std::string formatted;
        formatted += timestamp.substr(0, 4); // YYYY
        formatted += "-";
        formatted += timestamp.substr(4, 2); // MM
        formatted += "-";
        formatted += timestamp.substr(6, 2); // DD
        formatted += " ";
        formatted += timestamp.substr(9, 2); // HH
        formatted += ":";
        formatted += timestamp.substr(11, 2); // MM
        formatted += ":";
        formatted += timestamp.substr(13, 2); // SS

        return formatted;
    }

    /// Top of the list, under the heading divider. Shared by the draw and the touch hit test --
    /// when those two disagreed, every tap landed one row off.
    int BackupSelectionScreen::backupListTop() const { return backupLayout().listTop; }

    void BackupSelectionScreen::update(const PadState &pad, const TouchInput &touch)
    {
        u64 buttonsDown = padGetButtonsDown(&pad) | navTouchButton(touch); // nav-bar badges are tappable

        // A tap recorded earlier fires once its press has actually been on screen. See armTap().
        if (pendingTapButton != 0 && --pendingTapFrames <= 0)
        {
            buttonsDown |= pendingTapButton;
            pendingTapButton = 0;
            pendingTapButtonId = -1;
        }
        // expire the failure notice
        if (statusFrames > 0) --statusFrames;

        // A dialog still typing its message: A (or a tap) finishes it rather than answering unseen.
        if (PKSEFramebuffer::typingInProgress() && ((buttonsDown & HidNpadButton_A) || touch.justPressed()))
        {
            PKSEFramebuffer::skipTyping();
            return;
        }

        if (showDeleteConfirmation)
        {
            // Tappable buttons (captured last frame): A = Delete, B = Cancel.
            if (touch.justPressed())
            {
                auto isPressed = [&](const DlgBtn &b)
                {
                    return touch.x() >= b.hitX && touch.x() < b.hitX + b.hitWidth &&
                           touch.y() >= b.hitY && touch.y() < b.hitY + b.hitHeight;
                };
                if (isPressed(deleteDeleteBtn) && armTap(1, HidNpadButton_A))
                    return;
                if (isPressed(deleteCancelBtn) && armTap(0, HidNpadButton_B))
                    return;
            }
            if (buttonsDown & HidNpadButton_A)
            {
                deleteBackup(deleteConfirmationIndex);
                showDeleteConfirmation = false;
                deleteConfirmationIndex = -1;
                return;
            }
            if (buttonsDown & HidNpadButton_B)
            {
                // Cancel deletion
                showDeleteConfirmation = false;
                deleteConfirmationIndex = -1;
                return;
            }
            return; // Ignore other inputs while confirmation is shown
        }

        // Touch: tap a backup tile to select + open it (account for the scroll window).
        if (touch.justPressed())
        {
            const BackupLayout layout = backupLayout();
            const int startY = layout.listTop, tileX = layout.listX, tileW = layout.listW;
            if (touch.x() >= tileX && touch.x() < tileX + tileW && touch.y() >= startY)
            {
                int visIdx = (touch.y() - startY) / layout.rowH;
                int index = firstVisibleRow(selectedIndex, (int)backups.size()) + visIdx;
                if (visIdx >= 0 && visIdx < layout.visible && index < (int)backups.size())
                {
                    selectedIndex = index;              // draws selected this frame
                    // ...and opens on the next
                    if (armTap(-1, HidNpadButton_A)) return;
                }
            }
        }

        if (buttonsDown & HidNpadButton_B)
        {
            goBack = true;
            return;
        }

        if (buttonsDown & HidNpadButton_X)
        {
            // X button pressed - show delete confirmation for existing backups only
            if (selectedIndex > 0 && selectedIndex < (int)backups.size())
            {
                showDeleteConfirmation = true;
                deleteConfirmationIndex = selectedIndex;
            }
        }

        if (buttonsDown & HidNpadButton_Up)
        {
            if (selectedIndex > 0)
            {
                selectedIndex--;
            }
        }

        if (buttonsDown & HidNpadButton_Down)
        {
            if (selectedIndex < (int)backups.size() - 1)
            {
                selectedIndex++;
            }
        }

        if (buttonsDown & HidNpadButton_A)
        {
            const int row = selectedIndex;
            if (row < 0 || row >= (int)backups.size())
                return;
            if (row == 0)
            {
                // First option: Load from Title (Create New Backup)
                createNewBackup = true;
                backupSelected = true;
            }
            else
            {
                // Existing backup selected
                createNewBackup = false;
                char backupPath[512];
                snprintf(backupPath, sizeof(backupPath), "%s/%s",
                         gameDirectory.c_str(), backups[row].timestamp.c_str());
                selectedBackupPath = backupPath;
                backupSelected = true;
            }
        }
    }

    void BackupSelectionScreen::draw(PKSEFramebuffer &framebuffer)
    {
        framebuffer.clear(Colors::Background);
        drawTitleBar(framebuffer, titleName, "Backups");
        const BackupLayout layout = backupLayout();
        const char *hint = selectedIndex > 0 ? "A: Open  |  X: Delete  |  B: Back" : "A: Open  |  B: Back";

        if (isPkForgeTheme())
        {
            PopIn pop(framebuffer, "backup.window");
            framebuffer.drawFilledRect(0, 0, framebuffer.getWidth(), framebuffer.getHeight(), Color(0x14, 0x12, 0x1D, 0x66));
            framebuffer.pkPanel(layout.cardX, layout.cardY, layout.cardW, layout.cardH);
            framebuffer.pkHeaderStrip(layout.cardX + 10, layout.cardY + 10, layout.cardW - 20, 38, "Choose a save to open",
                                      Pksm::HeaderBlue, false, TextStyle::Heading);

            // The game, on the left: its art on a label sticker, its name, how many backups it has.
            const int colX = layout.cardX + 24, colW = 236, artSize = 176;
            const int artX = colX + (colW - artSize) / 2, artY = layout.cardY + 66;
            framebuffer.drawFilledRoundedRect(artX - 8, artY - 8, artSize + 16, artSize + 16, 8, Pksm::PaperShade);
            framebuffer.drawRoundedRect(artX - 8, artY - 8, artSize + 16, artSize + 16, 8, Pksm::LogoVoid, 2);
            const IconImage &icon = SystemIcons::titleIcon(titleId);
            if (icon.valid())
                framebuffer.drawImageScaled(artX, artY, icon.width, icon.height, artSize, artSize, icon.data, 4);
            int textY = artY + artSize + 22;
            for (const auto &line : wrapTextToWidth(framebuffer, titleName, colW, TextStyle::Body))
            {
                framebuffer.drawText(colX, textY, line, Pksm::Ink);
                textY += 26;
            }
            const int backupCount = static_cast<int>(backups.size()) - 1;
            framebuffer.drawText(colX, textY + 4,
                                 backupCount == 0 ? std::string("No backups yet")
                                                  : std::to_string(backupCount) + (backupCount == 1 ? " backup" : " backups"),
                                 Pksm::InkSoft, TextStyle::Caption);
            // What the highlighted row will do.
            const std::string about =
                selectedIndex == 0 ? (g_autoBackupEnabled ? "Reads the live save, keeping a dated backup of it first."
                                                          : "Reads the live save into a working copy.")
                                   : "Opens this backup. Your live save is not touched.";
            int aboutY = textY + 36;
            for (const auto &line : wrapTextToWidth(framebuffer, about, colW, TextStyle::Caption))
            {
                framebuffer.drawText(colX, aboutY, line, Pksm::InkSoft, TextStyle::Caption);
                aboutY += 20;
            }
            framebuffer.drawFilledRect(layout.listX - 16, layout.listTop, 2, layout.visible * layout.rowH, Pksm::LogoGrid);

            drawBackupList(framebuffer);
            drawPopupHints(framebuffer, layout.cardX + 12, layout.cardY + layout.cardH - 50, layout.cardW - 24, 40, hint);
        }
        else
        {
            framebuffer.drawCard(layout.cardX, layout.cardY, layout.cardW, layout.cardH);
            framebuffer.drawText(layout.cardX + 18, layout.cardY + 14, "Save Backups", Colors::Text, TextStyle::Heading);
            framebuffer.drawHDivider(layout.cardX + 18, layout.cardY + 50, layout.cardW - 36);
            drawBackupList(framebuffer);
            drawNavBar(framebuffer, hint);
        }

        // Transient failure notice, centred just above the nav bar (same treatment as the editor's).
        if (statusFrames > 0 && !statusMessage.empty())
        {
            int textWidth, th;
            framebuffer.measureText(statusMessage, textWidth, th);
            const int padX = 18, bw = textWidth + padX * 2, bh = th + 14;
            const int boxX = (framebuffer.getWidth() - bw) / 2, by = framebuffer.getHeight() - NAV_BAR_HEIGHT - bh - 12;
            framebuffer.drawSoftShadow(boxX, by, bw, bh, 8);
            framebuffer.drawFilledRoundedRect(boxX, by, bw, bh, 8, Colors::Panel);
            framebuffer.drawRoundedRect(boxX, by, bw, bh, 8, Colors::Orange, 2);
            framebuffer.drawText(boxX + padX, by + 7, statusMessage, Colors::Text);
        }

        if (showDeleteConfirmation)
        {
            PopIn pop(framebuffer, "backup.deleteConfirm");
            drawDeleteConfirmation(framebuffer);
        }
    }

    void BackupSelectionScreen::drawBackupList(PKSEFramebuffer &framebuffer)
    {
        const BackupLayout layout = backupLayout();
        const int startY = layout.listTop;
        const int total = (int)backups.size();
        const int first = firstVisibleRow(selectedIndex, total);
        // The list eases to `first` instead of jumping a row at a time; rows are drawn at a pixel
        // offset, one extra, clipped to the window.
        const float scrollPx = framebuffer.spring("backup.list", static_cast<float>(first * layout.rowH), 600.0f, 49.0f,
                                                  static_cast<float>(first * layout.rowH));
        const int drawFirst = std::max(0, static_cast<int>(scrollPx) / layout.rowH);
        const int last = std::min(total, drawFirst + layout.visible + 1);
        framebuffer.setClipRect(layout.listX - 6, startY, layout.listW + 12, layout.visible * layout.rowH);
        for (int row = drawFirst; row < last; row++)
        {
            int itemY = startY + row * layout.rowH - static_cast<int>(std::lround(scrollPx));
            bool selected = (row == selectedIndex);
            // Row 0 is the "load the live save" action -- accent it to stand out.
            drawHomeTile(framebuffer, layout.listX, itemY, layout.listW, layout.rowH - 10, backups[row].displayName,
                         selected, (row == 0));
        }
        framebuffer.clearClip();
        drawScrollbar(framebuffer, layout.listX + layout.listW + 4, startY, layout.visible * layout.rowH,
                      total * layout.rowH, static_cast<int>(scrollPx));
    }

    void BackupSelectionScreen::drawDeleteConfirmation(PKSEFramebuffer &framebuffer)
    {
        constexpr int dialogWidth = 560, h = 248;
        const int dialogX = (framebuffer.getWidth() - dialogWidth) / 2;
        const int dialogY = (framebuffer.getHeight() - h) / 2;

        int centerY =
            Dialogs::drawDialogFrame(framebuffer, dialogX, dialogY, dialogWidth, h, "Delete Backup?", Colors::Warning);
            framebuffer.beginTyping(); // the message types out; the buttons follow it
        framebuffer.drawText(dialogX + 24, centerY, "This action cannot be undone.", Colors::TextDim);
        if (deleteConfirmationIndex >= 0 && deleteConfirmationIndex < (int)backups.size())
        {
            framebuffer.drawText(dialogX + 24, centerY + 28, backups[deleteConfirmationIndex].displayName,
                                 Colors::Text);
        }

        // Buttons carry their glyph (B: Cancel, A: Delete); Delete stays red. Rects are stashed so
        // update() can hit-test taps next frame.
        const int buttonHeight = TouchTargetMin, by = dialogY + h - buttonHeight - 16, bw = (dialogWidth - 48 - 16) / 2;
        const int cancelX = dialogX + 24, delX = dialogX + dialogWidth - 24 - bw;
        drawGlyphButton(framebuffer, cancelX, by, bw, buttonHeight, "B", "Cancel", Colors::PanelAlt,
                        Colors::Text, pendingTapButtonId == 0);
        drawGlyphButton(framebuffer, delX, by, bw, buttonHeight, "A", "Delete", Colors::Red, Colors::White,
                        pendingTapButtonId == 1);
        deleteCancelBtn = {cancelX, by, bw, buttonHeight};
        deleteDeleteBtn = {delX, by, bw, buttonHeight};
    }

    bool BackupSelectionScreen::armTap(int buttonId, u64 button)
    {
        pendingTapButtonId = buttonId;
        pendingTapButton = button;
        pendingTapFrames = TAP_PRESS_FRAMES;
        return true;
    }

    void BackupSelectionScreen::deleteBackup(int index)
    {
        if (index <= 0 || index >= (int)backups.size())
        {
            return; // Can't delete the "Load from Title" option or invalid index
        }

        char backupPath[512];
        snprintf(backupPath, sizeof(backupPath), "%s/%s",
                 gameDirectory.c_str(), backups[index].timestamp.c_str());

        if (deleteDirectoryRecursive(backupPath))
        {
            backups.erase(backups.begin() + index);

            if (selectedIndex >= (int)backups.size())
            {
                selectedIndex = (int)backups.size() - 1;
            }
        }
    }
}
