#ifndef UI_PKSE_FRAMEBUFFER_H
#define UI_PKSE_FRAMEBUFFER_H

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "UI/Common.h"

// Forward-declare types so we don't pull heavy SDL/GL/NanoVG headers into the 15+ UI files that
// include this header. The concrete includes live only in PKSEFramebuffer.cpp.
struct SDL_Window;
typedef void *SDL_GLContext;
struct NVGcontext;

namespace UI
{
    // Typographic scale (Nunito). Body is the default; Heading and Title are faux-bold.
    enum class TextStyle
    {
        Caption,
        Body,
        Heading,
        Title,
        Count
    };

    // NanoVG (OpenGL) 2D surface — GPU anti-aliased vector graphics + text. The public API is
    // deliberately identical to the earlier SDL_Renderer version so every screen/panel/dialog
    // keeps working unchanged; SDL provides only the window, the GL context, and input.
    class PKSEFramebuffer
    {
    public:
        PKSEFramebuffer();
        ~PKSEFramebuffer();

        void clear(Color color);
        void drawPixel(int pixelX, int pixelY, Color color);
        void drawRect(int rectX, int rectY, int rectWidth, int rectHeight, Color color);
        void drawFilledRect(int rectX, int rectY, int rectWidth, int rectHeight, Color color);
        void drawText(int textX, int textY, const char *text, Color color, TextStyle style = TextStyle::Body);
        void drawText(int textX, int textY, const std::string &text, Color color, TextStyle style = TextStyle::Body);

        // Draw a symbol (gender ♂/♀, star ★, heart ♥, ◀ ▶, …) via the fallback symbol font,
        // since the primary UI font (Nunito) doesn't include these glyphs. `style` picks the size,
        // so a symbol placed inside a small badge can be measured and drawn at the same (Caption) size.
        void drawSymbol(int symbolX, int symbolY, const std::string &symbol, Color color,
                        TextStyle style = TextStyle::Body);
        void drawImage(int imageX, int imageY, int sourceWidth, int sourceHeight, const unsigned char *imageData,
                       int channels);
        void drawImageScaled(int imageX, int imageY, int sourceWidth, int sourceHeight, int destWidth, int destHeight,
                             const unsigned char *imageData, int channels, uint8_t alpha = 255);
        void flush();

        // Textures are cached under the ADDRESS of the pixel buffer they were built from, so when
        // the sprite cache frees a buffer its texture has to go too -- otherwise a later sprite
        // allocated at that same address would be drawn with the freed one's image. Hooked up to
        // SpriteManager's eviction callback; onSpriteEvicted forwards to the live framebuffer.
        void invalidateImage(const unsigned char *data);
        static void onSpriteEvicted(const unsigned char *data);

        // Clip subsequent drawing to a rectangle (for a scrollable region); clearClip() removes it.
        // Both need an active frame; primitives outside the clip rect are not rendered.
        void setClipRect(int clipX, int clipY, int clipWidth, int clipHeight);
        void clearClip();

        // Encapsulate the current look so panels stay simple and stay consistent when the
        // theme changes. All pull from the active Colors palette.
        void drawCard(int cardX, int cardY, int cardWidth, int cardHeight); // surface fill + border outline
        // selection fill + accent left edge
        void drawSelectionHighlight(int highlightX, int highlightY, int highlightWidth, int highlightHeight);
        void drawHDivider(int dividerX, int dividerY, int dividerWidth); // subtle horizontal rule
        // The header band across the top of a card (PKForge: a cobalt header strip inset in the panel).
        void drawCardBand(int bandX, int bandY, int bandWidth, int bandHeight);
        void drawFilledEllipse(int centerX, int centerY, int radiusX, int rectY, Color color); // e.g. soft shadows

