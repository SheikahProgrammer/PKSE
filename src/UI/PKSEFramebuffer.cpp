#include "UI/PKSEFramebuffer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

#include <switch.h> // pl: the console's shared CJK fonts, loaded as NanoVG fallbacks below
#include <glad/glad.h>
#include <SDL2/SDL.h>

#include "nanovg.h"
#define NANOVG_GL3
#include "nanovg_gl.h"

#include "Names/TypeNames.h"
#include "UI/SpriteManager.h" // eviction hook: textures are keyed on sprite buffer addresses
#include "Utils/Logger.h"

using namespace Utils;

namespace UI
{

    static constexpr const char *UI_FONT_PATH = "romfs:/fonts/Nunito.ttf";
    static constexpr const char *PIXEL_FONT_PATH = "romfs:/fonts/NDS12.ttf";
    static constexpr const char *SYMBOLS_FONT_PATH = "romfs:/fonts/NotoSansSymbols.ttf";
    static constexpr const char *SYMBOLS2_FONT_PATH = "romfs:/fonts/NotoSansSymbols2.ttf";

    // Nunito point size per TextStyle.
    static constexpr float FONT_SIZES[static_cast<int>(TextStyle::Count)] = {
        15.0f, // Caption
        19.0f, // Body
        26.0f, // Heading
        34.0f, // Title
    };

    // Pixel point size per TextStyle (NDS12 integer pixel rendering).
    static constexpr float PIXEL_FONT_SIZES[static_cast<int>(TextStyle::Count)] = {
        13.0f, // Caption
        16.0f, // Body
        22.0f, // Heading
        28.0f, // Title
    };

    /// Nunito's capital height as a fraction of its em -- OS/2 sCapHeight 705 over unitsPerEm 1000,
    /// read from romfs/fonts/Nunito.ttf. A property of the bundled font file, not a tuning value.
    static constexpr float NUNITO_CAP_HEIGHT_PER_EM = 0.705f;

    static inline NVGcolor toNVG(Color color) { return nvgRGBA(color.red, color.green, color.blue, color.alpha); }

    // The framebuffer SpriteManager's eviction callback should reach. Only one exists at a time
    // (UIManager owns it), and it is cleared on destruction so a late eviction is a no-op.
    static PKSEFramebuffer *s_activeFramebuffer = nullptr;

    PKSEFramebuffer::PKSEFramebuffer()
        : window(nullptr), glContext(nullptr), vg(nullptr),
          fontSans(-1), fontPixel(-1), fontSym(-1), fontSym2(-1), width(1280), height(720)
    {

        // Request an OpenGL 4.3 core context with a stencil buffer — NanoVG needs stencil for its
        // fills / stencil strokes. This is the proven Switch (mesa/nouveau) config.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        window = SDL_CreateWindow("PKSE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, SDL_WINDOW_OPENGL);
        if (!window)
        {
            logErrorToFile("SDL_CreateWindow(OPENGL) failed");
            logErrorToFile(SDL_GetError());
            return;
        }

        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
        {
            logErrorToFile("SDL_GL_CreateContext failed");
            logErrorToFile(SDL_GetError());
            return;
        }
        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1); // vsync

        if (!gladLoadGL())
        {
            logErrorToFile("gladLoadGL failed");
            return;
        }
        glGetError(); // clear any benign startup GL error

        const char *glver = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        // should be "OpenGL 4.x", not "OpenGL ES"
        if (glver) logInfoToFile("GL_VERSION:", glver);

        vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!vg)
        {
            logErrorToFile("nvgCreateGL3 failed");
            return;
        }

        // Fonts from romfs (fontstash reads via fopen; romfs is mounted). Add the two symbol fonts
        // as fallbacks to Nunito so nvgText() automatically covers glyphs Nunito lacks (♂ ♀ ★ ♥ ◀ ▶).
        fontSans = nvgCreateFont(vg, "sans", UI_FONT_PATH);
        if (fontSans < 0)
            logErrorToFile("nvgCreateFont Nunito failed");
        fontPixel = nvgCreateFont(vg, "pixel", PIXEL_FONT_PATH);
        if (fontPixel < 0)
            logErrorToFile("nvgCreateFont NDS12 failed");
        fontSym = nvgCreateFont(vg, "sym", SYMBOLS_FONT_PATH);
        fontSym2 = nvgCreateFont(vg, "sym2", SYMBOLS2_FONT_PATH);
        if (fontSans >= 0 && fontSym >= 0)
            nvgAddFallbackFontId(vg, fontSans, fontSym);
        if (fontSans >= 0 && fontSym2 >= 0)
            nvgAddFallbackFontId(vg, fontSans, fontSym2);
        if (fontPixel >= 0 && fontSym >= 0)
            nvgAddFallbackFontId(vg, fontPixel, fontSym);
        if (fontPixel >= 0 && fontSym2 >= 0)
            nvgAddFallbackFontId(vg, fontPixel, fontSym2);

        /**
         * Then the console's own fonts, so Japanese, Korean and Chinese render at all.
         *
         * Nunito and the two Noto Symbols faces carry Latin-1 completely -- accented French,
         * German, Italian and Spanish names have always drawn correctly -- and between them not one
         * CJK glyph. A Japanese Pokemon's nickname was decoded perfectly by the save layer and then
         * drew as nothing, which is what "non-English names do not render" actually meant.
         *
         * These come from `pl` shared memory rather than romfs because they are THE SAME FACES the
         * console and the games render Pokemon names with -- a Japanese nickname then looks in
         * PKSE exactly as it does in the game it came from, which a separately-sourced font cannot
         * promise -- and because there is no fetch step to go stale. `freeData = 0` because the
         * buffers belong to the service, not to fontstash; main.cpp holds `pl` open for the life
         * of the process for exactly that reason. Size is not the argument: PKSE runs under title
         * override, so the .nro's size is not a constraint either way.
         *
         * ORDER IS THE PRIORITY ORDER. fontstash walks the fallback chain until a face has the
         * glyph, so Nunito keeps every Latin character it already drew and these only answer for
         * what it lacks. Standard comes first because it covers kana and the common kanji; the
         * Chinese faces follow for Han the Japanese font does not have, then Hangul, then
         * Nintendo's private-use glyphs.
         */
        static constexpr struct { PlSharedFontType type; const char *name; } SHARED_FONTS[] = {
            {PlSharedFontType_Standard, "nxstd"},
            {PlSharedFontType_ChineseSimplified, "nxscn"},
            {PlSharedFontType_ExtChineseSimplified, "nxscnext"},
            {PlSharedFontType_ChineseTraditional, "nxtcn"},
            {PlSharedFontType_KO, "nxkor"},
            {PlSharedFontType_NintendoExt, "nxext"},
        };
        for (const auto &sharedFont : SHARED_FONTS)
        {
            PlFontData fontData;
            if (R_FAILED(plGetSharedFontByType(&fontData, sharedFont.type)))
                continue;
            const int fontId = nvgCreateFontMem(vg, sharedFont.name,
                                                static_cast<unsigned char *>(fontData.address),
                                                static_cast<int>(fontData.size), /*freeData=*/0);
            if (fontId >= 0 && fontSans >= 0)
                nvgAddFallbackFontId(vg, fontSans, fontId);
            if (fontId >= 0 && fontPixel >= 0)
                nvgAddFallbackFontId(vg, fontPixel, fontId);
        }

        s_activeFramebuffer = this;
        SpriteManager::setEvictCallback(&PKSEFramebuffer::onSpriteEvicted);
    }

    void PKSEFramebuffer::onSpriteEvicted(const unsigned char *data)
    {
        if (s_activeFramebuffer)
            s_activeFramebuffer->invalidateImage(data);
    }

    void PKSEFramebuffer::invalidateImage(const unsigned char *data)
    {
        if (!vg || !data)
            return;
        for (auto *cache : {&imageCache, &smoothImageCache})
        {
            auto iterator = cache->find(data);
            if (iterator == cache->end())
                continue;
            if (iterator->second >= 0)
                nvgDeleteImage(vg, iterator->second);
            cache->erase(iterator);
        }
    }

    PKSEFramebuffer::~PKSEFramebuffer()
    {
        // Stop eviction reaching a half-destroyed object; the loop below frees every texture anyway.
        SpriteManager::setEvictCallback(nullptr);
        if (s_activeFramebuffer == this)
            s_activeFramebuffer = nullptr;
        if (vg)
        {
            for (auto *cache : {&imageCache, &smoothImageCache})
            {
                for (auto &kv : *cache)
                    if (kv.second >= 0)
                        nvgDeleteImage(vg, kv.second);
                cache->clear();
            }
            nvgDeleteGL3(vg);
        }
        if (glContext)
            SDL_GL_DeleteContext(glContext);
        if (window)
            SDL_DestroyWindow(window);
    }

    // Begin a NanoVG frame if one isn't active yet (safety for draws before clear()).
    // Returns true if vg is valid and a frame is ready.
    bool PKSEFramebuffer::ensureFrame()
    {
        if (!vg)
            return false;
        if (!frameActive)
        {
            nvgBeginFrame(vg, static_cast<float>(width), static_cast<float>(height), 1.0f);
            frameActive = true;
        }
        return true;
    }

    void PKSEFramebuffer::clear(Color color)
    {
        if (!vg)
            return;
        if (frameActive)
        {
            nvgEndFrame(vg);
            frameActive = false;
        } // close a stray previous frame
        glViewport(0, 0, width, height);
        glClearColor(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f, color.alpha / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg, static_cast<float>(width), static_cast<float>(height), 1.0f);
        frameActive = true;
        // PKForge draws its logo grid behind every global menu body (DsChrome.GridBackground).
        if (isPkForgeTheme() && color == Colors::Background)
            pkLogoGrid(0, 0, width, height);
    }

    void PKSEFramebuffer::flush()
    {
        if (vg && frameActive)
        {
            nvgEndFrame(vg);
            frameActive = false;
        }
        if (window)
            SDL_GL_SwapWindow(window);

        uint64_t nowSeconds = SDL_GetPerformanceCounter();
        if (lastCounter != 0)
        {
            double freq = static_cast<double>(SDL_GetPerformanceFrequency());
            deltaSeconds = static_cast<float>(static_cast<double>(nowSeconds - lastCounter) / freq);
            // guard against huge dt after a pause
            if (deltaSeconds > 0.1f) deltaSeconds = 0.1f;
        }
        lastCounter = nowSeconds;
        totalSeconds += deltaSeconds;

        // A pop-in left open by an early return must not leak its transform into the next frame.
        popStack.clear();
        typingBusyLastFrame = typingBusyThisFrame;
        typingBusyThisFrame = false;
        skipTypingRequested = false;
        ++frameIndex;
        // Drop motion for ids gone a while; only every couple of seconds, since it walks the map.
        if (frameIndex % 128 == 0)
        {
            for (auto it = motion.begin(); it != motion.end();)
                it = (it->second.lastFrame + 2 < frameIndex) ? motion.erase(it) : std::next(it);
        }
    }

    void PKSEFramebuffer::drawPixel(int pixelX, int pixelY, Color color)
    {
        if (!ensureFrame())
            return;
        nvgBeginPath(vg);
        nvgRect(vg, (float)pixelX, (float)pixelY, 1.0f, 1.0f);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawFilledRect(int rectX, int rectY, int rectWidth, int rectHeight, Color color)
    {
        if (!ensureFrame())
            return;
        notePopupRect((float)rectX, (float)rectY, (float)rectWidth, (float)rectHeight);
        nvgBeginPath(vg);
        nvgRect(vg, (float)rectX, (float)rectY, (float)rectWidth, (float)rectHeight);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    // Clip subsequent drawing to a rect (nvgScissor) so a scrolled region's rows don't spill over
    // neighbouring chrome. clearClip() lifts the restriction; both are no-ops without an active frame.
    void PKSEFramebuffer::setClipRect(int clipX, int clipY, int clipWidth, int clipHeight)
    {
        if (!ensureFrame())
            return;
        nvgScissor(vg, (float)clipX, (float)clipY, (float)clipWidth, (float)clipHeight);
    }
    void PKSEFramebuffer::clearClip()
    {
        if (!ensureFrame())
            return;
        nvgResetScissor(vg);
    }

    void PKSEFramebuffer::drawRect(int rectX, int rectY, int rectWidth, int rectHeight, Color color)
    {
        if (!ensureFrame())
            return;
        nvgBeginPath(vg);
        nvgRect(vg, rectX + 0.5f, rectY + 0.5f, rectWidth - 1.0f, rectHeight - 1.0f);
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }

    // PKForge shapes: panels are UiTokens.PanelRadius (6), never the HOME 14-16 px curves. Discs and
    // pills (radius at half the short side) are left alone -- drawPill handles the latter.
    static int pkRadius(int radius, int w, int h)
    {
        if (!isPkForgeTheme())
            return radius;
        const int half = std::min(w, h) / 2;
        return (radius > 6 && radius < half) ? 6 : radius;
    }

    void PKSEFramebuffer::drawFilledRoundedRect(int rectX, int rectY, int rectWidth, int rectHeight, int cornerRadius,
                                                Color color)
    {
        if (rectWidth <= 0 || rectHeight <= 0 || !ensureFrame())
            return;
        if (isPkForgeFocusFill(color))
        {
            pkSelectedButton(rectX, rectY, rectWidth, rectHeight, std::min(cornerRadius, 6));
            return;
        }
        // A large Panel-coloured card is PKForge's device panel (top light, bezel, hard drop shadow).
        if (isPkForgeTheme() && color == Colors::Panel && rectWidth >= 180 && rectHeight >= 120)
        {
            pkPanel(rectX, rectY, rectWidth, rectHeight);
            return;
        }
        cornerRadius = pkRadius(cornerRadius, rectWidth, rectHeight);
        int rmax = std::min(rectWidth, rectHeight) / 2;
        if (cornerRadius > rmax)
            cornerRadius = rmax;
        if (cornerRadius < 0)
            cornerRadius = 0;
        fillRoundedRaw((float)rectX, (float)rectY, (float)rectWidth, (float)rectHeight, (float)cornerRadius, color);
    }

    void PKSEFramebuffer::drawRoundedRect(int rectX, int rectY, int rectWidth, int rectHeight, int cornerRadius,
                                          Color color, int thickness)
    {
        if (rectWidth <= 0 || rectHeight <= 0 || thickness <= 0 || !ensureFrame())
            return;
        cornerRadius = pkRadius(cornerRadius, rectWidth, rectHeight);
        // PKForge panels carry a 2 dp cobalt bezel, not a hairline.
        if (isPkForgeTheme() && thickness == 1 && color == Colors::Border)
            thickness = 2;
        int rmax = std::min(rectWidth, rectHeight) / 2;
        if (cornerRadius > rmax)
            cornerRadius = rmax;
        if (cornerRadius < 0)
            cornerRadius = 0;
        float strokeWidth = (float)thickness;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, rectX + strokeWidth * 0.5f, rectY + strokeWidth * 0.5f, rectWidth - strokeWidth,
                       rectHeight - strokeWidth, std::max(0.0f, cornerRadius - strokeWidth * 0.5f));
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, strokeWidth);
        nvgStroke(vg);
    }

    void PKSEFramebuffer::drawFilledCircle(int centerX, int centerY, int radius, Color color)
    {
        if (radius <= 0 || !ensureFrame())
            return;
        if (isPkForgeFocusFill(color))
        {
            // A focused round control: the selected-button body with its pale rim.
            NVGpaint body = nvgLinearGradient(vg, 0, (float)(centerY - radius), 0, (float)(centerY + radius),
                                              toNVG(Color(0x5D, 0x7B, 0xB2)), toNVG(Color(0x28, 0x48, 0x89)));
            nvgBeginPath(vg);
            nvgCircle(vg, (float)centerX, (float)centerY, (float)radius);
            nvgFillPaint(vg, body);
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgCircle(vg, (float)centerX, (float)centerY, radius - 1.5f);
            nvgStrokeColor(vg, toNVG(Color(0xF4, 0xF8, 0xFF, 0x70)));
            nvgStrokeWidth(vg, 1.2f);
            nvgStroke(vg);
            return;
        }
        nvgBeginPath(vg);
        nvgCircle(vg, (float)centerX, (float)centerY, (float)radius);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawCircle(int centerX, int centerY, int radius, Color color, int thickness)
    {
        if (radius <= 0 || !ensureFrame())
            return;
        float strokeWidth = (float)thickness;
        nvgBeginPath(vg);
        nvgCircle(vg, (float)centerX, (float)centerY, radius - strokeWidth * 0.5f);
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, strokeWidth);
        nvgStroke(vg);
    }

    void PKSEFramebuffer::drawEgg(int centerX, int centerY, int size)
    {
        // Classic Pokemon egg: a cream oval (taller than wide) with a few teal spots.
        const int shellRadiusX = std::max(4, static_cast<int>(size * 0.30f));
        const int rectY = std::max(5, static_cast<int>(size * 0.40f));
        drawFilledEllipse(centerX, centerY, shellRadiusX + 2, rectY + 2, Color(206, 196, 168, 255)); // subtle edge ring
        drawFilledEllipse(centerX, centerY, shellRadiusX, rectY, Color(238, 232, 210, 255));         // cream shell
        const Color spot(120, 198, 176, 255);                                 // teal spots
        drawFilledCircle(centerX - shellRadiusX / 3, centerY - rectY / 3, std::max(2, size / 13), spot);
        drawFilledCircle(centerX + shellRadiusX / 3, centerY, std::max(2, size / 16), spot);
        drawFilledCircle(centerX - shellRadiusX / 5, centerY + rectY / 3, std::max(2, size / 18), spot);
    }

    void PKSEFramebuffer::drawShinyMark(int markX, int markY, int size, Color color)
    {
        // HOME's shiny mark: a four-pointed sparkle. The four tips sit on the cardinal directions; the
        // edges between them curve INWARD (a quad curve pulled toward a control point near the center), so
        // it reads as a pinched "twinkle" rather than a plain star. Anchored top-left in a size×size box,
        // like every other drawn icon.
        if (size <= 1 || !ensureFrame())
            return;
        const float tipRadius = size * 0.5f;      // tip radius (half the box)
        const float controlRadius = tipRadius * 0.34f;        // control radius -> depth of the concave pinch
        const float diagonalControl = controlRadius * 0.70710678f; // c * cos45, the diagonal control offset
        const float centerX = markX + tipRadius, cy = markY + tipRadius;
        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, cy - tipRadius);                   // top tip
        nvgQuadTo(vg, centerX + diagonalControl, cy - diagonalControl, centerX + tipRadius, cy); // -> right tip
        nvgQuadTo(vg, centerX + diagonalControl, cy + diagonalControl, centerX, cy + tipRadius); // -> bottom tip
        nvgQuadTo(vg, centerX - diagonalControl, cy + diagonalControl, centerX - tipRadius, cy); // -> left tip
        nvgQuadTo(vg, centerX - diagonalControl, cy - diagonalControl, centerX, cy - tipRadius); // -> back to top tip
        nvgClosePath(vg);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawFolderIcon(int iconX, int iconY, int size, Color color, bool open)
    {
        // A manila folder: a back panel carrying the tab, with the front panel over it. Two shapes
        // rather than one outline, because the seam between them is what makes it read as a folder
        // rather than as a rounded rectangle with a bite taken out.
        //
        // Proportions are a 16x16 art box scaled to `size`. The folder is WIDER than it is tall
        // (real ones are), so it is centred vertically in the square box the caller reserved --
        // that is what keeps it on the same baseline as the file icon beside it.
        if (size <= 3 || !ensureFrame())
            return;
        const float unitScale = size / 16.0f;
        const float iconWidth = 16.0f * unitScale;
        const float iconHeight = 13.0f * unitScale;               // shorter than the box
        const float top = iconY + (size - iconHeight) * 0.5f; // centred, so both icons share a baseline
        const float cornerRadius = std::max(1.0f, 1.6f * unitScale);
        const float tabW = 6.5f * unitScale; // the tab covers a little under half the width
        const float tabH = 2.6f * unitScale;
        const float bodyTop = top + tabH;

        auto shade = [&](float floatValue) { // f < 1 darkens, f > 1 lightens toward white
            auto shadeChannel = [&](int value)
            {
                const float offset =
                    (floatValue <= 1.0f) ? value * floatValue : value + (255.0f - value) * (floatValue - 1.0f);
                return (unsigned char)std::min(255.0f, std::max(0.0f, offset));
            };
            return nvgRGBA(shadeChannel(color.red), shadeChannel(color.green), shadeChannel(color.blue), color.alpha);
        };

        // Back panel + tab, as one path: up the left edge, across the tab, down its right shoulder,
        // then along the back's top edge to the right corner.
        nvgBeginPath(vg);
        nvgMoveTo(vg, iconX, top + iconHeight);
        nvgLineTo(vg, iconX, top + cornerRadius);
        nvgQuadTo(vg, iconX, top, iconX + cornerRadius, top); // top-left corner
        nvgLineTo(vg, iconX + tabW - cornerRadius, top);
        nvgQuadTo(vg, iconX + tabW, top, iconX + tabW + cornerRadius, bodyTop); // the tab's sloped shoulder
        nvgLineTo(vg, iconX + iconWidth - cornerRadius, bodyTop);
        nvgQuadTo(vg, iconX + iconWidth, bodyTop, iconX + iconWidth, bodyTop + cornerRadius); // top-right corner
        nvgLineTo(vg, iconX + iconWidth, top + iconHeight);
        nvgClosePath(vg);
        nvgFillColor(vg, shade(0.72f)); // the back sits in shadow
        nvgFill(vg);

        // Front panel. Closed: square to the back, leaving the tab and a sliver of back showing.
        //
        // Open: a TRAPEZOID, splayed wider at the top and shifted right, which is how an open
        // folder is drawn everywhere. A rectangle merely nudged sideways was tried first and is
        // not legible at a 24px row -- at that size the shift is two pixels and reads as a
        // rendering slip rather than as a different icon.
        const float frontTop = bodyTop + 1.2f * unitScale;
        const float splayT = open ? 3.0f * unitScale : 0.0f; // top edge, shifted right
        const float splayB = open ? 1.4f * unitScale : 0.0f; // bottom edge, pulled in on both sides
        nvgBeginPath(vg);
        nvgMoveTo(vg, iconX + splayT, frontTop + cornerRadius);
        nvgQuadTo(vg, iconX + splayT, frontTop, iconX + splayT + cornerRadius, frontTop);
        nvgLineTo(vg, iconX + iconWidth - cornerRadius, frontTop);
        nvgQuadTo(vg, iconX + iconWidth, frontTop, iconX + iconWidth, frontTop + cornerRadius);
        nvgLineTo(vg, iconX + iconWidth - splayB, top + iconHeight - cornerRadius);
        nvgQuadTo(vg, iconX + iconWidth - splayB, top + iconHeight, iconX + iconWidth - splayB - cornerRadius,
                  top + iconHeight);
        nvgLineTo(vg, iconX + splayB + cornerRadius, top + iconHeight);
        nvgQuadTo(vg, iconX + splayB, top + iconHeight, iconX + splayB, top + iconHeight - cornerRadius);
        nvgClosePath(vg);
        // Lit from the top: a flat fill at this size reads as a solid block.
        nvgFillPaint(vg, nvgLinearGradient(vg, iconX, frontTop, iconX, top + iconHeight, shade(1.18f), toNVG(color)));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawFileIcon(int iconX, int iconY, int size, Color color)
    {
        // A sheet of paper with the top-right corner turned down. The fold is the whole point --
        // without it a page is a rounded rectangle, which is also what a button looks like.
        if (size <= 3 || !ensureFrame())
            return;
        const float unitScale = size / 16.0f;
        const float iconWidth = 12.5f * unitScale; // portrait, and narrower than the folder
        const float iconHeight = 15.0f * unitScale;
        const float left = iconX + (size - iconWidth) * 0.5f; // centred in the box, like the folder
        const float top = iconY + (size - iconHeight) * 0.5f;
        const float cornerRadius = std::max(1.0f, 1.4f * unitScale);
        const float fold = 5.4f * unitScale;

        auto shade = [&](float floatValue)
        {
            auto shadeChannel = [&](int value)
            {
                const float offset =
                    (floatValue <= 1.0f) ? value * floatValue : value + (255.0f - value) * (floatValue - 1.0f);
                return (unsigned char)std::min(255.0f, std::max(0.0f, offset));
            };
            return nvgRGBA(shadeChannel(color.red), shadeChannel(color.green), shadeChannel(color.blue), color.alpha);
        };

        // The page, with the top-right corner cut off along the fold line.
        nvgBeginPath(vg);
        nvgMoveTo(vg, left + cornerRadius, top);
        nvgLineTo(vg, left + iconWidth - fold, top);
        nvgLineTo(vg, left + iconWidth, top + fold); // the diagonal
        nvgLineTo(vg, left + iconWidth, top + iconHeight - cornerRadius);
        nvgQuadTo(vg, left + iconWidth, top + iconHeight, left + iconWidth - cornerRadius, top + iconHeight);
        nvgLineTo(vg, left + cornerRadius, top + iconHeight);
        nvgQuadTo(vg, left, top + iconHeight, left, top + iconHeight - cornerRadius);
        nvgLineTo(vg, left, top + cornerRadius);
        nvgQuadTo(vg, left, top, left + cornerRadius, top);
        nvgClosePath(vg);
        nvgFillPaint(vg, nvgLinearGradient(vg, left, top, left, top + iconHeight, shade(1.18f), toNVG(color)));
        nvgFill(vg);

        // The turned corner, darker so the fold reads as folded rather than as a notch.
        nvgBeginPath(vg);
        nvgMoveTo(vg, left + iconWidth - fold, top);
        nvgLineTo(vg, left + iconWidth, top + fold);
        nvgLineTo(vg, left + iconWidth - fold, top + fold);
        nvgClosePath(vg);
        nvgFillColor(vg, shade(0.58f));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawEvolveArrow(int iconX, int iconY, int size, Color color)
    {
        // A 16x16 art box scaled to `size`, with the extents symmetric in it: the head spans
        // x 2.5..13.5 about the centreline at 8, and the whole arrow runs y 2..14. One filled path
        // rather than a triangle plus a rectangle -- two primitives meeting at an edge leave a seam
        // the antialiasing shows as a lighter line straight across the arrow.
        if (size <= 3 || !ensureFrame())
            return;
        const float unitScale = size / 16.0f;
        const float centerX = iconX + 8.0f * unitScale;
        const float headBaseY = iconY + 8.5f * unitScale;
        const float halfShaft = 2.6f * unitScale;

        nvgBeginPath(vg);
        nvgMoveTo(vg, centerX, iconY + 2.0f * unitScale);            // apex
        nvgLineTo(vg, iconX + 13.5f * unitScale, headBaseY);         // head, right
        nvgLineTo(vg, centerX + halfShaft, headBaseY);               // step in to the shaft
        nvgLineTo(vg, centerX + halfShaft, iconY + 14.0f * unitScale);
        nvgLineTo(vg, centerX - halfShaft, iconY + 14.0f * unitScale);
        nvgLineTo(vg, centerX - halfShaft, headBaseY);
        nvgLineTo(vg, iconX + 2.5f * unitScale, headBaseY);          // head, left
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.red, color.green, color.blue, color.alpha));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawSearchIcon(int iconX, int iconY, int size, Color color)
    {
        // Proportions are a 16x16 art box scaled to `size`, chosen so the glyph's extents are
        // SYMMETRIC in it: the lens's outer edge and the handle's cap each land 0.8 units in from
        // their corner. That is what lets the caller centre the icon by centring its box, the same
        // contract the folder and file icons keep.
        if (size <= 3 || !ensureFrame())
            return;
        const float unitScale = size / 16.0f;
        const float rimWidth = std::max(1.5f, 2.0f * unitScale);
        const float handleWidth = std::max(1.6f, 2.4f * unitScale); // a grip is thicker than a rim
        const float lensRadius = 4.7f * unitScale;                  // the rim's centreline
        const float lensCenterX = iconX + 6.5f * unitScale;
        const float lensCenterY = iconY + 6.5f * unitScale;

        // Glass first, under the rim: a wash of the caller's own colour rather than a second
        // outline. Without it the lens is a hole, and a hole with a stick on it only reads as a
        // magnifier to someone who already knows that is what they are looking at.
        nvgBeginPath(vg);
        nvgCircle(vg, lensCenterX, lensCenterY, lensRadius);
        nvgFillColor(vg, nvgRGBA(color.red, color.green, color.blue,
                                 (unsigned char)(color.alpha * 0.16f)));
        nvgFill(vg);

        // The handle, running out along the diagonal from under the rim. Its start is placed so
        // the ROUND CAP's back edge lands exactly on the rim's inner edge: the handle is thicker
        // than the rim, so starting it any further in leaves a rounded nub intruding into the
        // glass, which at a glance reads as a chip out of the lens.
        constexpr float diagonal = 0.70710678f; // cos 45
        const float handleStart = lensRadius - rimWidth * 0.5f + handleWidth * 0.5f;
        nvgLineCap(vg, NVG_ROUND);
        nvgBeginPath(vg);
        nvgMoveTo(vg, lensCenterX + handleStart * diagonal, lensCenterY + handleStart * diagonal);
        nvgLineTo(vg, iconX + 14.0f * unitScale, iconY + 14.0f * unitScale);
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, handleWidth);
        nvgStroke(vg);
        // Nothing in this file wraps its state in nvgSave/nvgRestore -- every routine sets what it
        // needs -- so the cap has to go back, or the next stroke drawn this frame inherits it.
        nvgLineCap(vg, NVG_BUTT);

        // The rim last, over the handle's inner end.
        nvgBeginPath(vg);
        nvgCircle(vg, lensCenterX, lensCenterY, lensRadius);
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, rimWidth);
        nvgStroke(vg);
    }

    void PKSEFramebuffer::drawPointerCursor(int tipX, int tipY, int headHeight, Color color)
    {
        // The storage grid's cursor: a slim arrowhead, pointing straight down at the slot, with no
        // shaft. Four corners, in art units 18 wide by 26 tall with the point at (0, 0).
        //
        // This is drawn rather than copied. Tracing an existing 3DS-era pointer sprite was tried first
        // and abandoned: it is 19x28 pixels, so at the size PKSE needs it every wobble in
        // the original outline shows. The proportions below were instead chosen against a real 78px
        // slot disc at the sizes the app actually uses.
        //
        // Two of them matter more than they look:
        //   * The notch depth is tied to the width. Arm thickness is w*n/hypot(w,26), so a narrow
        //     head with a deep notch thins the arms until the outline is most of what is left and
        //     the colour barely shows -- hence the shallower -20 that goes with these narrow wings.
        //   * The corners are mitred. Rounding them swallows the notch and the point, and what
        //     survives reads as a circle.
        // The body is bevelled rather than flat-filled, which is what stops a solid silhouette from
        // reading as a flat cut-out.
        static const signed char arrowOutline[][2] = {
            {0, 0},    // the point
            {-9, -26}, // left wing
            {0, -20},  // the notch in the back edge
            {9, -26},  // right wing
        };
        constexpr int count = (int)(sizeof(arrowOutline) / sizeof(arrowOutline[0]));
        if (headHeight <= 6 || !ensureFrame())
            return;

        // The outline is drawn UNDER the body, not over it. Strokes are centred on the path, so at a
        // ~60-degree apex they cover the fill for several pixels and the point reads as a hollow V.
        // Stroking wide first and filling on top leaves the outline entirely outside the body, and
        // the body itself runs all the way into the point.
        //
        // `headHeight` is the head itself -- the part you actually see as the arrow. Below it the
        // mitred outline runs on to a point, overshooting the apex by outer/sin(halfAngle). That is
        // measured off the art rather than hardcoded, because it depends strongly on how slim the
        // head is (2.0x the outline thickness at the widest head tried, 3.1x at this one); pin it to
        // a constant and a reshape silently pokes the point lower than the caller reserved room for.
        float halfW = 1.0f, artH = 1.0f;
        for (int index = 0; index < count; ++index)
        {
            halfW = std::max(halfW, (float)std::abs(arrowOutline[index][0]));
            artH = std::max(artH, (float)-arrowOutline[index][1]);
        }
        // outer is 7% of the WHOLE cursor, and the whole cursor is head + over -- so solve for over
        // rather than measuring it off a total we do not have yet.
        const float floatValue = 0.07f * std::sqrt(halfW * halfW + artH * artH) / halfW;
        const float over = headHeight * floatValue / (1.0f - floatValue);
        const float outer = (headHeight + over) * 0.07f; // outline thickness outside the body
        const float artScale = headHeight / artH;
        const float baseY = tipY - over; // the body's apex; the outline's point is at tipY
        auto trace = [&](float offsetX, float offsetY)
        {
            nvgBeginPath(vg);
            for (int index = 0; index < count; ++index)
            {
                const float pointX = tipX + offsetX + arrowOutline[index][0] * artScale;
                const float pointY = baseY + offsetY + arrowOutline[index][1] * artScale;
                if (index == 0)
                    nvgMoveTo(vg, pointX, pointY);
                else
                    nvgLineTo(vg, pointX, pointY);
            }
            nvgClosePath(vg);
        };
        auto shade = [&](float floatValue) { // f < 1 darkens, f > 1 lightens toward white
            auto shadeChannel = [&](int value)
            {
                const float shadedChannel =
                    (floatValue <= 1.0f) ? value * floatValue : value + (255.0f - value) * (floatValue - 1.0f);
                return (unsigned char)std::min(255.0f, std::max(0.0f, shadedChannel));
            };
            return nvgRGBA(shadeChannel(color.red), shadeChannel(color.green), shadeChannel(color.blue), color.alpha);
        };

        // Drop shadow: the whole outer silhouette (stroke + fill), offset.
        trace(headHeight * 0.04f, headHeight * 0.07f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 105));
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 105));
        nvgStrokeWidth(vg, outer * 2.0f);
        nvgStroke(vg);
        nvgFill(vg);

        // Widest stroke first, then a narrower dark one over its inner half, then the body over
        // that: what is left outside the body is a dark rim with a white outline beyond it.
        trace(0.0f, 0.0f);
        nvgStrokeColor(vg, nvgRGBA(242, 242, 242, 240));
        nvgStrokeWidth(vg, outer * 2.0f);
        nvgStroke(vg);
        nvgStrokeColor(vg, shade(0.42f));
        nvgStrokeWidth(vg, outer * 0.9f);
        nvgStroke(vg);
        // Lit from the upper-left, falling off across the head.
        NVGpaint body = nvgLinearGradient(vg, tipX - halfW * artScale, baseY - artH * artScale, tipX + halfW * artScale,
                                          baseY, shade(1.5f), toNVG(color));
        nvgFillPaint(vg, body);
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawPill(int pillX, int pillY, int pillWidth, int pillHeight, Color color)
    {
        // PKForge has no stadiums: rows, tabs and chips are UiTokens.ControlRadius (4) rectangles.
        if (isPkForgeTheme())
        {
            if (isPkForgeFocusFill(color))
                pkSelectedButton(pillX, pillY, pillWidth, pillHeight, 4);
            else
                fillRoundedRaw((float)pillX, (float)pillY, (float)pillWidth, (float)pillHeight, 4.0f, color);
            return;
        }
        drawFilledRoundedRect(pillX, pillY, pillWidth, pillHeight, pillHeight / 2, color);
    }
    void PKSEFramebuffer::drawPillBorder(int pillX, int pillY, int pillWidth, int pillHeight, Color color,
                                         int thickness)
    {
        drawRoundedRect(pillX, pillY, pillWidth, pillHeight, isPkForgeTheme() ? 4 : pillHeight / 2, color, thickness);
    }

    void PKSEFramebuffer::drawFilledEllipse(int centerX, int centerY, int radiusX, int rectY, Color color)
    {
        if (radiusX <= 0 || rectY <= 0 || !ensureFrame())
            return;
        nvgBeginPath(vg);
        nvgEllipse(vg, (float)centerX, (float)centerY, (float)radiusX, (float)rectY);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawSoftShadow(int shadowX, int shadowY, int shadowWidth, int shadowHeight, int cornerRadius)
    {
        if (shadowWidth <= 0 || shadowHeight <= 0 || !ensureFrame())
            return;
        if (isPkForgeTheme())
        {
            // PksmPaint.Panel: a hard 3 dp pixel drop in LogoVoid, no glow.
            fillRoundedRaw(shadowX + 3.0f, shadowY + 3.0f, (float)shadowWidth, (float)shadowHeight,
                           (float)std::min(cornerRadius, 6), Color(0x14, 0x12, 0x1D, 0x88));
            return;
        }
        notePopupRect((float)shadowX, (float)shadowY, (float)shadowWidth, (float)shadowHeight);
        // Real feathered drop shadow: box gradient in the ring around the element (hole in the middle).
        NVGpaint shadowPaint =
            nvgBoxGradient(vg, (float)shadowX, shadowY + 3.0f, (float)shadowWidth, (float)shadowHeight,
                           cornerRadius + 2.0f, 12.0f, nvgRGBA(0, 0, 0, 110), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(vg);
        nvgRect(vg, shadowX - 14.0f, shadowY - 14.0f, shadowWidth + 28.0f, shadowHeight + 28.0f);
        nvgRoundedRect(vg, (float)shadowX, (float)shadowY, (float)shadowWidth, (float)shadowHeight,
                       (float)cornerRadius);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawVerticalGradient(int gradientX, int gradientY, int gradientWidth, int gradientHeight,
                                               Color top, Color bottom)
    {
        if (gradientWidth <= 0 || gradientHeight <= 0 || !ensureFrame())
            return;
        notePopupRect((float)gradientX, (float)gradientY, (float)gradientWidth, (float)gradientHeight);
        NVGpaint gradientPaint = nvgLinearGradient(vg, (float)gradientX, (float)gradientY, (float)gradientX,
                                                   (float)(gradientY + gradientHeight), toNVG(top), toNVG(bottom));
        nvgBeginPath(vg);
        nvgRect(vg, (float)gradientX, (float)gradientY, (float)gradientWidth, (float)gradientHeight);
        nvgFillPaint(vg, gradientPaint);
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawStatHexagon(int centerX, int centerY, int hexRadius, const float *values, int count,
                                          float maxValue, Color fill, Color webColor, Color outline)
    {
        if (hexRadius <= 0 || count <= 0 || !values || !ensureFrame())
            return;
        static const double axisAngles[6] = {90.0, 30.0, -30.0, 270.0, 210.0, 150.0};
        const double piRadians = 3.14159265358979323846;
        int axisCount = count < 6 ? count : 6;
        auto axisX = [&](double angleDegrees, double ringRadius)
        { return centerX + (float)(std::cos(angleDegrees * piRadians / 180.0) * ringRadius); };
        auto axisY = [&](double angleDegrees, double ringRadius)
        { return centerY - (float)(std::sin(angleDegrees * piRadians / 180.0) * ringRadius); };

        // Web rings.
        for (int ring = 1; ring <= 4; ++ring)
        {
            double webRingRadius = hexRadius * (ring / 4.0);
            nvgBeginPath(vg);
            for (int index = 0; index < axisCount; ++index)
            {
                float panelX = axisX(axisAngles[index], webRingRadius), py = axisY(axisAngles[index], webRingRadius);
                if (index == 0)
                    nvgMoveTo(vg, panelX, py);
                else
                    nvgLineTo(vg, panelX, py);
            }
            nvgClosePath(vg);
            nvgStrokeColor(vg, toNVG(webColor));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
        // Spokes.
        nvgBeginPath(vg);
        for (int index = 0; index < axisCount; ++index)
        {
            nvgMoveTo(vg, (float)centerX, (float)centerY);
            nvgLineTo(vg, axisX(axisAngles[index], hexRadius), axisY(axisAngles[index], hexRadius));
        }
        nvgStrokeColor(vg, toNVG(webColor));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        // Data polygon.
        nvgBeginPath(vg);
        for (int index = 0; index < axisCount; ++index)
        {
            double value = maxValue > 0.0f ? values[index] / maxValue : 0.0;
            value = std::clamp(value, 0.06, 1.0);
            float panelX = axisX(axisAngles[index], hexRadius * value),
                  py = axisY(axisAngles[index], hexRadius * value);
            if (index == 0)
                nvgMoveTo(vg, panelX, py);
            else
                nvgLineTo(vg, panelX, py);
        }
        nvgClosePath(vg);
        nvgFillColor(vg, toNVG(fill));
        nvgFill(vg);
        nvgStrokeColor(vg, toNVG(outline));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);

        // Vertex dots on the data polygon
        for (int index = 0; index < axisCount; ++index)
        {
            double value = maxValue > 0.0f ? values[index] / maxValue : 0.0;
            value = std::clamp(value, 0.06, 1.0);
            float panelX = axisX(axisAngles[index], hexRadius * value),
                  py = axisY(axisAngles[index], hexRadius * value);
            nvgBeginPath(vg);
            nvgCircle(vg, panelX, py, 3.0f);
            nvgFillColor(vg, toNVG(outline));
            nvgFill(vg);
        }
    }

    void PKSEFramebuffer::applyTextStyle(TextStyle style) const
    {
        const bool usePixel = (g_themeMode == ThemeMode::RetroDS && fontPixel >= 0);
        nvgFontFaceId(vg, usePixel ? fontPixel : fontSans);
        nvgFontSize(vg, usePixel ? PIXEL_FONT_SIZES[static_cast<int>(style)] : FONT_SIZES[static_cast<int>(style)]);
    }

    void PKSEFramebuffer::drawText(int textX, int textY, const char *text, Color color, TextStyle style)
    {
        if (!text || !*text)
            return;
        drawText(textX, textY, std::string(text), color, style);
    }

    void PKSEFramebuffer::drawText(int textX, int textY, const std::string &text, Color color, TextStyle style)
    {
        if (text.empty() || !ensureFrame())
            return;
        const size_t shown = typedBytes(text);
        if (shown == 0)
            return;
        const char *end = text.c_str() + shown;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, toNVG(color));
        nvgText(vg, (float)textX, (float)textY, text.c_str(), end);
        // Faux-bold for the display styles (Nunito is loaded regular; re-stroke slightly offset).
        // For pixel fonts, we skip faux-bolding so glyph edges stay crisp without blur.
        const bool usePixel = (g_themeMode == ThemeMode::RetroDS && fontPixel >= 0);
        if (!usePixel && (style == TextStyle::Heading || style == TextStyle::Title))
            nvgText(vg, textX + 0.6f, (float)textY, text.c_str(), end);
    }

    void PKSEFramebuffer::drawSymbol(int symbolX, int symbolY, const std::string &symbol, Color color, TextStyle style)
    {
        // Symbol fonts are fallbacks on "sans", so nvgText resolves ♂/♀/★/♥/◀/▶ automatically.
        if (symbol.empty() || !ensureFrame())
            return;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, toNVG(color));
        nvgText(vg, (float)symbolX, (float)symbolY, symbol.c_str(), nullptr);
    }

    void PKSEFramebuffer::measureText(const std::string &text, int &outWidth, int &outHeight, TextStyle style)
    {
        outWidth = 0;
        outHeight = 0;
        if (!vg)
            return;
        applyTextStyle(style);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        float advanceWidth = nvgTextBounds(vg, 0, 0, text.c_str(), nullptr, nullptr);
        outWidth = (int)std::ceil(advanceWidth);
        outHeight = lineHeight(style);
    }

    int PKSEFramebuffer::textYCenteredOn(int centerY, TextStyle style) const
    {
        if (!vg)
            return centerY;
        applyTextStyle(style);
        // TWO NORMALISATIONS, AND BOTH MATTER. drawText aligns TOP, and fontstash places the baseline
        // `ascender` below that y, where ascender is the font's ascent over (ascent - descent) times
        // the size (fontstash.h: font->ascender). The glyphs themselves are scaled per EM instead
        // (stbtt_ScaleForMappingEmToPixels), so a capital stands size * capHeight/em above that
        // baseline. Measured on a console screenshot, this puts a 26px heading's capitals within half
        // a pixel of where it predicts; the hand-picked offsets it replaces left them 5px high.
        float ascender = 0, descender = 0, lineHeightPixels = 0;
        nvgTextMetrics(vg, &ascender, &descender, &lineHeightPixels);
        const bool usePixel = (g_themeMode == ThemeMode::RetroDS && fontPixel >= 0);
        const float fontSize = usePixel ? PIXEL_FONT_SIZES[static_cast<int>(style)] : FONT_SIZES[static_cast<int>(style)];
        const float capRatio = usePixel ? 0.70f : NUNITO_CAP_HEIGHT_PER_EM;
        const float capitalHeight = fontSize * capRatio;
        return static_cast<int>(std::lround(static_cast<float>(centerY) - ascender + capitalHeight / 2.0f));
    }

    int PKSEFramebuffer::lineHeight(TextStyle style) const
    {
        if (!vg)
            return 0;
        applyTextStyle(style);
        float ascender = 0, desc = 0, lh = 0;
        nvgTextMetrics(vg, &ascender, &desc, &lh);
        return (int)std::ceil(lh);
    }

    int PKSEFramebuffer::nvgImageFor(const unsigned char *data, int imageWidth, int imageHeight, int channels,
                                     bool shrinking)
    {
        if (!vg || !data || imageWidth <= 0 || imageHeight <= 0)
            return -1;
        // Small art (box icons, items, balls, PKSM icons) is pixel art: drawn at its own size or
        // larger it goes nearest-neighbour in EVERY theme, so its pixels stay square -- the linear
        // filter smeared them in the HOME themes. Shrunk, nearest would drop whole rows, so a
        // second, mipmapped copy serves that case. Larger art (HD renders, covers) is always mipmapped.
        const bool small = imageWidth <= 128 && imageHeight <= 128;
        const bool pixelArt = small && !shrinking;
        auto &cache = (small && shrinking) ? smoothImageCache : imageCache;
        auto iterator = cache.find(data);
        if (iterator != cache.end())
            return iterator->second;
        const int flags = pixelArt ? NVG_IMAGE_NEAREST : NVG_IMAGE_GENERATE_MIPMAPS;
        int image = -1;
        if (channels == 4)
        {
            image = nvgCreateImageRGBA(vg, imageWidth, imageHeight, flags, data);
        }
        else if (channels == 3)
        {
            std::vector<unsigned char> rgba(static_cast<size_t>(imageWidth) * imageHeight * 4);
            for (int index = 0; index < imageWidth * imageHeight; ++index)
            {
                rgba[index * 4 + 0] = data[index * 3 + 0];
                rgba[index * 4 + 1] = data[index * 3 + 1];
                rgba[index * 4 + 2] = data[index * 3 + 2];
                rgba[index * 4 + 3] = 255;
            }
            image = nvgCreateImageRGBA(vg, imageWidth, imageHeight, flags, rgba.data());
        }
        else
        {
            return -1;
        }
        cache[data] = image;
        return image;
    }

    void PKSEFramebuffer::drawImage(int imageX, int imageY, int imageWidth, int imageHeight, const unsigned char *data,
                                    int channels)
    {
        if (!ensureFrame())
            return;
        int image = nvgImageFor(data, imageWidth, imageHeight, channels);
        if (image < 0)
            return;
        NVGpaint imagePaint =
            nvgImagePattern(vg, (float)imageX, (float)imageY, (float)imageWidth, (float)imageHeight, 0.0f, image, 1.0f);
        nvgBeginPath(vg);
        nvgRect(vg, (float)imageX, (float)imageY, (float)imageWidth, (float)imageHeight);
        nvgFillPaint(vg, imagePaint);
        nvgFill(vg);
    }

    void PKSEFramebuffer::drawImageScaled(int imageX, int imageY, int imageWidth, int imageHeight, int destWidth,
                                          int destHeight, const unsigned char *data, int channels, uint8_t alpha)
    {
        if (!ensureFrame())
            return;
        const bool shrinking = destWidth < imageWidth || destHeight < imageHeight;
        int image = nvgImageFor(data, imageWidth, imageHeight, channels, shrinking);
        if (image < 0)
            return;
        const float a = static_cast<float>(alpha) / 255.0f;
        NVGpaint imagePaint =
            nvgImagePattern(vg, (float)imageX, (float)imageY, (float)destWidth, (float)destHeight, 0.0f, image, a);
        nvgBeginPath(vg);
        nvgRect(vg, (float)imageX, (float)imageY, (float)destWidth, (float)destHeight);
        nvgFillPaint(vg, imagePaint);
        nvgFill(vg);
    }

    // Pixel art (box icons, DS sprites) is at most 128 px a side; HD renders are far larger.
    static bool isPixelArtSize(int sourceWidth, int sourceHeight) { return sourceWidth <= 128 && sourceHeight <= 128; }

    void PKSEFramebuffer::fitSprite(int boxWidth, int boxHeight, int sourceWidth, int sourceHeight, int &outWidth,
                                    int &outHeight, int *pixelScale) const
    {
        if (pixelScale)
            *pixelScale = 0;
        if (sourceWidth <= 0 || sourceHeight <= 0 || boxWidth <= 0 || boxHeight <= 0)
        {
            outWidth = outHeight = 0;
            return;
        }
        const float scale = std::min(static_cast<float>(boxWidth) / sourceWidth,
                                     static_cast<float>(boxHeight) / sourceHeight);
        if (isPixelArtSize(sourceWidth, sourceHeight) && scale >= 1.0f)
        {
            // Whole multiples only, or the pixels come out uneven (every few rows doubled). It may
            // round UP when it is nearly there: the icon frames carry several transparent pixels on
            // every side, so a 2x a little over the box still reads as fitting it.
            const int whole = std::max(1, static_cast<int>(scale + 0.3f));
            outWidth = sourceWidth * whole;
            outHeight = sourceHeight * whole;
            if (pixelScale)
                *pixelScale = whole;
            return;
        }
        // HD art, or pixel art that has to shrink: keep the aspect (a 68x56 icon is not square).
        outWidth = std::max(1, static_cast<int>(std::lround(sourceWidth * scale)));
        outHeight = std::max(1, static_cast<int>(std::lround(sourceHeight * scale)));
    }

    void PKSEFramebuffer::drawSpriteFit(int boxX, int boxY, int boxWidth, int boxHeight, int sourceWidth,
                                        int sourceHeight, const unsigned char *data, int channels, uint8_t alpha)
    {
        int drawWidth, drawHeight;
        fitSprite(boxWidth, boxHeight, sourceWidth, sourceHeight, drawWidth, drawHeight);
        if (drawWidth <= 0)
            return;
        drawImageScaled(boxX + (boxWidth - drawWidth) / 2, boxY + (boxHeight - drawHeight) / 2, sourceWidth,
                        sourceHeight, drawWidth, drawHeight, data, channels, alpha);
    }

    void PKSEFramebuffer::drawSpriteIdle(int spriteX, int spriteY, int boxWidth, int boxHeight, int sourceWidth,
                                         int sourceHeight, const unsigned char *data, int channels, float phase)
    {
        if (!data || boxWidth <= 0 || boxHeight <= 0)
            return;
        int fitWidth, fitHeight, pixelScale;
        fitSprite(boxWidth, boxHeight, sourceWidth, sourceHeight, fitWidth, fitHeight, &pixelScale);
        if (fitWidth <= 0)
            return;
        double bobPhase = std::sin(totalSeconds * 2.7 + phase);
        int drawWidth = fitWidth, drawHeight = fitHeight, bobOffset;
        if (pixelScale > 0)
        {
            // Pixel art bobs by whole art pixels and never squashes: a 3% stretch on nearest-sampled
            // pixels doubles a different row every frame, which reads as shimmering.
            bobOffset = static_cast<int>(std::lround(bobPhase * 1.0)) * pixelScale;
        }
        else
        {
            bobOffset = (int)std::lround(bobPhase * 3.0);
            drawWidth = (int)std::lround(fitWidth * (1.0 - bobPhase * 0.03));
            drawHeight = (int)std::lround(fitHeight * (1.0 + bobPhase * 0.03));
        }
        // Feet on the box's floor, centred across it.
        int baseY = spriteY + boxHeight;
        int drawX = spriteX + (boxWidth - drawWidth) / 2;
        int drawY = baseY - drawHeight - bobOffset;
        int shadowRadiusX = (int)std::lround(fitWidth * 0.30 - bobPhase * 2.0);
        int shadowRadiusY = (int)std::lround(5.0 - bobPhase * 1.0);
        drawFilledEllipse(spriteX + boxWidth / 2, baseY - 1, shadowRadiusX, shadowRadiusY, Color(0, 0, 0, 80));
        drawImageScaled(drawX, drawY, sourceWidth, sourceHeight, drawWidth, drawHeight, data, channels);
    }

    void PKSEFramebuffer::drawCard(int cardX, int cardY, int cardWidth, int cardHeight)
    {
        if (isPkForgeTheme())
        {
            pkPanel(cardX, cardY, cardWidth, cardHeight);
            return;
        }
        drawFilledRoundedRect(cardX, cardY, cardWidth, cardHeight, 14, Colors::Panel);
        drawRoundedRect(cardX, cardY, cardWidth, cardHeight, 14, Colors::Border, 1);
    }

    void PKSEFramebuffer::drawSelectionHighlight(int highlightX, int highlightY, int highlightWidth,
                                                 int highlightHeight)
    {
        if (isPkForgeTheme())
        {
            pkSelectedButton(highlightX, highlightY, highlightWidth, highlightHeight, 4);
            return;
        }
        drawFilledRoundedRect(highlightX, highlightY, highlightWidth, highlightHeight, 10, Colors::Selected);
        drawRoundedRect(highlightX, highlightY, highlightWidth, highlightHeight, 10, Colors::Accent, 2);
    }

    void PKSEFramebuffer::drawCardBand(int bandX, int bandY, int bandWidth, int bandHeight)
    {
        if (isPkForgeTheme())
        {
            pkHeaderStrip(bandX + 6, bandY + 6, bandWidth - 12, bandHeight - 8);
            return;
        }
        drawFilledRoundedRect(bandX, bandY, bandWidth, bandHeight, 16, Colors::AccentDim);
        drawFilledRect(bandX, bandY + bandHeight - 16, bandWidth, 16, Colors::AccentDim); // square the bottom edge
    }

    void PKSEFramebuffer::drawHDivider(int dividerX, int dividerY, int dividerWidth)
    {
        if (isPkForgeTheme())
        {
            drawFilledRect(dividerX, dividerY, dividerWidth, 2, Color(0x2B, 0x4E, 0x95, 0x70)); // UiTokens.Divider
            return;
        }
        drawFilledRect(dividerX, dividerY, dividerWidth, 2, Colors::Border);
    }

    int PKSEFramebuffer::drawTypeBadge(int badgeX, int badgeY, const std::string &typeName, Color typeColor)
    {
        const int badgeHeight = 24;
        int textWidth, th;
        measureText(typeName, textWidth, th, TextStyle::Caption);
        const int padX = 12, total = textWidth + padX * 2;
        drawFilledRoundedRect(badgeX, badgeY, total, badgeHeight, badgeHeight / 2, typeColor);
        int luminance = (typeColor.red * 299 + typeColor.green * 587 + typeColor.blue * 114) / 1000;
        Color labelColor = luminance > 150 ? Color(30, 30, 36) : Color(245, 245, 250);
        drawText(badgeX + padX, badgeY + (badgeHeight - th) / 2, typeName, labelColor, TextStyle::Caption);
        return total;
    }

    int PKSEFramebuffer::drawTypePill(int x, int y, int width, int height, uint8_t typeId)
    {
        if (!ensureFrame())
            return width;
        const char *rawName = (typeId < Names::getTypeNameCount()) ? Names::getTypeName(typeId) : "???";
        std::string typeName = rawName;
        for (char &c : typeName)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        int textWidth = 0, th = 0;
        measureText(typeName, textWidth, th, TextStyle::Caption);

        int pillWidth = (width > 0) ? width : (textWidth + 14);
        Color typeColor = (typeId < 19) ? Colors::TypeColors[typeId] : Colors::Gray;

        // Base pill
        drawFilledRoundedRect(x, y, pillWidth, height, height / 2, typeColor);

        // Specular highlight
        if (height >= 10)
        {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, static_cast<float>(x) + 1.0f, static_cast<float>(y) + 1.0f,
                           static_cast<float>(pillWidth) - 2.0f, static_cast<float>(height) * 0.44f,
                           static_cast<float>(height) * 0.22f);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 45));
            nvgFill(vg);
        }

        // Contrast label color
        int luminance = (typeColor.red * 299 + typeColor.green * 587 + typeColor.blue * 114) / 1000;
        Color labelColor = luminance > 155 ? Color(25, 25, 30) : Color(255, 255, 255);

        int tx = x + (pillWidth - textWidth) / 2;
        int ty = y + (height - th) / 2;
        drawText(tx, ty, typeName, labelColor, TextStyle::Caption);

        return pillWidth;
    }

    void PKSEFramebuffer::drawMoveCategoryIcon(int iconX, int iconY, int size, uint8_t category)
    {
        if (size <= 0 || !ensureFrame())
            return;

        const float cx = static_cast<float>(iconX) + static_cast<float>(size) * 0.5f;
        const float cy = static_cast<float>(iconY) + static_cast<float>(size) * 0.5f;
        const float r = static_cast<float>(size) * 0.5f;
        constexpr float pi = 3.14159265358979323846f;

        if (category == 1) // Physical (Burst Star)
        {
            constexpr int points = 8;
            nvgBeginPath(vg);
            for (int i = 0; i < points * 2; ++i)
            {
                float angle = static_cast<float>(i) * (pi / static_cast<float>(points)) - (pi * 0.5f);
                float rad = (i % 2 == 0) ? r : (r * 0.44f);
                float px = cx + std::cos(angle) * rad;
                float py = cy + std::sin(angle) * rad;
                if (i == 0)
                    nvgMoveTo(vg, px, py);
                else
                    nvgLineTo(vg, px, py);
            }
            nvgClosePath(vg);
            nvgFillColor(vg, toNVG(Colors::CategoryPhysical));
            nvgFill(vg);

            // Subtle bright starlet core
            nvgBeginPath(vg);
            for (int i = 0; i < points * 2; ++i)
            {
                float angle = static_cast<float>(i) * (pi / static_cast<float>(points)) - (pi * 0.5f);
                float rad = (i % 2 == 0) ? (r * 0.40f) : (r * 0.18f);
                float px = cx + std::cos(angle) * rad;
                float py = cy + std::sin(angle) * rad;
                if (i == 0)
                    nvgMoveTo(vg, px, py);
                else
                    nvgLineTo(vg, px, py);
            }
            nvgClosePath(vg);
            nvgFillColor(vg, nvgRGBA(255, 235, 170, 230));
            nvgFill(vg);
        }
        else if (category == 2) // Special (Concentric Energy Ripples)
        {
            // Outer ring
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.88f);
            nvgStrokeWidth(vg, std::max(1.5f, static_cast<float>(size) * 0.09f));
            nvgStrokeColor(vg, toNVG(Colors::CategorySpecial));
            nvgStroke(vg);

            // Inner ring
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.58f);
            nvgStrokeWidth(vg, std::max(1.5f, static_cast<float>(size) * 0.09f));
            nvgStrokeColor(vg, toNVG(Colors::CategorySpecial));
            nvgStroke(vg);

            // Core disc
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.28f);
            nvgFillColor(vg, toNVG(Colors::CategorySpecial));
            nvgFill(vg);

            // Center spark
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.12f);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 200));
            nvgFill(vg);
        }
        else // Status (Yin-Yang swirl / split circle)
        {
            // Dark base disc
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.92f);
            nvgFillColor(vg, nvgRGBA(70, 72, 82, 255));
            nvgFill(vg);

            // Light swirl half
            float R = r * 0.92f;
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, R, -pi * 0.5f, pi * 0.5f, NVG_CW);
            nvgArc(vg, cx, cy + R * 0.5f, R * 0.5f, pi * 0.5f, -pi * 0.5f, NVG_CW);
            nvgArc(vg, cx, cy - R * 0.5f, R * 0.5f, pi * 0.5f, -pi * 0.5f, NVG_CCW);
            nvgClosePath(vg);
            nvgFillColor(vg, toNVG(Colors::CategoryStatus));
            nvgFill(vg);

            // Small dot 1 (dark on light half)
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy - R * 0.5f, R * 0.18f);
            nvgFillColor(vg, nvgRGBA(70, 72, 82, 255));
            nvgFill(vg);

            // Small dot 2 (light on dark half)
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy + R * 0.5f, R * 0.18f);
            nvgFillColor(vg, toNVG(Colors::CategoryStatus));
            nvgFill(vg);

            // Outline
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, R);
            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, nvgRGBA(180, 180, 195, 120));
            nvgStroke(vg);
        }
    }

    void PKSEFramebuffer::drawGaugeBar(int x, int y, int width, int height, float fraction, Color fillColor,
                                       Color trackColor)
    {
        if (width <= 0 || height <= 0 || !ensureFrame())
            return;

        // Background track
        drawFilledRoundedRect(x, y, width, height, height / 2, trackColor);

        // Clamped filled fraction
        float clamped = std::clamp(fraction, 0.0f, 1.0f);
        int fillW = static_cast<int>(std::round(static_cast<float>(width) * clamped));
        if (fillW > 0)
        {
            nvgSave(vg);
            nvgIntersectScissor(vg, static_cast<float>(x), static_cast<float>(y), static_cast<float>(fillW),
                                static_cast<float>(height));
            drawFilledRoundedRect(x, y, width, height, height / 2, fillColor);
            nvgRestore(vg);
        }
    }

    void PKSEFramebuffer::drawCornerBrackets(int boxX, int boxY, int boxWidth, int boxHeight, int bracketLen,
                                             int thickness, Color color, float pulseOffset)
    {
        if (boxWidth <= 0 || boxHeight <= 0 || bracketLen <= 0 || thickness <= 0 || !ensureFrame())
            return;

        const float x0 = static_cast<float>(boxX) - pulseOffset;
        const float y0 = static_cast<float>(boxY) - pulseOffset;
        const float x1 = static_cast<float>(boxX + boxWidth) + pulseOffset;
        const float y1 = static_cast<float>(boxY + boxHeight) + pulseOffset;
        const float len = static_cast<float>(bracketLen);

        nvgBeginPath(vg);

        // Top-Left corner: (x0 + len, y0) -> (x0, y0) -> (x0, y0 + len)
        nvgMoveTo(vg, x0 + len, y0);
        nvgLineTo(vg, x0, y0);
        nvgLineTo(vg, x0, y0 + len);

        // Top-Right corner: (x1 - len, y0) -> (x1, y0) -> (x1, y0 + len)
        nvgMoveTo(vg, x1 - len, y0);
        nvgLineTo(vg, x1, y0);
        nvgLineTo(vg, x1, y0 + len);

        // Bottom-Left corner: (x0 + len, y1) -> (x0, y1) -> (x0, y1 - len)
        nvgMoveTo(vg, x0 + len, y1);
        nvgLineTo(vg, x0, y1);
        nvgLineTo(vg, x0, y1 - len);

        // Bottom-Right corner: (x1 - len, y1) -> (x1, y1) -> (x1, y1 - len)
        nvgMoveTo(vg, x1 - len, y1);
        nvgLineTo(vg, x1, y1);
        nvgLineTo(vg, x1, y1 - len);

        nvgStrokeWidth(vg, static_cast<float>(thickness));
        nvgStrokeColor(vg, toNVG(color));
        nvgLineCap(vg, NVG_SQUARE);
        nvgLineJoin(vg, NVG_MITER);
        nvgStroke(vg);
    }

    static inline Color mixColors(Color a, Color b, float t)
    {
        float inv = 1.0f - t;
        uint8_t r = static_cast<uint8_t>(std::clamp(a.red * inv + b.red * t, 0.0f, 255.0f));
        uint8_t g = static_cast<uint8_t>(std::clamp(a.green * inv + b.green * t, 0.0f, 255.0f));
        uint8_t bVal = static_cast<uint8_t>(std::clamp(a.blue * inv + b.blue * t, 0.0f, 255.0f));
        uint8_t alpha = static_cast<uint8_t>(std::clamp(a.alpha * inv + b.alpha * t, 0.0f, 255.0f));
        return Color(r, g, bVal, alpha);
    }

    static inline Color lighterColor(Color c, float t)
    {
        return mixColors(c, Color(255, 255, 255, c.alpha), t);
    }

    static inline Color darkerColor(Color c, float t)
    {
        return mixColors(c, Color(0, 0, 0, c.alpha), t);
    }

    void PKSEFramebuffer::drawRetroDSPanel(int x, int y, int width, int height, int radius, Color body)
    {
        pkPanel(x, y, width, height, body, radius);
    }

    void PKSEFramebuffer::drawRetroDSHeaderStrip(int x, int y, int width, int height, const char *label, Color accent,
                                                 TextStyle style)
    {
        pkHeaderStrip(x, y, width, height, label ? label : "", accent, false, style);
    }

    void PKSEFramebuffer::drawRetroDSButton(int x, int y, int width, int height, const char *label, bool selected,
                                            Color accent, int radius)
    {
        if (selected)
            pkSelectedButton(x, y, width, height, radius, accent == Colors::Accent ? Pksm::HeaderBlue : accent);
        else
            pkBlackButton(x, y, width, height, radius);
        if (label && label[0] != '\0')
        {
            int tw = 0, th = 0;
            measureText(label, tw, th, TextStyle::Body);
            drawText(x + (width - tw) / 2, y + (height - th) / 2, label, selected ? Pksm::Ink : Pksm::Ink,
                     TextStyle::Body);
        }
    }

    // ── PKForge chrome (PKForge.Chrome/PksmPaint.cs) ──────────────────────────────────────────

    void PKSEFramebuffer::fillRoundedRaw(float x, float y, float w, float h, float radius, Color color)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        notePopupRect(x, y, w, h);
        radius = std::max(0.0f, std::min(radius, std::min(w, h) / 2));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, radius);
        nvgFillColor(vg, toNVG(color));
        nvgFill(vg);
    }

    void PKSEFramebuffer::fillGradientRaw(float x, float y, float w, float h, float radius, Color top, Color bottom)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        notePopupRect(x, y, w, h);
        radius = std::max(0.0f, std::min(radius, std::min(w, h) / 2));
        NVGpaint paint = nvgLinearGradient(vg, 0, y, 0, y + h, toNVG(top), toNVG(bottom));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, radius);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    void PKSEFramebuffer::strokeRoundedRaw(float x, float y, float w, float h, float radius, Color color, float t)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        radius = std::max(0.0f, std::min(radius, std::min(w, h) / 2));
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, radius);
        nvgStrokeColor(vg, toNVG(color));
        nvgStrokeWidth(vg, t);
        nvgStroke(vg);
    }

    // The exact visual rhythm of the logo background: navy field, crisp cobalt grid.
    void PKSEFramebuffer::pkLogoGrid(int x, int y, int w, int h, int cell, int lineWidth)
    {
        if (!ensureFrame())
            return;
        drawFilledRect(x, y, w, h, Pksm::Housing);
        const Color line(Pksm::LogoGrid.red, Pksm::LogoGrid.green, Pksm::LogoGrid.blue, 0xD0);
        const int ox = x % cell, oy = y % cell;
        for (int gx = x - ox; gx <= x + w; gx += cell)
            drawFilledRect(gx, y, lineWidth, h, line);
        for (int gy = y - oy; gy <= y + h; gy += cell)
            drawFilledRect(x, gy, w, lineWidth, line);
    }

    // The device panel: navy body, a faint light along the top edge, the 2 dp cobalt bezel and a
    // hard 3 dp pixel drop shadow. No glow.
    void PKSEFramebuffer::pkPanel(int x, int y, int w, int h, Color fill, int radius)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        const float r = (float)radius;
        fillRoundedRaw(x + 3.0f, y + 3.0f, (float)w, (float)h, r, Color(0x14, 0x12, 0x1D, 0x88));
        fillRoundedRaw((float)x, (float)y, (float)w, (float)h, r, fill);
        const float lightH = std::min(26.0f, h / 3.0f);
        fillGradientRaw(x + 2.0f, y + 2.0f, w - 4.0f, lightH, std::max(1.0f, r - 2), lighterColor(fill, 0.05f), fill);
        strokeRoundedRaw(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, r, Pksm::PaperEdge, 2.0f);
    }

    // The resting menu button: void outline, navy body with a whisper of gradient, a 1.5 dp cobalt edge.
    void PKSEFramebuffer::pkBlackButton(int x, int y, int w, int h, int radius)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        const float r = (float)radius;
        fillRoundedRaw((float)x, (float)y, (float)w, (float)h, r, Pksm::LogoVoid);
        fillGradientRaw(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, std::max(0.0f, r - 1), lighterColor(Pksm::LogoDeck, 0.04f),
                        darkerColor(Pksm::LogoDeck, 0.05f));
        strokeRoundedRaw(x + 1.75f, y + 1.75f, w - 3.5f, h - 3.5f, std::max(0.0f, r - 1), Pksm::LogoGrid, 1.5f);
    }

    // The selected / active button: the accent body (cobalt by default) with a soft top light and the
    // thin pale focus rim. Never a cyan halo.
    void PKSEFramebuffer::pkSelectedButton(int x, int y, int w, int h, int radius, Color accent)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        const float r = (float)radius;
        fillRoundedRaw((float)x, (float)y, (float)w, (float)h, r, Pksm::LogoVoid);
        fillGradientRaw(x + 1.5f, y + 1.5f, w - 3.0f, h - 3.0f, std::max(0.0f, r - 1), lighterColor(accent, 0.18f),
                        darkerColor(accent, 0.08f));
        strokeRoundedRaw(x + 2.1f, y + 2.1f, w - 4.2f, h - 4.2f, std::max(0.0f, r - 1), Color(0xF4, 0xF8, 0xFF, 0x70), 1.2f);
    }

    // Header strip: accent body (cobalt by default), soft top light, dark outline, white caption.
    void PKSEFramebuffer::pkHeaderStrip(int x, int y, int w, int h, const std::string &label, Color accent, bool centred,
                                        TextStyle style)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        fillRoundedRaw((float)x, (float)y, (float)w, (float)h, 4.0f, Pksm::LogoVoid);
        fillGradientRaw(x + 1.5f, y + 1.5f, w - 3.0f, h - 3.0f, 3.0f, lighterColor(accent, 0.16f), darkerColor(accent, 0.1f));
        fillRoundedRaw(x + 2.5f, y + 2.5f, w - 5.0f, h / 2.0f - 2.5f, 2.0f, Color(255, 255, 255, 0x16));
        if (!label.empty())
        {
            int tw = 0, th = 0;
            measureText(label, tw, th, style);
            const int tx = centred ? x + (w - tw) / 2 : x + 10;
            const int ty = textYCenteredOn(y + h / 2, style);
            drawText(tx + 1, ty + 1, label, Color(0x14, 0x12, 0x1D, 0x90), style);
            drawText(tx, ty, label, Color(255, 255, 255), style);
        }
    }

    // List row inside a panel: soft stripe idle; the selected button look chosen.
    void PKSEFramebuffer::pkStripeRow(int x, int y, int w, int h, bool selected)
    {
        if (selected)
            pkSelectedButton(x, y, w, h, 4);
        else
            fillRoundedRaw((float)x, (float)y, (float)w, (float)h, 3.0f,
                           Color(Pksm::PaperShade.red, Pksm::PaperShade.green, Pksm::PaperShade.blue, 0x70));
    }

    void PKSEFramebuffer::pkWallpaper(int x, int y, int w, int h, Color base)
    {
        if (!ensureFrame())
            return;
        drawFilledRect(x, y, w, h, base);
        Color grid = Pksm::wallpaperShade(base);
        grid.alpha = 0x60;
        for (int gx = x + 18; gx < x + w; gx += 30)
            drawFilledRect(gx, y, 2, h, grid);
        for (int gy = y + 18; gy < y + h; gy += 30)
            drawFilledRect(x, gy, w, 2, grid);
    }

    void PKSEFramebuffer::pkEmptySlot(int x, int y, int w, int h, Color wallpaper)
    {
        if (!ensureFrame())
            return;
        Color shade = Pksm::wallpaperShade(wallpaper);
        shade.alpha = 0x66;
        const float cx = x + w / 2.0f, cy = y + h / 2.0f, rad = w * 0.16f;
        nvgStrokeColor(vg, toNVG(shade));
        nvgStrokeWidth(vg, 2.0f);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, rad);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, cy - rad);
        nvgLineTo(vg, cx, cy + rad);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, 3.0f);
        nvgStroke(vg);
    }

    // Box banner: a header strip with the name centred and two small menu-button caps carrying the
    // chevrons (dimmed when there is nowhere to go).
    void PKSEFramebuffer::pkBoxNameBar(int x, int y, int w, int h, const std::string &label, bool canPrev, bool canNext)
    {
        if (w <= 0 || h <= 0 || !ensureFrame())
            return;
        pkHeaderStrip(x, y, w, h, label, Pksm::HeaderBlue, true);
        const int inset = 2 + 2; // the strip's outline + the cap margin
        const int capW = static_cast<int>((h - 3) * 1.15f);
        auto cap = [&](int cx, bool left, bool enabled)
        {
            pkBlackButton(cx, y + inset, capW, h - inset * 2, 3);
            const float mx = cx + capW / 2.0f, my = y + h / 2.0f;
            const float arm = std::min(4.5f, (h - inset * 2) * 0.2f) * 1.6f;
            nvgBeginPath(vg);
            if (left)
            {
                nvgMoveTo(vg, mx + arm * 0.6f, my - arm);
                nvgLineTo(vg, mx - arm * 0.6f, my);
                nvgLineTo(vg, mx + arm * 0.6f, my + arm);
            }
            else
            {
                nvgMoveTo(vg, mx - arm * 0.6f, my - arm);
                nvgLineTo(vg, mx + arm * 0.6f, my);
                nvgLineTo(vg, mx - arm * 0.6f, my + arm);
            }
            Color ink = Pksm::Ink;
            if (!enabled)
                ink.alpha = 0x40;
            nvgStrokeColor(vg, toNVG(ink));
            nvgStrokeWidth(vg, 2.6f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
        };
        cap(x + inset, true, canPrev);
        cap(x + w - inset - capW, false, canNext);
    }

    // Selection on a slot: red corner brackets, the one grid selection (PksmPaint.Selection).
    void PKSEFramebuffer::pkSelection(int x, int y, int w, int h, Color color)
    {
        const int arm = std::min(14, static_cast<int>(std::min(w, h) * 0.4f));
        const int l = x - 2, t = y - 2, r = x + w + 2, b = y + h + 2, k = 4;
        drawFilledRect(l, t, arm, k, color);
        drawFilledRect(l, t, k, arm, color);
        drawFilledRect(r - arm, t, arm, k, color);
        drawFilledRect(r - k, t, k, arm, color);
        drawFilledRect(l, b - k, arm, k, color);
        drawFilledRect(l, b - arm, k, arm, color);
        drawFilledRect(r - arm, b - k, arm, k, color);
        drawFilledRect(r - k, b - arm, k, arm, color);
    }

    // Grab state: the slot ghost when carrying a mon (light-blue dashes).
    void PKSEFramebuffer::pkCarryGhost(int x, int y, int w, int h)
    {
        if (!ensureFrame())
            return;
        const float x0 = x + 1.5f, y0 = y + 1.5f, x1 = x + w - 1.5f, y1 = y + h - 1.5f;
        auto dashes = [&](float ax, float ay, float bx, float by)
        {
            const float len = std::hypot(bx - ax, by - ay);
            for (float d = 0; d < len; d += 11.0f)
            {
                const float e = std::min(len, d + 6.0f);
                nvgBeginPath(vg);
                nvgMoveTo(vg, ax + (bx - ax) * d / len, ay + (by - ay) * d / len);
                nvgLineTo(vg, ax + (bx - ax) * e / len, ay + (by - ay) * e / len);
                nvgStrokeColor(vg, toNVG(Pksm::SelectBorder));
                nvgStrokeWidth(vg, 3.0f);
                nvgStroke(vg);
            }
        };
        dashes(x0, y0, x1, y0);
        dashes(x1, y0, x1, y1);
        dashes(x1, y1, x0, y1);
        dashes(x0, y1, x0, y0);
    }

    void PKSEFramebuffer::pkBattery(int x, int y, int percent)
    {
        if (!ensureFrame())
            return;
        // 22x12 outline, 2 px InkSoft stroke, green fill inset (DsChrome.StatusStrip).
        strokeRoundedRaw(x + 1.0f, y + 1.0f, 20.0f, 10.0f, 2.0f, Pksm::InkSoft, 2.0f);
        fillRoundedRaw(x + 22.0f, y + 4.0f, 2.0f, 4.0f, 1.0f, Pksm::InkSoft);
        const float full = 16.0f;
        const float level = percent < 0 ? full : full * std::clamp(percent, 0, 100) / 100.0f;
        const Color fill = (percent >= 0 && percent <= 15) ? Pksm::Illegal : Pksm::Legal;
        if (level > 0)
            fillRoundedRaw(x + 3.0f, y + 3.0f, std::max(1.0f, level), 6.0f, 1.0f, fill);
    }

    // ---- Motion -------------------------------------------------------------------------------

    // Xamarin/MAUI's curves, which PKForge's numbers were tuned against.
    static float easeCubicOut(float t) { return (t - 1) * (t - 1) * (t - 1) + 1; }
    static float easeSpringOut(float t)
    {
        const float x = t - 1;
        return x * x * ((1.70158f + 1) * x + 1.70158f) + 1;
    }

    // Kit.AnimateIn: opacity 0 -> 1 over 130ms CubicOut, scale 0.92 -> 1 over 160ms SpringOut.
    static constexpr double POP_FADE_SECONDS = 0.130;
    static constexpr double POP_SCALE_SECONDS = 0.160;
    static constexpr float POP_START_SCALE = 0.92f;

    PKSEFramebuffer::MotionEntry &PKSEFramebuffer::motionFor(const char *id, bool &fresh)
    {
        auto [slot, inserted] = motion.try_emplace(id);
        MotionEntry &entry = slot->second;
        // Never seen, or absent last frame -> it has just appeared.
        fresh = inserted || entry.lastFrame + 1 < frameIndex;
        entry.lastFrame = frameIndex;
        return entry;
    }

    void PKSEFramebuffer::stepSpring(MotionEntry &entry, int axis, float target, float stiffness, float damping)
    {
        // Small sub-steps keep the integration stable through a dropped frame (Motion.cs Spring.Step).
        const float dt = deltaSeconds;
        const int steps = std::max(1, static_cast<int>(std::ceil(dt / (1.0f / 240.0f))));
        const float h = dt / steps;
        float &value = entry.value[axis];
        float &velocity = entry.velocity[axis];
        for (int i = 0; i < steps; ++i)
        {
            velocity += (stiffness * (target - value) - damping * velocity) * h;
            value += velocity * h;
        }
        if (std::fabs(target - value) < 0.01f && std::fabs(velocity) < 0.01f)
        {
            value = target;
            velocity = 0;
        }
    }

    void PKSEFramebuffer::glide(const char *id, float targetX, float targetY, float &outX, float &outY,
                                float stiffness, float damping)
    {
        bool fresh;
        MotionEntry &entry = motionFor(id, fresh);
        if (fresh)
        {
            entry.value[0] = targetX;
            entry.value[1] = targetY;
            entry.velocity[0] = entry.velocity[1] = 0;
            entry.steppedFrame = frameIndex;
        }
        else if (entry.steppedFrame != frameIndex)
        {
            entry.steppedFrame = frameIndex;
            stepSpring(entry, 0, targetX, stiffness, damping);
            stepSpring(entry, 1, targetY, stiffness, damping);
        }
        outX = entry.value[0];
        outY = entry.value[1];
    }

    float PKSEFramebuffer::spring(const char *id, float target, float stiffness, float damping, float initial)
    {
        bool fresh;
        MotionEntry &entry = motionFor(id, fresh);
        if (fresh)
        {
            entry.value[0] = initial;
            entry.velocity[0] = 0;
            entry.steppedFrame = 0;
        }
        if (entry.steppedFrame != frameIndex)
        {
            entry.steppedFrame = frameIndex;
            stepSpring(entry, 0, target, stiffness, damping);
        }
        return entry.value[0];
    }

    void PKSEFramebuffer::pushScale(float centerX, float centerY, float scale)
    {
        if (!ensureFrame())
            return;
        nvgSave(vg);
        nvgTranslate(vg, centerX, centerY);
        nvgScale(vg, scale, scale);
        nvgTranslate(vg, -centerX, -centerY);
    }

    void PKSEFramebuffer::popTransform()
    {
        if (vg && frameActive)
            nvgRestore(vg);
    }

    void PKSEFramebuffer::beginPopIn(const char *id)
    {
        if (!ensureFrame())
            return;
        bool fresh;
        MotionEntry &entry = motionFor(id, fresh);
        if (fresh)
        {
            entry.start = totalSeconds;
            entry.typedTotal = 0;
            entry.typedDoneAt = -1.0;
            entry.typingSkipped = false;
        }
        nvgSave(vg);
        // Finished pop-ins skip the transform entirely (applied = true), so a dialog that has been
        // up for a while draws exactly as it did before this existed.
        const bool done = totalSeconds - entry.start >= POP_SCALE_SECONDS;
        PopScope scope{entry.start, done, &entry};
        popStack.push_back(scope);
    }

    // DialogueChoiceBox: 24ms a character, after the window has mostly popped in.
    static constexpr double TYPE_SECONDS_PER_CHAR = 0.024;
    static constexpr double TYPE_START_DELAY = 0.06;
    static constexpr double TYPE_MAX_SECONDS = 1.0; // a long message speeds up rather than dragging
    static constexpr double CHOICES_FADE_SECONDS = 0.100;

    static PKSEFramebuffer *s_typingFramebuffer = nullptr;

    bool PKSEFramebuffer::typingInProgress() { return s_typingFramebuffer && s_typingFramebuffer->typingBusyLastFrame; }
    void PKSEFramebuffer::skipTyping()
    {
        if (s_typingFramebuffer)
            s_typingFramebuffer->skipTypingRequested = true;
    }

    void PKSEFramebuffer::beginTyping()
    {
        if (popStack.empty())
            return;
        s_typingFramebuffer = this;
        PopScope &scope = popStack.back();
        MotionEntry &entry = *scope.entry;
        if (skipTypingRequested)
            entry.typingSkipped = true;
        scope.typing = true;
        scope.typed = 0;
        if (entry.typingSkipped)
        {
            scope.budget = 1 << 30;
            return;
        }
        const double perChar =
            entry.typedTotal > 0 ? std::min(TYPE_SECONDS_PER_CHAR, TYPE_MAX_SECONDS / entry.typedTotal)
                                 : TYPE_SECONDS_PER_CHAR;
        const double t = totalSeconds - scope.start - TYPE_START_DELAY;
        scope.budget = t <= 0 ? 0 : static_cast<int>(t / perChar);
    }

    size_t PKSEFramebuffer::typedBytes(const std::string &text)
    {
        if (popStack.empty() || !popStack.back().typing)
            return text.size();
        PopScope &scope = popStack.back();
        // Count in code points, so an accented letter appears whole.
        size_t bytes = 0;
        while (bytes < text.size())
        {
            if (scope.typed >= scope.budget)
            {
                // Keep counting what is still due, so endTyping() knows the message is unfinished.
                for (size_t rest = bytes; rest < text.size(); ++rest)
                    if ((static_cast<unsigned char>(text[rest]) & 0xC0) != 0x80)
                        ++scope.typed;
                return bytes;
            }
            ++bytes;
            while (bytes < text.size() && (static_cast<unsigned char>(text[bytes]) & 0xC0) == 0x80)
                ++bytes;
            ++scope.typed;
        }
        return bytes;
    }

    void PKSEFramebuffer::endTyping()
    {
        if (popStack.empty() || !popStack.back().typing)
            return;
        PopScope &scope = popStack.back();
        MotionEntry &entry = *scope.entry;
        scope.typing = false;
        entry.typedTotal = scope.typed;
        float choices;
        if (scope.typed <= scope.budget)
        {
            if (entry.typedDoneAt < 0)
                entry.typedDoneAt = totalSeconds;
            choices = static_cast<float>(std::clamp((totalSeconds - entry.typedDoneAt) / CHOICES_FADE_SECONDS, 0.0, 1.0));
            choices = easeCubicOut(choices);
        }
        else
        {
            entry.typedDoneAt = -1.0;
            choices = 0.0f;
            typingBusyThisFrame = true;
        }
        if (vg && frameActive && choices < 1.0f)
            nvgGlobalAlpha(vg, scope.fade * choices);
    }

    void PKSEFramebuffer::endPopIn()
    {
        if (popStack.empty())
            return;
        endTyping(); // a message with no buttons after it still records that it finished
        popStack.pop_back();
        if (vg && frameActive)
            nvgRestore(vg);
    }

    void PKSEFramebuffer::notePopupRect(float x, float y, float rectWidth, float rectHeight)
    {
        if (popStack.empty() || popStack.back().applied)
            return;
        // The scrim (and a full-page backdrop) covers the screen; it is not the window.
        if (rectWidth >= width - 1 && rectHeight >= height - 1)
            return;
        PopScope &scope = popStack.back();
        scope.applied = true;
        const float t = static_cast<float>(totalSeconds - scope.start);
        const float fade = easeCubicOut(std::clamp(t / static_cast<float>(POP_FADE_SECONDS), 0.0f, 1.0f));
        const float grow = easeSpringOut(std::clamp(t / static_cast<float>(POP_SCALE_SECONDS), 0.0f, 1.0f));
        const float scale = POP_START_SCALE + (1.0f - POP_START_SCALE) * grow;
        const float centerX = x + rectWidth / 2, centerY = y + rectHeight / 2;
        nvgTranslate(vg, centerX, centerY);
        nvgScale(vg, scale, scale);
        nvgTranslate(vg, -centerX, -centerY);
        nvgGlobalAlpha(vg, fade);
        scope.fade = fade;
    }
}