        // Anti-aliased on the horizontal edges of rounded corners (fractional coverage);
        // straight edges are exact. `r` is the corner radius (clamped to min(w,h)/2).
        void drawFilledRoundedRect(int rectX, int rectY, int rectWidth, int rectHeight, int cornerRadius, Color color);
        void drawRoundedRect(int rectX, int rectY, int rectWidth, int rectHeight, int cornerRadius, Color color,
                             int thickness = 1);
        void drawFilledCircle(int centerX, int centerY, int radius, Color color);
        void drawCircle(int centerX, int centerY, int radius, Color color, int thickness = 1);
        // Draws a Pokemon egg (cream oval + teal spots) centered at (cx,cy), ~size px tall.
        // Used in the box/storage grids in place of the species sprite when a pokemon is an egg.
        void drawEgg(int centerX, int centerY, int size);
        // Draws HOME's shiny mark -- a four-pointed sparkle (concave-sided star) -- filling a size×size box
        // whose top-left is (x,y), so it drops into a marker row like any other icon.
        void drawShinyMark(int markX, int markY, int size, Color color);

        /**
         * File-browser row icons: a manila folder and a sheet of paper with a turned corner.
         *
         * Drawn rather than set in text. The browser used font glyphs -- a solid triangle for a
         * folder and a bullet for a file -- and a triangle in a list reads as "expand this row",
         * which is not what pressing A does. These are shapes with no other meaning.
         *
         * Vector rather than sprites for the same reason every other icon here is: they stay crisp
         * at any row height, they take the theme's colours, and they add nothing to romfs. Both are
         * anchored top-left in a `size` x `size` box, like drawShinyMark, so a caller can swap one
         * for the other without moving anything.
         *
         * `open` tilts the folder's front panel forward, which the browser uses to mark the ".."
         * row -- the one folder you are leaving rather than entering.
         */
        void drawFolderIcon(int iconX, int iconY, int size, Color color, bool open = false);
        void drawFileIcon(int iconX, int iconY, int size, Color color);
        /**
         * The search box's magnifier: a lens with a handle running down-right at 45 degrees.
         *
         * Drawn rather than set in text for the same reason as the two above -- the UI font has no
         * magnifier. THE HANDLE IS THE WHOLE ICON. This was a stroked circle with a short
         * horizontal stub beside it, which is not a magnifying glass at any size: it reads as a
         * lower-case "o" followed by an underscore. What makes the shape legible is the handle
         * being diagonal, round-capped and THICKER than the rim, so it reads as something held
         * rather than as two primitives that happen to touch.
         *
         * Anchored top-left in a `size` x `size` box, like drawShinyMark and the browser icons.
         */
        void drawSearchIcon(int iconX, int iconY, int size, Color color);
        /**
         * The Trade Evolve button's mark: a solid arrow pointing UP -- a triangular head over a
         * short shaft, the shape the games use for a Pokemon moving up a stage.
         *
         * Drawn rather than set in text for the same reason as the icons above: the UI font has no
         * such glyph, and a text caret reads as "expand this row", which is not what the button
         * does. Anchored top-left in a `size` x `size` box with its extents SYMMETRIC in it, so a
         * caller centres the icon by centring its box -- the contract every icon here keeps.
         */
        void drawEvolveArrow(int iconX, int iconY, int size, Color color);
        // Storage-grid cursor: a wide arrowhead pointing straight down, no shaft. Its POINT lands on
        // (tipX, tipY) with the head above, symmetric about that x. `headHeight` sizes the head --
        // the visible arrow; the mitred outline runs on below it to the point at tipY, adding ~27%
        // more. Drawn in the active cursor mode's colour.
        void drawPointerCursor(int tipX, int tipY, int headHeight, Color color);
        // Stadium (fully-rounded) pill = rounded rect with r = h/2. HOME's headers/badges/buttons.
        void drawPill(int pillX, int pillY, int pillWidth, int pillHeight, Color color);
        void drawPillBorder(int pillX, int pillY, int pillWidth, int pillHeight, Color color, int thickness = 1);
        // Soft drop shadow / elevation under a card, pill, or bar (layered translucent rounded rects).
        void drawSoftShadow(int shadowX, int shadowY, int shadowWidth, int shadowHeight, int cornerRadius);
        // Vertical two-stop gradient (screen backdrops, header/pill fills).
        void drawVerticalGradient(int gradientX, int gradientY, int gradientWidth, int gradientHeight, Color top,
                                  Color bottom);
        // The signature stat radar. `values` must be in HOME vertex order
        // [HP, Attack, Defense, Speed, Sp.Def, Sp.Atk] (HP at top, clockwise). Draws the
        // web + spokes, a translucent filled polygon, and the outline. Labels are the caller's job.
        void drawStatHexagon(int centerX, int centerY, int hexRadius, const float *values, int count,
                             float maxValue, Color fill, Color webColor, Color outline);
        // Two-tone rounded type badge (HOME style): colored icon chip + name. Returns total width drawn.
        int drawTypeBadge(int badgeX, int badgeY, const std::string &typeName, Color typeColor);
        // Move summary plate components (PKForge design)
        int drawTypePill(int x, int y, int width, int height, uint8_t typeId);
        void drawMoveCategoryIcon(int iconX, int iconY, int size, uint8_t category);
        void drawGaugeBar(int x, int y, int width, int height, float fraction, Color fillColor, Color trackColor);
        // Corner brackets / targeting reticle (DS box crosshair / PKForge style).
        void drawCornerBrackets(int boxX, int boxY, int boxWidth, int boxHeight, int bracketLen, int thickness,
                                Color color, float pulseOffset = 0.0f);
        // Retro DS hardware chrome (PksmPaint / PKForge design tokens)
        void drawRetroDSPanel(int x, int y, int width, int height, int radius = 6, Color body = Colors::Panel);
        void drawRetroDSHeaderStrip(int x, int y, int width, int height, const char *label = nullptr,
                                    Color accent = Color(0x2B, 0x4E, 0x95), TextStyle style = TextStyle::Heading);
        void drawRetroDSButton(int x, int y, int width, int height, const char *label = nullptr, bool selected = false,
                               Color accent = Colors::Accent, int radius = 6);
        // PKForge chrome, ported 1:1 from PKForge.Chrome/PksmPaint.cs. Drawn from the Pksm tokens, so
        // they look the same whatever theme is active; the theme-aware primitives above route to
        // them when the PKForge theme is on.
        void pkLogoGrid(int x, int y, int width, int height, int cell = 32, int lineWidth = 2);
        void pkPanel(int x, int y, int width, int height, Color fill = Pksm::Paper, int radius = 6);
        void pkBlackButton(int x, int y, int width, int height, int radius = 4);
        void pkSelectedButton(int x, int y, int width, int height, int radius = 4, Color accent = Pksm::HeaderBlue);
        void pkHeaderStrip(int x, int y, int width, int height, const std::string &label = "",
                           Color accent = Pksm::HeaderBlue, bool centred = false, TextStyle style = TextStyle::Body);
        void pkStripeRow(int x, int y, int width, int height, bool selected);
        // Box wallpaper: dark world tint + a quieter echo of the logo grid.
        void pkWallpaper(int x, int y, int width, int height, Color base = Pksm::BoxWallpapers[0]);
        // An empty box slot: only a faint waiting ball (the games draw no cell frames).
        void pkEmptySlot(int x, int y, int width, int height, Color wallpaper = Pksm::BoxWallpapers[0]);
        // Status-strip battery (PKForge DsChrome.StatusStrip), `percent` < 0 draws it full.
        void pkBattery(int x, int y, int percent);
        void pkBoxNameBar(int x, int y, int width, int height, const std::string &label, bool canPrev, bool canNext);
        void pkSelection(int x, int y, int width, int height, Color color = Pksm::CursorRed);
        void pkCarryGhost(int x, int y, int width, int height);

        // Draw a sprite with a gentle idle animation (bob + breathe) and a soft ground
        // shadow, driven by the frame tick. (x,y,boxW,boxH) is the nominal placement box;
        // (srcW,srcH) the sprite's native size; `phase` offsets the timing per sprite.
        void drawSpriteIdle(int spriteX, int spriteY, int boxWidth, int boxHeight, int sourceWidth, int sourceHeight,
                            const unsigned char *data, int channels, float phase);

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        // Motion, ported from PKForge.App/Services/Motion.cs and Kit.AnimateIn.
        //
        // IMMEDIATE-MODE, LIKE EVERYTHING ELSE HERE. Each piece of motion is keyed by an id and lives
        // as long as that id is drawn every frame. An id that skipped a frame starts over, which is
        // exactly the rule a screen needs: a popup that was not on screen last frame has just opened
        // and pops in, and a cursor that was not drawn last frame has just appeared and snaps rather
        // than flying in from wherever it was the last time that panel was open. So no caller has to
        // tell the motion layer when something opened -- and none of the ~30 dialog flags had to change.
        //
        // Input never waits on any of it: state changes the frame the button is pressed, and the
        // motion only decides where that state is drawn while it catches up.

        // Everything drawn between these animates in when `id` first appears: the window grows from
        // 92% with PKForge's SpringOut over 160ms while fading in over 130ms. The scrim does NOT
        // move -- the transform is applied at the first fill that is not full-screen, so the veil
        // drawn before the window stays put and the window scales about its own centre.
        void beginPopIn(const char *id);
        void endPopIn();

        // A cursor riding a critically damped spring toward (targetX, targetY): fast, eased, no
        // overshoot. PKForge's box cursor uses 700 / 52.9. Snaps on the frame the id first appears.
        void glide(const char *id, float targetX, float targetY, float &outX, float &outY, float stiffness = 700.0f,
                   float damping = 52.9f);
        // A single value on a spring. `initial` is where it starts when the id first appears; damping
        // below 2*sqrt(stiffness) bounces (the pick-up hop, the focus pop).
        float spring(const char *id, float target, float stiffness = 420.0f, float damping = 41.0f,
                     float initial = 0.0f);

        // DialogueChoiceBox's typewriter. Inside a pop-in, text drawn after beginTyping() is revealed
        // a character at a time (24ms each, PKForge's pace, but never longer than a second for the
        // whole message); everything drawn after endTyping() -- the buttons -- stays hidden until the
        // message is out, then fades in over 100ms. drawGlyphButton calls endTyping(), so a dialog
        // only has to say where its message starts. A no-op outside a pop-in.
        void beginTyping();
        void endTyping();
        // True while some dialog is still typing (as of the last frame drawn). The screens check it
        // first: A, or a tap, then finishes the message instead of answering the dialog unseen.
        static bool typingInProgress();
        static void skipTyping();

        // Scale everything drawn until popTransform() about (centerX, centerY). Pairs must nest.
        void pushScale(float centerX, float centerY, float scale);
        void popTransform();

        // Frame timing (updated each flush()). Use these to drive animation without
        // every screen having to read a clock. deltaSeconds = time since last frame.
        float getDeltaSeconds() const { return deltaSeconds; }
        double getTimeSeconds() const { return totalSeconds; }

        // Exposed so callers can do their own layout math (centering, right-align, wrapping).
        // Returns the pixel size of `text` in the given style's font.
        void measureText(const std::string &text, int &outWidth, int &outHeight, TextStyle style = TextStyle::Body);
        // Line height (font ascent+descent) for a style — for consistent vertical spacing.
        int lineHeight(TextStyle style = TextStyle::Body) const;
        // The y to hand drawText so the text's CAPITAL LETTERS are centred on `centerY` -- which is what
        // lines a label up with a badge, a dot or the middle of a row. Centring the LINE BOX instead
        // (centerY - lineHeight / 2) leaves the capitals a few pixels high, because the box keeps room
        // below the baseline for descenders that most labels never use.
        int textYCenteredOn(int centerY, TextStyle style = TextStyle::Body) const;

    private:
        // Lazily build + cache a NanoVG image from a raw sprite pixel buffer, keyed by the buffer
        // pointer (SpriteManager caches sprites for the session, so pointers are stable). Converts
        // RGB→RGBA if needed (NanoVG images are RGBA). Returns the image handle (>=0), or -1.
        // Raw fills that skip the PKForge focus routing (the pk* chrome itself uses these).
        void fillRoundedRaw(float x, float y, float width, float height, float radius, Color color);
        void fillGradientRaw(float x, float y, float width, float height, float radius, Color top, Color bottom);
        void strokeRoundedRaw(float x, float y, float width, float height, float radius, Color color, float thickness);

        int nvgImageFor(const unsigned char *data, int dialogWidth, int dialogHeight, int channels);
        // Set NanoVG's font face + size for a text style (Heading/Title get a heavier faux-bold).
        void applyTextStyle(TextStyle style) const;
        // Begin the NanoVG frame lazily if one isn't already active. Returns true if vg is valid
        // and a frame is ready to draw into (so callers do `if (!ensureFrame()) return;`).
        bool ensureFrame();

        SDL_Window *window;
        SDL_GLContext glContext;
        NVGcontext *vg;
        int fontSans;             // Nunito (primary); the symbol fonts are added as fallbacks to it
        int fontPixel;            // NDS12 (Nintendo DS pixel font); symbols and CJK are fallbacks to it
        int fontSym;              // Noto Sans Symbols (♂ ♀ ★ …)
        int fontSym2;             // Noto Sans Symbols 2 (card suits ♥ …)
        bool frameActive = false; // true between the frame's first draw/clear and flush()
        int width;
        int height;

        // Frame timing (see getDeltaSeconds/getTimeSeconds).
        uint64_t lastCounter = 0;
        float deltaSeconds = 0.0f;
        double totalSeconds = 0.0;

        // Motion state (see beginPopIn / glide / spring), keyed by id. `lastFrame` is the frame the
        // id was last drawn on; anything that missed one starts fresh. Pruned in flush().
        struct MotionEntry
        {
            uint64_t lastFrame = 0;
            uint64_t steppedFrame = 0; // a spring advances once per frame however often it is asked
            double start = 0.0;        // pop-in start time
            float value[2] = {0, 0};
            float velocity[2] = {0, 0};
            int typedTotal = 0;        // characters the typed message had last frame
            double typedDoneAt = -1.0; // when the message finished typing (-1 = not yet)
            bool typingSkipped = false;
        };
        std::unordered_map<std::string, MotionEntry> motion;
        uint64_t frameIndex = 1;
        MotionEntry &motionFor(const char *id, bool &fresh);
        void stepSpring(MotionEntry &entry, int axis, float target, float stiffness, float damping);

        struct PopScope
        {
            double start;
            bool applied; // the transform has been applied (at the window's first fill)
            MotionEntry *entry;
            float fade = 1.0f;     // the window's current opacity
            bool typing = false;   // between beginTyping() and endTyping()
            int typed = 0;         // characters drawn (or due) so far this frame
            int budget = 0;        // characters revealed by now
        };
        bool typingBusyThisFrame = false, typingBusyLastFrame = false, skipTypingRequested = false;
        // How many of `text`'s bytes the typewriter lets through; advances the scope's count.
        size_t typedBytes(const std::string &text);
        std::vector<PopScope> popStack;
        // Called by the fills: inside a pop-in, the first fill smaller than the screen is the window,
        // so that is where the scale goes on.
        void notePopupRect(float x, float y, float rectWidth, float rectHeight);

        // NanoVG image handles keyed by the source pixel-buffer pointer.
        std::map<const unsigned char *, int> imageCache;
    };

    /// Scoped beginPopIn/endPopIn: `{ PopIn pop(framebuffer, "id"); drawTheDialog(...); }`.
    struct PopIn
    {
        PKSEFramebuffer &framebuffer;
        PopIn(PKSEFramebuffer &target, const char *id) : framebuffer(target) { framebuffer.beginPopIn(id); }
        ~PopIn() { framebuffer.endPopIn(); }
        PopIn(const PopIn &) = delete;
        PopIn &operator=(const PopIn &) = delete;
    };
}

#endif
