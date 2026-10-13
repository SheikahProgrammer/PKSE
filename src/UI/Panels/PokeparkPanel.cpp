#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "UI/Panels/PokeparkPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/ScreenChrome.h"
#include "UI/SpriteManager.h"
#include "UI/Pokepark.h"
#include "UI/Common.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"
#include "Utils/StringHelpers.h"

namespace UI
{
    namespace Panels
    {
        namespace
        {
            constexpr int PANEL_X = 32, PANEL_Y = 96, PANEL_W = 1216, PANEL_H = 588;

            float easeSpringOut(float t)
            {
                const float x = t - 1;
                return x * x * ((1.70158f + 1) * x + 1.70158f) + 1;
            }

            // Pixel icons (68x56) vs HD renders: only the HD ones squash and stretch.
            bool isPixelSprite(const Sprite *sprite) { return sprite && sprite->width <= 128 && sprite->height <= 128; }
        }

        // ---- Sky & scenery -------------------------------------------------------------------
        // Three layers: the sky and far silhouettes drift a little against the selected resident
        // (parallax), the middle layer less so, and the ground -- which the residents walk on and
        // the terrain map is traced from -- stays put.

        static void drawSky(PKSEFramebuffer &framebuffer, int x, int y, int w, int h, int hour, float farShift,
                            double time)
        {
            Color topColor, bottomColor;
            if (hour >= 6 && hour < 11)
            {
                topColor = Color(135, 175, 230);
                bottomColor = Color(245, 215, 175);
            }
            else if (hour >= 11 && hour < 18)
            {
                topColor = Color(85, 155, 235);
                bottomColor = Color(185, 225, 255);
            }
            else if (hour >= 18 && hour < 21)
            {
                topColor = Color(110, 65, 145);
                bottomColor = Color(245, 140, 95);
            }
            else
            {
                topColor = Color(18, 22, 55);
                bottomColor = Color(45, 50, 95);
            }
            framebuffer.drawVerticalGradient(x, y, w, h, topColor, bottomColor);

            const int bodyX = x + w - 180 + static_cast<int>(farShift * 0.5f), bodyY = y + 70;
            if (hour >= 6 && hour < 18)
            {
                framebuffer.drawFilledCircle(bodyX, bodyY, 34, Color(255, 245, 190, 70));
                framebuffer.drawFilledCircle(bodyX, bodyY, 25, Color(255, 250, 210, 170));
                framebuffer.drawFilledCircle(bodyX, bodyY, 18, Color(255, 255, 240));
            }
            else
            {
                framebuffer.drawFilledCircle(bodyX, bodyY, 20, Color(240, 245, 255, 220));
                framebuffer.drawFilledCircle(bodyX - 6, bodyY - 4, 18, topColor);
                static const int stars[][3] = {{120, 45, 2}, {280, 80, 2}, {450, 35, 3}, {620, 90, 2}, {820, 50, 3},
                                               {980, 30, 2}, {200, 120, 1}, {700, 140, 1}, {1100, 110, 2}};
                for (int i = 0; i < 9; ++i)
                {
                    // Twinkle.
                    const int a = 140 + static_cast<int>(std::sin(time * 2.0 + i * 1.7) * 80.0);
                    framebuffer.drawFilledCircle(x + stars[i][0] + static_cast<int>(farShift * 0.3f), y + stars[i][1],
                                                 stars[i][2], Color(255, 255, 255, static_cast<uint8_t>(std::clamp(a, 40, 255))));
                }
            }
        }

        static void drawFarLayer(PKSEFramebuffer &fb, int x, int y, int w, int h, ParkEnvironmentId env, float shift)
        {
            const int s = static_cast<int>(shift);
            switch (env)
            {
            case ParkEnvironmentId::Commons:
                fb.drawFilledEllipse(x + 200 + s, y + 250, 320, 90, Color(120, 190, 140, 200));
                fb.drawFilledEllipse(x + 760 + s, y + 240, 380, 100, Color(110, 180, 135, 200));
                fb.drawFilledEllipse(x + 1150 + s, y + 255, 260, 80, Color(125, 195, 145, 200));
                break;
            case ParkEnvironmentId::WaterfallLake:
                fb.drawFilledEllipse(x + 520 + s, y + 270, 300, 140, Color(105, 135, 165, 200));
                fb.drawFilledEllipse(x + 940 + s, y + 280, 320, 120, Color(95, 125, 160, 200));
                fb.drawFilledEllipse(x + 940 + s, y + 175, 60, 22, Color(240, 245, 255, 200)); // snowcap
                break;
            case ParkEnvironmentId::EmberGrove:
                fb.drawFilledEllipse(x + 820 + s, y + 290, 260, 150, Color(110, 70, 60, 210));
                fb.drawFilledEllipse(x + 820 + s, y + 150, 40, 14, Color(255, 150, 70, 160)); // a far, friendly glow
                fb.drawFilledEllipse(x + 300 + s, y + 300, 300, 120, Color(120, 80, 65, 210));
                break;
            case ParkEnvironmentId::SkySummit:
                fb.drawFilledEllipse(x + 260 + s, y + 300, 220, 170, Color(150, 160, 185, 200));
                fb.drawFilledEllipse(x + 700 + s, y + 320, 260, 200, Color(140, 150, 178, 200));
                fb.drawFilledEllipse(x + 700 + s, y + 140, 70, 24, Color(250, 252, 255, 220));
                fb.drawFilledEllipse(x + 1080 + s, y + 300, 220, 160, Color(150, 160, 185, 200));
                break;
            }
            (void)w;
            (void)h;
        }

        static void drawMidLayer(PKSEFramebuffer &fb, int x, int y, int w, int h, ParkEnvironmentId env, float shift,
                                 double time)
        {
            const int s = static_cast<int>(shift);
            switch (env)
            {
            case ParkEnvironmentId::Commons:
                // A tree line along the back.
                for (int i = 0; i < 7; ++i)
                {
                    const int tx = x + 60 + i * 170 + s;
                    fb.drawFilledCircle(tx, y + 205, 42, Color(70, 140, 80));
                    fb.drawFilledCircle(tx + 30, y + 215, 34, Color(80, 150, 85));
                }
                break;
            case ParkEnvironmentId::WaterfallLake:
                for (int i = 0; i < 10; ++i)
                    fb.drawFilledRect(x + 220 + i * 95 + s, y + 380 - (i % 3) * 6, 4, 26 + (i % 3) * 6, Color(70, 125, 70));
                break;
            case ParkEnvironmentId::EmberGrove:
                fb.drawFilledCircle(x + 60 + s, y + 90, 90, Color(215, 95, 35));
                fb.drawFilledCircle(x + w - 80 + s, y + 80, 110, Color(195, 75, 30));
                break;
            case ParkEnvironmentId::SkySummit:
            {
                // The cloud sea, drifting.
                const int drift = static_cast<int>(std::fmod(time * 8.0, 400.0));
                fb.drawFilledEllipse(x + 180 + s + drift % 200, y + h - 170, 240, 50, Color(255, 255, 255, 190));
                fb.drawFilledEllipse(x + 520 + s - drift % 160, y + h - 180, 280, 60, Color(255, 255, 255, 210));
                fb.drawFilledEllipse(x + 920 + s + drift % 120, y + h - 175, 310, 55, Color(255, 255, 255, 190));
                break;
            }
            }
        }

        static void drawGround(PKSEFramebuffer &fb, int x, int y, int w, int h, ParkEnvironmentId env, double time,
                               float berryRipeness)
        {
            switch (env)
            {
            case ParkEnvironmentId::Commons:
            {
                fb.drawFilledEllipse(x + 250, y + h + 80, 500, 260, Color(85, 175, 80));
                fb.drawFilledEllipse(x + w - 280, y + h + 60, 480, 240, Color(75, 165, 70));
                fb.drawFilledRect(x, y + h - 180, w, 180, Color(65, 155, 60));
                fb.drawFilledRect(x, y + 230, w, h - 410, Color(95, 180, 85));
                fb.drawFilledEllipse(x + w / 2, y + h + 20, 280, 110, Color(215, 195, 150));
                static const int flowers[][3] = {{160, 498, 0}, {320, 468, 1}, {890, 513, 0}, {1040, 478, 2},
                                                 {420, 420, 1}, {680, 300, 0}, {240, 330, 2}, {840, 360, 1}};
                for (const auto &f : flowers)
                {
                    const Color petal = f[2] == 0 ? Color(255, 255, 255) : f[2] == 1 ? Color(255, 220, 80) : Color(255, 180, 200);
                    const int sway = static_cast<int>(std::sin(time * 1.5 + f[0] * 0.01) * 1.5);
                    fb.drawFilledCircle(x + f[0] + sway, y + f[1], 4, petal);
                    fb.drawFilledCircle(x + f[0] + sway, y + f[1], 2, Color(255, 215, 0));
                }
                // The berry tree: trunk, canopy and berries that fill in as the next one ripens.
                const int tx = x + 1010, ty = y + 268;
                fb.drawFilledEllipse(tx, ty + 6, 34, 10, Color(0, 0, 0, 50));
                fb.drawFilledRoundedRect(tx - 12, ty - 70, 24, 76, 6, Color(120, 85, 55));
                fb.drawFilledCircle(tx - 34, ty - 92, 42, Color(55, 135, 65));
                fb.drawFilledCircle(tx + 30, ty - 98, 46, Color(60, 145, 70));
                fb.drawFilledCircle(tx, ty - 124, 44, Color(70, 155, 75));
                const int berries = std::clamp(static_cast<int>((1.0f - berryRipeness / 70.0f) * 7.0f) + 1, 1, 7);
                static const int spots[7][2] = {{-40, -96}, {22, -110}, {-6, -132}, {40, -84}, {-20, -70}, {12, -82}, {-52, -118}};
                for (int i = 0; i < berries; ++i)
                    fb.drawFilledCircle(tx + spots[i][0], ty + spots[i][1], 5, Color(230, 60, 90));
                break;
            }
            case ParkEnvironmentId::WaterfallLake:
            {
                fb.drawFilledRect(x, y + h - 220, w, 220, Color(75, 155, 75));
                fb.drawFilledRect(x, y + 230, w, h - 450, Color(95, 170, 90));
                fb.drawFilledRoundedRect(x - 40, y + 40, 180, h - 120, 24, Color(100, 110, 120));
                fb.drawFilledRect(x + 40, y + 90, 48, h - 170, Color(135, 205, 250, 220));
                // Falling water: bright streaks running down.
                for (int i = 0; i < 4; ++i)
                {
                    const int sy = static_cast<int>(std::fmod(time * 220.0 + i * 90.0, static_cast<double>(h - 170)));
                    fb.drawFilledRect(x + 48 + i * 10, y + 90 + sy, 4, 40, Color(240, 250, 255, 200));
                }
                fb.drawFilledEllipse(x + 580, y + h - 70, 480, 120, Color(65, 140, 215));
                fb.drawFilledEllipse(x + 580, y + h - 70, 420, 95, Color(85, 165, 235));
                for (int i = 0; i < 4; ++i)
                {
                    const float phase = static_cast<float>(time * 0.6 + i * 0.8);
                    const int rx = 60 + static_cast<int>(std::fmod(phase, 1.0f) * 40);
                    fb.drawFilledEllipse(x + 300 + i * 190, y + h - 70 + (i % 2) * 20, rx, 6,
                                         Color(255, 255, 255, static_cast<uint8_t>(130 * (1.0f - std::fmod(phase, 1.0f)))));
                }
                fb.drawFilledEllipse(x + 64, y + h - 80, 60, 20, Color(255, 255, 255, 180));
                break;
            }
            case ParkEnvironmentId::EmberGrove:
            {
                fb.drawFilledRect(x, y + h - 200, w, 200, Color(125, 80, 55));
                fb.drawFilledRect(x, y + 230, w, h - 430, Color(140, 95, 65));
                fb.drawFilledEllipse(x + 300, y + h - 90, 350, 120, Color(160, 95, 60));
                // The cinder ring between the stones, glowing softly.
                const float glow = 0.6f + 0.4f * static_cast<float>(std::sin(time * 2.2));
                fb.drawFilledEllipse(x + 618, y + 470, 70, 22, Color(255, 140, 50, static_cast<uint8_t>(60 * glow)));
                for (int i = 0; i < 10; ++i)
                {
                    const float a = i * 0.628f;
                    fb.drawFilledCircle(x + 618 + static_cast<int>(std::cos(a) * 64), y + 470 + static_cast<int>(std::sin(a) * 20), 7,
                                        Color(70, 62, 66));
                }
                fb.drawFilledCircle(x + 488, y + 488, 36, Color(55, 50, 55));
                fb.drawFilledCircle(x + 488, y + 488, 28, Color(245, 130, 45, static_cast<uint8_t>(150 + 40 * glow)));
                fb.drawFilledCircle(x + 748, y + 503, 42, Color(65, 60, 65));
                fb.drawFilledCircle(x + 748, y + 503, 32, Color(255, 160, 60, static_cast<uint8_t>(130 + 40 * glow)));
                break;
            }
            case ParkEnvironmentId::SkySummit:
            {
                fb.drawFilledRect(x, y + h - 190, w, 190, Color(145, 155, 165));
                fb.drawFilledEllipse(x + w / 2, y + h - 50, 580, 120, Color(165, 175, 185));
                fb.drawFilledEllipse(x + 220, y + h - 110, 110, 24, Color(245, 250, 255));
                fb.drawFilledEllipse(x + 890, y + h - 95, 130, 28, Color(245, 250, 255));
                // The lookout: a tall rock the flyers like to perch on.
                const int lx = x + 930, ly = y + 392;
                fb.drawFilledEllipse(lx, ly + 4, 52, 12, Color(0, 0, 0, 50));
                fb.drawFilledRoundedRect(lx - 40, ly - 50, 80, 56, 14, Color(120, 125, 135));
                fb.drawFilledRoundedRect(lx - 28, ly - 64, 56, 24, 10, Color(135, 140, 150));
                fb.drawFilledEllipse(lx, ly - 62, 26, 6, Color(245, 250, 255));
                break;
            }
            }
        }

        static void drawWeatherTint(PKSEFramebuffer &fb, int x, int y, int w, int h, ParkWeather weather, bool night)
        {
            if (night)
                fb.drawFilledRect(x, y, w, h, Color(10, 15, 45, 80));
            switch (weather)
            {
            case ParkWeather::Rain: fb.drawFilledRect(x, y, w, h, Color(30, 45, 80, 60)); break;
            case ParkWeather::Fog: fb.drawFilledRect(x, y, w, h, Color(230, 235, 240, 55)); break;
            case ParkWeather::Heat: fb.drawFilledRect(x, y, w, h, Color(255, 150, 60, 28)); break;
            case ParkWeather::Snow: fb.drawFilledRect(x, y, w, h, Color(220, 230, 255, 30)); break;
            default: break;
            }
        }

        static void drawParticles(PKSEFramebuffer &fb, int x, int y, const std::vector<ParkParticle> &particles)
        {
            for (const auto &p : particles)
            {
                const float t = p.life / p.maxLife;
                const int px = x + static_cast<int>(p.x), py = y + static_cast<int>(p.y);
                const int size = std::max(1, static_cast<int>(p.size));
                switch (p.kind)
                {
                case ParkParticleKind::Dust:
                    fb.drawFilledCircle(px, py, size, Color(230, 215, 180, static_cast<uint8_t>(150 * (1 - t))));
                    break;
                case ParkParticleKind::Splash:
                    fb.drawFilledCircle(px, py, size, Color(220, 240, 255, static_cast<uint8_t>(220 * (1 - t))));
                    break;
                case ParkParticleKind::Ember:
                    fb.drawFilledCircle(px, py, size, Color(255, 170 - static_cast<int>(80 * t), 60, static_cast<uint8_t>(230 * (1 - t))));
                    break;
                case ParkParticleKind::Snow:
                    fb.drawFilledCircle(px, py, size, Color(255, 255, 255, static_cast<uint8_t>(220 * (1 - t * t))));
                    break;
                case ParkParticleKind::Leaf:
                    fb.drawFilledEllipse(px, py, size, std::max(1, size / 2), Color(120, 170, 60, 220));
                    break;
                case ParkParticleKind::Rain:
                    fb.drawFilledRect(px, py, 2, 12, Color(200, 220, 255, 150));
                    break;
                case ParkParticleKind::Firefly:
                {
                    const int a = static_cast<int>(200 * std::sin(t * 3.14159f) * (0.6f + 0.4f * std::sin(p.life * 8.0f)));
                    fb.drawFilledCircle(px, py, 6, Color(220, 255, 120, static_cast<uint8_t>(std::clamp(a / 4, 0, 255))));
                    fb.drawFilledCircle(px, py, 2, Color(240, 255, 170, static_cast<uint8_t>(std::clamp(a, 0, 255))));
                    break;
                }
                case ParkParticleKind::Mist:
                    fb.drawFilledEllipse(px, py, size, size / 2, Color(255, 255, 255, static_cast<uint8_t>(60 * std::sin(t * 3.14159f))));
                    break;
                case ParkParticleKind::Sparkle:
                    fb.drawShinyMark(px - size, py - size, size * 2, Color(255, 240, 150, static_cast<uint8_t>(255 * (1 - t))));
                    break;
                case ParkParticleKind::Heart:
                    fb.drawSymbol(px, py, "\xE2\x99\xA5", Color(255, 90, 130, static_cast<uint8_t>(255 * (1 - t))), TextStyle::Caption);
                    break;
                }
            }
        }

        static void drawEmotionBubble(PKSEFramebuffer &framebuffer, int cx, int cy, ParkEmotion emotion, float age)
        {
            if (emotion == ParkEmotion::None)
                return;
            // Pops in on the UI's spring curve.
            const float pop = 0.6f + 0.4f * easeSpringOut(std::min(1.0f, age / 0.18f));
            framebuffer.pushScale(static_cast<float>(cx), static_cast<float>(cy), pop);
            constexpr int bw = 32, bh = 26;
            framebuffer.drawFilledRoundedRect(cx - bw / 2, cy - bh, bw, bh, 8, Color(250, 250, 255));
            framebuffer.drawRoundedRect(cx - bw / 2, cy - bh, bw, bh, 8, Color(40, 45, 70), 2);
            framebuffer.drawFilledCircle(cx, cy + 1, 4, Color(250, 250, 255));
            switch (emotion)
            {
            case ParkEmotion::Heart:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x99\xA5", Color(255, 75, 115), TextStyle::Caption);
                break;
            case ParkEmotion::Music:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x99\xAA", Color(45, 175, 135), TextStyle::Caption);
                break;
            case ParkEmotion::Sleep:
                framebuffer.drawText(cx - 11, cy - bh + 5, "Zzz", Color(125, 105, 205), TextStyle::Caption);
                break;
            case ParkEmotion::Angry:
                framebuffer.drawSymbol(cx - 7, cy - bh + 4, "\xE2\x9C\x96", Color(235, 65, 55), TextStyle::Caption);
                break;
            case ParkEmotion::Exclamation:
                framebuffer.drawText(cx - 3, cy - bh + 2, "!", Color(240, 165, 30), TextStyle::Heading);
                break;
            case ParkEmotion::Question:
                framebuffer.drawText(cx - 5, cy - bh + 2, "?", Color(65, 145, 235), TextStyle::Heading);
                break;
            case ParkEmotion::Sweat:
                framebuffer.drawFilledEllipse(cx, cy - bh / 2, 5, 8, Color(110, 180, 245));
                break;
            case ParkEmotion::Sparkle:
                framebuffer.drawShinyMark(cx - 9, cy - bh + 4, 18, Color(240, 190, 50));
                break;
            default:
                break;
            }
            framebuffer.popTransform();
        }

        // ---- One creature in the scene ---------------------------------------------------------

        struct Actor
        {
            float y;    // depth sort key (feet)
            int index;  // resident index, or -1 for the wild visitor
            int visibleIndex;
        };

        static float depthScale(float sceneY)
        {
            const float t = (sceneY - PokeparkManager::PLAY_MIN_Y) / (PokeparkManager::PLAY_MAX_Y - PokeparkManager::PLAY_MIN_Y);
            return 0.82f + 0.30f * std::clamp(t, 0.0f, 1.0f);
        }

        static void drawCreature(PKSEFramebuffer &fb, ParkEnvironmentId env, uint16_t species, uint8_t form, bool shiny,
                                 float sceneX, float sceneY, bool facingLeft, float hop, ParkActivity activity,
                                 float animClock, bool moving, int clipTop, int clipH)
        {
            Sprite *sp = SpriteManager::getIconSprite(species, form, shiny);
            if (!sp || !sp->data)
                return;
            const ParkTerrain ground = PokeparkManager::terrainAt(env, sceneX, sceneY);
            const bool inAir = ground == ParkTerrain::Air;
            const bool inWater = ground == ParkTerrain::Water && activity != ParkActivity::Perch;
            const float depth = depthScale(sceneY);
            const int size = static_cast<int>(64 * depth);
            const int rx = PANEL_X + static_cast<int>(sceneX), feet = PANEL_Y + static_cast<int>(sceneY);
            const bool pixel = isPixelSprite(sp);

            // Bob: a hop when hopping, a gentle float in the air, a slow breath at rest.
            float lift = hop * (14.0f * depth);
            if (inAir)
                lift = 6.0f + std::sin(animClock * 2.4f) * 4.0f;
            else if (!moving && hop <= 0.0f)
                lift = std::sin(animClock * 2.5f) * 1.2f;
            if (pixel)
                lift = std::round(lift); // whole pixels only

            // Shadow: on the ground below; for a flyer over the summit, on the plateau's edge.
            const int shadowY = inAir ? PANEL_Y + 412 : feet;
            const float shadowShrink = inAir ? 0.6f : 1.0f - hop * 0.25f;
            if (!inWater)
                fb.drawFilledEllipse(rx, shadowY, static_cast<int>(22 * depth * shadowShrink), std::max(3, static_cast<int>(7 * depth * shadowShrink)),
                                     Color(0, 0, 0, inAir ? 45 : 70));
            else
            {
                const float ring = std::fmod(animClock * 0.8f, 1.0f);
                fb.drawFilledEllipse(rx, feet - 4, static_cast<int>((20 + ring * 14) * depth), static_cast<int>(5 * depth),
                                     Color(255, 255, 255, static_cast<uint8_t>(120 * (1 - ring))));
            }

            // Poses: squash a little when sitting or asleep (HD only -- pixel art stays square).
            float sx = 1.0f, sy = 1.0f, sink = 0.0f;
            if (!pixel)
            {
                if (activity == ParkActivity::Nap)
                {
                    sx = 1.08f;
                    sy = 0.88f + 0.02f * std::sin(animClock * 1.6f);
                }
                else if (activity == ParkActivity::Sit || activity == ParkActivity::Sulk)
                {
                    sx = 1.04f;
                    sy = 0.94f;
                }
                else if (hop > 0.05f)
                {
                    sy = 1.0f + 0.08f * hop;
                    sx = 1.0f - 0.05f * hop;
                }
            }
            if (activity == ParkActivity::Nap)
                sink = 4.0f * depth;
            if (inWater)
                sink = 16.0f * depth; // swimming: only the top half shows

            const int top = feet - size - static_cast<int>(lift) + static_cast<int>(sink);
            // Box icons face left; mirror when walking right.
            const float mirror = facingLeft ? 1.0f : -1.0f;
            if (inWater)
                fb.setClipRect(PANEL_X + 2, clipTop, PANEL_W - 4, std::max(0, feet - 4 - clipTop));
            fb.pushTransform(static_cast<float>(rx), static_cast<float>(feet), mirror * sx, sy);
            fb.drawSpriteFit(rx - size / 2, top, size, size, sp->width, sp->height, sp->data, sp->channels);
            fb.popTransform();
            if (inWater)
                fb.setClipRect(PANEL_X + 2, clipTop, PANEL_W - 4, clipH);

            // Asleep: little z's drifting up.
            if (activity == ParkActivity::Nap)
            {
                for (int i = 0; i < 2; ++i)
                {
                    const float t = std::fmod(animClock * 0.5f + i * 0.5f, 1.0f);
                    fb.drawText(rx + 14 + static_cast<int>(t * 12), top - static_cast<int>(t * 28), "z",
                                Color(255, 255, 255, static_cast<uint8_t>(220 * (1 - t))), TextStyle::Caption);
                }
            }
        }

        // ---- Toys ------------------------------------------------------------------------------

        static void drawToys(PKSEFramebuffer &fb, const std::vector<ParkToy> &toys, int clipTop, int clipH)
        {
            static const Color berryColors[5] = {Color(225, 60, 60), Color(120, 90, 200), Color(250, 150, 190),
                                                 Color(70, 170, 220), Color(240, 210, 70)};
            for (const auto &toy : toys)
            {
                if (toy.gone)
                    continue;
                const float f = std::min(1.0f, toy.flight);
                const float gx = toy.fromX + (toy.x - toy.fromX) * f, gy = toy.fromY + (toy.y - toy.fromY) * f;
                const float arc = std::sin(f * 3.14159f) * (toy.kind == ParkToyKind::Berry && toy.fromY < toy.y - 60 ? 40.0f : 140.0f);
                const int px = PANEL_X + static_cast<int>(gx), py = PANEL_Y + static_cast<int>(gy);
                const float depth = depthScale(gy);
                fb.drawFilledEllipse(px, py, static_cast<int>(9 * depth), 3, Color(0, 0, 0, 60));
                const int cy = py - static_cast<int>(arc) - static_cast<int>(8 * depth);
                const int r = static_cast<int>(8 * depth);
                if (toy.kind == ParkToyKind::Ball)
                {
                    // A Poke Ball: white body, red top half, black band and button.
                    fb.drawFilledCircle(px, cy, r, Color(250, 250, 250));
                    fb.setClipRect(px - r - 1, std::max(clipTop, cy - r - 1), r * 2 + 2, std::max(0, cy - std::max(clipTop, cy - r - 1)));
                    fb.drawFilledCircle(px, cy, r, Color(235, 60, 60));
                    fb.setClipRect(PANEL_X + 2, clipTop, PANEL_W - 4, clipH);
                    fb.drawFilledRect(px - r, cy - 1, r * 2, 2, Color(40, 40, 50));
                    fb.drawCircle(px, cy, r, Color(40, 40, 50), 2);
                    fb.drawFilledCircle(px, cy, std::max(2, r / 3), Color(40, 40, 50));
                    fb.drawFilledCircle(px, cy, std::max(1, r / 5), Color(250, 250, 250));
                }
                else
                {
                    fb.drawFilledCircle(px, cy, r, berryColors[static_cast<int>(toy.flavor) % 5]);
                    fb.drawFilledEllipse(px + 2, cy - r, 4, 2, Color(80, 160, 70));
                }
            }
        }

        // ---- Overlays --------------------------------------------------------------------------

        static void drawGauge(PKSEFramebuffer &fb, int x, int y, int w, const char *label, float value, Color color)
        {
            fb.drawText(x, y, label, Colors::TextDim, TextStyle::Caption);
            const int barX = x + 90, barW = w - 90;
            fb.drawGaugeBar(barX, y + 4, barW, 10, std::clamp(value / 100.0f, 0.0f, 1.0f), color, Colors::PanelAlt);
        }

        static void drawProfileCard(PokeparkView &screen, PKSEFramebuffer &fb, PokeparkManager &mgr, int residentIndex)
        {
            const auto &res = mgr.getResidents()[residentIndex];
            constexpr int cardW = 820, cardH = 500;
            const int cardX = PANEL_X + (PANEL_W - cardW) / 2, cardY = PANEL_Y + (PANEL_H - cardH) / 2;
            fb.drawFilledRect(0, 0, fb.getWidth(), fb.getHeight(), Color(0x14, 0x12, 0x1D, 0x99));
            fb.drawSoftShadow(cardX, cardY, cardW, cardH, 16);
            fb.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 16, Colors::Panel);
            fb.drawCardBand(cardX, cardY, cardW, 46);
            fb.drawText(cardX + 24, cardY + 10, res.name + "  ·  Resident", Colors::Text, TextStyle::Heading);

            // Bond with you: five hearts.
            for (int i = 0; i < 5; ++i)
            {
                const bool full = res.bond >= (i + 1) * 20, half = !full && res.bond >= i * 20 + 10;
                fb.drawSymbol(cardX + cardW - 150 + i * 26, cardY + 12, "\xE2\x99\xA5",
                              full ? Color(255, 90, 130) : half ? Color(255, 170, 190) : Color(120, 120, 140), TextStyle::Body);
            }

            // Big render.
            constexpr int pSize = 160;
            const int pX = cardX + 26, pY = cardY + 66;
            fb.drawFilledRoundedRect(pX, pY, pSize, pSize, 12, Colors::PanelAlt);
            if (Sprite *big = SpriteManager::getSprite(res.species, res.form, res.shiny); big && big->data)
                fb.drawSpriteFit(pX + 8, pY + 8, pSize - 16, pSize - 16, big->width, big->height, big->data, big->channels);

            // Needs.
            const int gx = pX, gy = pY + pSize + 16, gw = pSize + 20;
            drawGauge(fb, gx, gy, gw, "Energy", res.energy, Color(90, 200, 120));
            drawGauge(fb, gx, gy + 24, gw, "Company", 100.0f - res.social, Color(90, 160, 240));
            drawGauge(fb, gx, gy + 48, gw, "Fun", 100.0f - res.play, Color(240, 170, 60));
            drawGauge(fb, gx, gy + 72, gw, "Wonder", 100.0f - res.curiosity, Color(190, 120, 230));

            // Facts.
            const int infoX = cardX + 236, valueX = infoX + 132, valueW = cardX + cardW - 24 - valueX;
            int rowY = cardY + 64;
            auto row = [&](const char *label, const std::string &value) {
                fb.drawText(infoX, rowY, label, Colors::TextDim, TextStyle::Body);
                const auto lines = wrapTextToWidth(fb, value, valueW, TextStyle::Body);
                for (size_t i = 0; i < lines.size() && i < 2; ++i)
                    fb.drawText(valueX, rowY + static_cast<int>(i) * 24, lines[i], Colors::Text, TextStyle::Body);
                rowY += 30 + (lines.size() > 1 ? 24 : 0);
            };
            row("Personality", PokeparkManager::getPersonalityName(res.personality));
            row("Trait", PokeparkManager::getPersonalityDescription(res.personality));
            row("Mood", PokeparkManager::getMoodName(res.mood));
            row("Doing", res.currentActivity);
            const std::string bonds = mgr.bondSummary(residentIndex);
            row("Bonds", bonds.empty() ? std::string("Still getting to know everyone") : bonds);
            // The game it came from, then where in it. A resident from before the park was shared
            // knows only its slot until that game is opened once (adoptLegacyResidents).
            row("From", res.sourceInfo.find("  ·  ") != std::string::npos
                            ? res.sourceInfo
                            : res.sourceInfo + "  (open its game once to name it)");

            // Berry choice for Snack (Up/Down).
            static const char *berryNames[5] = {"Cheri (spicy)", "Chesto (dry)", "Pecha (sweet)", "Rawst (bitter)", "Aspear (sour)"};
            const std::string berry = std::string("Berry:  ") + berryNames[std::clamp(screen.pokeparkBerryFlavor, 0, 4)] + "   (Up/Down)";
            fb.drawText(infoX, cardY + cardH - 110, berry, Colors::TextDim, TextStyle::Body);

            // Actions.
            constexpr int btnW = 140, btnH = 44, btnGap = 12;
            const int btnStartX = cardX + (cardW - (btnW * 5 + btnGap * 4)) / 2, btnY = cardY + cardH - 62;
            static const char *actions[5] = {"Talk", "Play", "Snack", "Relax", "Call"};
            static const char *glyphs[5] = {"A", "X", "Y", "R", "ZL"};
            for (int b = 0; b < 5; ++b)
            {
                const int bx = btnStartX + b * (btnW + btnGap);
                const bool selected = screen.pokeparkCardActionSelected == b;
                drawGlyphButton(fb, bx, btnY, btnW, btnH, glyphs[b], actions[b],
                                selected ? Colors::Primary : Colors::PanelAlt, selected ? Colors::PrimaryText : Colors::Text,
                                selected);
                screen.touchButtons.push_back({300 + b, bx, btnY, btnW, btnH});
            }
        }

        static std::string dayLabel(uint64_t timestamp)
        {
            const std::time_t now = std::time(nullptr), then = static_cast<std::time_t>(timestamp);
            std::tm a = *std::localtime(&now);
            std::tm b = *std::localtime(&then);
            if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday)
                return "Today";
            const std::time_t yesterday = now - 86400;
            std::tm y = *std::localtime(&yesterday);
            if (y.tm_year == b.tm_year && y.tm_yday == b.tm_yday)
                return "Yesterday";
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%A %d %B", &b);
            return buffer;
        }

        static Color kindColor(ParkJournalEventKind kind)
        {
            switch (kind)
            {
            case ParkJournalEventKind::Friendship: return Color(45, 175, 125);
            case ParkJournalEventKind::Affection: return Color(245, 75, 125);
            case ParkJournalEventKind::Discovery: return Color(235, 165, 45);
            case ParkJournalEventKind::Conflict: return Color(225, 80, 60);
            case ParkJournalEventKind::Reconciliation: return Color(110, 190, 230);
            case ParkJournalEventKind::Bond: return Color(250, 200, 60);
            case ParkJournalEventKind::Visitor: return Color(120, 200, 90);
            case ParkJournalEventKind::Weather: return Color(150, 170, 210);
            case ParkJournalEventKind::Story: return Color(200, 130, 240);
            case ParkJournalEventKind::Photo: return Color(240, 240, 240);
            default: return Colors::Primary;
            }
        }

        static void drawJournal(PokeparkView &screen, PKSEFramebuffer &fb, PokeparkManager &mgr)
        {
            constexpr int jW = 900, jH = 520;
            const int jX = PANEL_X + (PANEL_W - jW) / 2, jY = PANEL_Y + (PANEL_H - jH) / 2;
            fb.drawFilledRect(0, 0, fb.getWidth(), fb.getHeight(), Color(0x14, 0x12, 0x1D, 0x99));
            fb.drawSoftShadow(jX, jY, jW, jH, 16);
            fb.drawFilledRoundedRect(jX, jY, jW, jH, 16, Colors::Panel);
            fb.drawCardBand(jX, jY, jW, 46);
            fb.drawText(jX + 24, jY + 10, "Poképark Journal", Colors::Text, TextStyle::Heading);

            const auto &journal = mgr.getJournal();
            if (journal.empty())
            {
                fb.drawText(jX + 40, jY + 140, "No entries yet. Leave your Pokémon to explore and play together!",
                            Colors::TextDim, TextStyle::Body);
                fb.drawText(jX + 26, jY + jH - 32, "B: Close", Colors::TextDim, TextStyle::Caption);
                return;
            }

            // Newest first, a header at each new day. The scroll counts entries.
            const int count = static_cast<int>(journal.size());
            screen.pokeparkJournalScroll = std::clamp(screen.pokeparkJournalScroll, 0, std::max(0, count - 1));
            const int textX = jX + 150, textW = jX + jW - 30 - textX;
            // Entries vary in height, so the ease works in an average row (the slide reads the same).
            int y = jY + 58 + static_cast<int>(std::lround(easedListShift(fb, "park.journal", screen.pokeparkJournalScroll, 62)));
            const int bottom = jY + jH - 44;
            std::string lastDay;
            fb.setClipRect(jX, jY + 50, jW, bottom - jY - 50);
            for (int n = screen.pokeparkJournalScroll; n < count && y < bottom; ++n)
            {
                const auto &entry = journal[count - 1 - n];
                const std::string day = dayLabel(entry.timestamp);
                if (day != lastDay)
                {
                    fb.drawText(jX + 26, y + 2, day, Colors::Accent, TextStyle::Body);
                    fb.drawHDivider(jX + 26, y + 30, jW - 52);
                    y += 38;
                    lastDay = day;
                }
                const auto lines = wrapTextToWidth(fb, entry.text, textW, TextStyle::Body);
                const int shown = std::min<int>(2, static_cast<int>(lines.size()));
                const int rowH = std::max(54, 16 + shown * 24);
                fb.drawFilledRoundedRect(jX + 20, y, jW - 40, rowH - 6, 10, Colors::PanelAlt);
                fb.drawText(jX + 32, y + (rowH - 6) / 2 - 10, entry.timeStr, Colors::TextDim, TextStyle::Caption);
                fb.drawFilledCircle(jX + 86, y + (rowH - 6) / 2, 6, kindColor(entry.kind));
                // Who it was about.
                int iconX = jX + 98;
                for (uint16_t species : {entry.species1, entry.species2})
                {
                    if (species == 0)
                        continue;
                    if (Sprite *icon = SpriteManager::getIconSprite(species, 0, false); icon && icon->data)
                        fb.drawSpriteFit(iconX, y + (rowH - 6) / 2 - 16, 34, 32, icon->width, icon->height, icon->data, icon->channels);
                    iconX += 24;
                }
                for (int i = 0; i < shown; ++i)
                    fb.drawText(textX + (iconX > jX + 122 ? 22 : 0), y + 8 + i * 24, lines[i], Colors::Text, TextStyle::Body);
                y += rowH;
            }
            fb.clearClip();
            fb.drawText(jX + 26, jY + jH - 32, "Up/Down: Scroll  |  Left/Right: Page  |  B: Close", Colors::TextDim,
                        TextStyle::Caption);
        }

        // ---- Roster picker ---------------------------------------------------------------------
        // Who lives in the park: the residents on the left, the save's party and boxes on the right.

        static const ::Pokemon::Pokemon *rosterCandidate(TrainerViewScreen &screen, int box, int slot)
        {
            if (box < 0)
                return slot >= 0 && slot < static_cast<int>(screen.trainer.party.size()) ? screen.trainer.party[slot].get() : nullptr;
            if (box >= static_cast<int>(screen.trainer.boxes.size()) || slot < 0 ||
                slot >= static_cast<int>(screen.trainer.getSlotsPerBox()))
                return nullptr;
            return screen.trainer.boxes[box][slot].get();
        }

        // Residents come from every save, so an id carries the save it came from (resumeKey: the title
        // and account, or a loose save's path) as well as where in it the Pokémon sits.
        static std::string rosterId(TrainerViewScreen &screen, int box, int slot)
        {
            const ::Pokemon::Pokemon *mon = rosterCandidate(screen, box, slot);
            return mon ? PokeparkManager::residentKey(screen.resumeKey(), *mon) : std::string();
        }

        void drawParkRoster(TrainerViewScreen &screen, PKSEFramebuffer &fb)
        {
            PokeparkManager &mgr = PokeparkManager::instance();
            screen.touchButtons.clear();
            constexpr int wW = 1080, wH = 540;
            const int wX = PANEL_X + (PANEL_W - wW) / 2, wY = PANEL_Y + (PANEL_H - wH) / 2;
            fb.drawFilledRect(0, 0, fb.getWidth(), fb.getHeight(), Color(0x14, 0x12, 0x1D, 0xAA));
            fb.drawSoftShadow(wX, wY, wW, wH, 16);
            fb.drawFilledRoundedRect(wX, wY, wW, wH, 16, Colors::Panel);
            fb.drawCardBand(wX, wY, wW, 46);
            const auto &residents = mgr.getResidents();
            fb.drawText(wX + 24, wY + 10,
                        "Poképark Residents  ·  " + std::to_string(residents.size()) + " / " +
                            std::to_string(PokeparkManager::MAX_RESIDENTS) + "  ·  from every game",
                        Colors::Text, TextStyle::Heading);

            // Left: who lives here.
            const int lX = wX + 18, lY = wY + 60, lW = 340, rowH = 34;
            for (int i = 0; i < static_cast<int>(PokeparkManager::MAX_RESIDENTS); ++i)
            {
                const int ry = lY + i * rowH;
                const bool focus = screen.pokeparkRosterPane == 0 && screen.pokeparkRosterRow == i;
                if (i >= static_cast<int>(residents.size()))
                {
                    fb.drawRoundedRect(lX, ry, lW, rowH - 4, 6, Colors::Border, 1);
                    fb.drawText(lX + 44, fb.textYCenteredOn(ry + (rowH - 4) / 2, TextStyle::Caption), "Empty spot",
                                Colors::TextDim, TextStyle::Caption);
                    if (focus)
                        fb.pkSelection(lX, ry, lW, rowH - 4);
                    continue;
                }
                const auto &r = residents[i];
                if (focus)
                    fb.pkSelectedButton(lX, ry, lW, rowH - 4, 4);
                else
                    fb.drawFilledRoundedRect(lX, ry, lW, rowH - 4, 6, Colors::PanelAlt);
                if (Sprite *icon = SpriteManager::getIconSprite(r.species, r.form, r.shiny); icon && icon->data)
                    fb.drawSpriteFit(lX + 4, ry - 2, 38, 32, icon->width, icon->height, icon->data, icon->channels);
                fb.drawText(lX + 46, fb.textYCenteredOn(ry + (rowH - 4) / 2), r.name, Colors::Text);
                // Which game it came from (the park is shared, so this is the useful thing to know here).
                const size_t cut = r.sourceInfo.find("  ·  ");
                std::string game = cut != std::string::npos ? r.sourceInfo.substr(0, cut) : std::string("Earlier game");
                if (game.rfind("Pokémon ", 0) == 0)
                    game = game.substr(std::string("Pokémon ").size());
                int tw, th;
                fb.measureText(game, tw, th, TextStyle::Caption);
                const int maxW = lW - 46 - 130;
                while (tw > maxW && game.size() > 4)
                {
                    game.pop_back();
                    while (!game.empty() && (static_cast<unsigned char>(game.back()) & 0xC0) == 0x80)
                        game.pop_back();
                    fb.measureText(game + "...", tw, th, TextStyle::Caption);
                    if (tw <= maxW)
                        game += "...";
                }
                fb.drawText(lX + lW - tw - 12, fb.textYCenteredOn(ry + (rowH - 4) / 2, TextStyle::Caption), game,
                            Colors::TextDim, TextStyle::Caption);
                screen.touchButtons.push_back({400 + i, lX, ry, lW, rowH - 4});
            }

            // Right: the save, a box at a time (the party first).
            const int gX = wX + 390, gY = wY + 60, gW = wW - 390 - 18;
            const int box = screen.pokeparkRosterBox;
            const std::string boxName = box < 0 ? std::string("Party") : "Box " + std::to_string(box + 1);
            fb.pkBoxNameBar(gX, gY, gW, 40, boxName, true, true);
            screen.touchButtons.push_back({440, gX, gY, 60, 40});
            screen.touchButtons.push_back({441, gX + gW - 60, gY, 60, 40});
            const int slots = box < 0 ? 6 : static_cast<int>(screen.trainer.getSlotsPerBox());
            const int cols = box < 0 ? 3 : boxGridColumns(slots);
            const int rows = (slots + cols - 1) / cols;
            const int cellW = 108, cellH = 74;
            const int gridX = gX + (gW - cols * cellW) / 2, gridY = gY + 52;
            fb.pkWallpaper(gX, gY + 44, gW, rows * cellH + 16, Pksm::wallpaperAt(std::max(0, box)));
            int focusX = -1, focusY = -1;
            for (int slot = 0; slot < slots; ++slot)
            {
                const int cx = gridX + (slot % cols) * cellW, cy = gridY + (slot / cols) * cellH;
                const ::Pokemon::Pokemon *mon = rosterCandidate(screen, box, slot);
                const bool present = mon && mon->speciesID() != 0 && !mon->isEgg();
                if (present)
                {
                    const bool shiny = mon->isShiny(mon->id32(), mon->species());
                    if (Sprite *icon = SpriteManager::getIconSprite(mon->speciesID(), mon->form(), shiny); icon && icon->data)
                        fb.drawSpriteFit(cx + (cellW - 68) / 2, cy + (cellH - 56) / 2, 68, 56, icon->width, icon->height,
                                         icon->data, icon->channels, mgr.hasResident(rosterId(screen, box, slot)) ? 255 : 200);
                    if (mgr.hasResident(rosterId(screen, box, slot)))
                    {
                        // Lives in the park: a leaf badge.
                        fb.drawFilledCircle(cx + cellW - 22, cy + 14, 10, Color(70, 170, 90));
                        fb.drawText(cx + cellW - 27, fb.textYCenteredOn(cy + 14, TextStyle::Caption), "P", Color(255, 255, 255),
                                    TextStyle::Caption);
                    }
                }
                else
                    fb.pkEmptySlot(cx + (cellW - 56) / 2, cy + (cellH - 56) / 2, 56, 56, Pksm::wallpaperAt(std::max(0, box)));
                if (screen.pokeparkRosterPane == 1 && screen.pokeparkRosterSlot == slot)
                {
                    focusX = cx + cellW / 2;
                    focusY = cy + cellH / 2;
                }
                screen.touchButtons.push_back({450 + slot, cx, cy, cellW, cellH});
            }
            if (focusX >= 0)
            {
                float gx, gy;
                fb.glide("park.roster.cursor", static_cast<float>(focusX), static_cast<float>(focusY), gx, gy);
                fb.pkSelection(static_cast<int>(gx) - 34, static_cast<int>(gy) - 32, 68, 64);
            }

            // What A will do.
            std::string info;
            if (screen.pokeparkRosterPane == 0)
                info = screen.pokeparkRosterRow < static_cast<int>(residents.size())
                           ? "A: " + residents[screen.pokeparkRosterRow].name + " leaves the park"
                           : "Pick a Pokémon on the right to fill this spot";
            else if (const ::Pokemon::Pokemon *mon = rosterCandidate(screen, box, screen.pokeparkRosterSlot);
                     mon && mon->speciesID() != 0 && !mon->isEgg())
            {
                const std::string name =
                    mon->nickname().empty() ? std::string(mon->species()) : Utils::utf16ToUtf8(mon->nickname());
                info = mgr.hasResident(rosterId(screen, box, screen.pokeparkRosterSlot)) ? "A: " + name + " leaves the park"
                       : residents.size() >= PokeparkManager::MAX_RESIDENTS ? name + ": the park is full"
                                                                            : "A: " + name + " moves into the park";
            }
            fb.drawText(gX, wY + wH - 92, info, Colors::Text, TextStyle::Body);
            if (screen.pokeparkRefillArmed > 0.0f)
                screen.pokeparkRefillArmed -= fb.getDeltaSeconds();
            drawPopupHints(fb, wX + 16, wY + wH - 52, wW - 32, 40,
                           screen.pokeparkRefillArmed > 0.0f
                               ? "X: Press again to send your whole party  |  B: Cancel"
                               : "D-Pad: Move  |  L/R: Box  |  A: Send / Bring back  |  X: Send party  |  B: Done");
        }

        // ---- The park --------------------------------------------------------------------------

        void drawPokepark(PokeparkView &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();
            PokeparkManager &mgr = PokeparkManager::instance();
            const bool photo = screen.pokeparkPhotoMode;
            const bool capturing = photo && screen.pokeparkPhotoCapture;
            // While a card, the journal or a line of dialogue is up, only it is tappable.
            const bool overlay = screen.pokeparkCardActive || screen.pokeparkJournalActive || !screen.pokeparkDialogueText.empty();

            // Photo mode: this frame is the photo (the hint is left out of it); the next one reports.
            if (screen.pokeparkPhotoPending)
            {
                const int result = framebuffer.screenshotResult();
                if (result != 0)
                {
                    screen.pokeparkPhotoPending = false;
                    screen.pokeparkToast = result > 0 ? "Photo saved to /PKSE/photos/" + screen.pokeparkPhotoName
                                                      : std::string("Couldn't save the photo. Check SD card space.");
                    screen.pokeparkToastTimer = 3.0f;
                    if (result > 0)
                        mgr.recordPhoto(screen.pokeparkPhotoName);
                }
            }
            if (capturing)
            {
                framebuffer.requestScreenshot("sdmc:/PKSE/photos/" + screen.pokeparkPhotoName);
                screen.pokeparkPhotoCapture = false;
                screen.pokeparkPhotoPending = true;
            }
            if (screen.pokeparkToastTimer > 0.0f)
                screen.pokeparkToastTimer -= framebuffer.getDeltaSeconds();

            // Advance the world on real time; a photo, or a card being read, holds still.
            if (!photo && !screen.pokeparkCardActive && !screen.pokeparkJournalActive)
                mgr.update(framebuffer.getDeltaSeconds());

            const ParkEnvironmentId env = mgr.getCurrentHabitat();
            const double time = framebuffer.getTimeSeconds();
            const auto visibleIndices = mgr.getVisibleResidentIndices(env);
            const auto &residents = mgr.getResidents();
            if (screen.pokeparkSelectedResident < 0)
                screen.pokeparkSelectedResident = 0;
            if (!visibleIndices.empty() && screen.pokeparkSelectedResident >= static_cast<int>(visibleIndices.size()))
                screen.pokeparkSelectedResident = static_cast<int>(visibleIndices.size()) - 1;
            const int selected = visibleIndices.empty() ? -1 : visibleIndices[screen.pokeparkSelectedResident];

            // Parallax follows the selected resident, gently.
            const float focusX = selected >= 0 ? residents[selected].x : PokeparkManager::SCENE_W / 2;
            const float camera = framebuffer.spring("park.camera", focusX - PokeparkManager::SCENE_W / 2, 60.0f, 15.5f, 0.0f);

            framebuffer.drawSoftShadow(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 16);
            framebuffer.drawFilledRoundedRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 16, Colors::Panel);
            const int clipTop = PANEL_Y + 2, clipH = PANEL_H - 4;
            framebuffer.setClipRect(PANEL_X + 2, clipTop, PANEL_W - 4, clipH);
            drawSky(framebuffer, PANEL_X + 2, PANEL_Y + 2, PANEL_W - 4, PANEL_H - 4, mgr.getHour(), -camera * 0.02f, time);
            drawFarLayer(framebuffer, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, env, -camera * 0.03f);
            drawMidLayer(framebuffer, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, env, -camera * 0.06f, time);
            drawGround(framebuffer, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, env, time, mgr.berryRipeness());

            // Residents and the visitor, back to front.
            std::vector<Actor> actors;
            for (size_t vi = 0; vi < visibleIndices.size(); ++vi)
                actors.push_back({residents[visibleIndices[vi]].y, visibleIndices[vi], static_cast<int>(vi)});
            const ParkVisitor &visitor = mgr.getVisitor();
            if (visitor.active)
                actors.push_back({visitor.y, -1, -1});
            std::sort(actors.begin(), actors.end(), [](const Actor &a, const Actor &b) { return a.y < b.y; });

            drawToys(framebuffer, mgr.getToys(), clipTop, clipH);
            if (mgr.getResidents().empty() && !photo)
            {
                const char *empty = "Nobody lives here yet. Open a game and pick Poképark to send Pokémon over.";
                int ew, eh;
                framebuffer.measureText(empty, ew, eh, TextStyle::Body);
                framebuffer.drawFilledRoundedRect(PANEL_X + (PANEL_W - ew) / 2 - 20, PANEL_Y + 250, ew + 40, eh + 24, 12,
                                                  Color(20, 25, 35, 200));
                framebuffer.drawText(PANEL_X + (PANEL_W - ew) / 2, PANEL_Y + 262, empty, Color(255, 255, 255));
            }
            for (const Actor &actor : actors)
            {
                if (actor.index < 0)
                {
                    drawCreature(framebuffer, env, visitor.species, 0, visitor.shiny, visitor.x, visitor.y, visitor.facingLeft,
                                 0.0f, ParkActivity::Wander, visitor.animClock, true, clipTop, clipH);
                    if (!photo)
                    {
                        const int vx = PANEL_X + static_cast<int>(visitor.x), vy = PANEL_Y + static_cast<int>(visitor.y);
                        framebuffer.drawPill(vx - 22, vy + 6, 44, 16, Color(60, 140, 70, 200));
                        framebuffer.drawText(vx - 14, vy + 6, "wild", Color(255, 255, 255), TextStyle::Caption);
                    }
                    continue;
                }
                const auto &res = residents[actor.index];
                const bool isSelected = actor.index == selected && !photo;
                const int rx = PANEL_X + static_cast<int>(res.x), feet = PANEL_Y + static_cast<int>(res.y);
                const float depth = depthScale(res.y);
                if (isSelected)
                {
                    framebuffer.drawFilledEllipse(rx, feet, static_cast<int>(32 * depth), static_cast<int>(11 * depth),
                                                  Color(255, 215, 0, 110));
                }
                drawCreature(framebuffer, env, res.species, res.form, res.shiny, res.x, res.y, res.facingLeft, res.hop,
                             res.activity, res.animClock, res.isMoving, clipTop, clipH);
                const int head = feet - static_cast<int>(64 * depth) - static_cast<int>(res.hop * 14 * depth);
                if (res.emotion != ParkEmotion::None && !capturing)
                    drawEmotionBubble(framebuffer, rx, head - 2, res.emotion, res.emotionAge);
                if (isSelected)
                {
                    int nw, nh;
                    framebuffer.measureText(res.name, nw, nh, TextStyle::Caption);
                    const int npW = nw + 18, npH = 22, npX = rx - npW / 2, npY = head - (res.emotion != ParkEmotion::None ? 56 : 28);
                    framebuffer.drawPill(npX, npY, npW, npH, Color(20, 25, 35, 210));
                    framebuffer.drawText(npX + 9, npY + (npH - nh) / 2, res.name, Color(255, 255, 255), TextStyle::Caption);
                }
                if (!overlay)
                    screen.touchButtons.push_back({200 + actor.visibleIndex, rx - 35, head - 10, 70, feet - head + 20});
            }
            drawWeatherTint(framebuffer, PANEL_X + 2, PANEL_Y + 2, PANEL_W - 4, PANEL_H - 4, mgr.getWeather(),
                            mgr.getHour() >= 21 || mgr.getHour() < 6);
            drawParticles(framebuffer, PANEL_X, PANEL_Y, mgr.getParticles());

            // Tapping open ground throws a ball there (residents' targets above take precedence).
            if (!photo && !overlay)
                screen.touchButtons.push_back({270, PANEL_X, PANEL_Y + 60, PANEL_W, PANEL_H - 100});

            if (!photo)
            {
                // Header: habitat, weather and time, the habitat switcher and the journal.
                const auto &envInfo = PokeparkManager::getEnvironmentInfo(env);
                constexpr int headerH = 50;
                framebuffer.drawFilledRect(PANEL_X, PANEL_Y, PANEL_W, headerH, Color(20, 25, 35, 190));
                framebuffer.drawText(PANEL_X + 24, PANEL_Y + 12, std::string(envInfo.name) + "  ·  " + envInfo.subtitle,
                                     Color(255, 255, 255), TextStyle::Heading);
                char clock[8];
                std::snprintf(clock, sizeof(clock), "%02d:00", mgr.getHour());
                const std::string sky = std::string(PokeparkManager::getWeatherName(mgr.getWeather())) + "  ·  " + clock;
                int sw, sh;
                framebuffer.measureText(sky, sw, sh, TextStyle::Caption);

                const std::string habitatNav = "L  Habitats  R";
                int hNavW, hNavH;
                framebuffer.measureText(habitatNav, hNavW, hNavH, TextStyle::Caption);
                const int navPillW = hNavW + 28, navPillH = 30;
                const int navPillX = PANEL_X + PANEL_W - navPillW - 20, navPillY = PANEL_Y + 10;
                framebuffer.drawPill(navPillX, navPillY, navPillW, navPillH, Color(255, 255, 255, 40));
                framebuffer.drawText(navPillX + 14, navPillY + (navPillH - hNavH) / 2, habitatNav, Color(255, 255, 255),
                                     TextStyle::Caption);
                if (!overlay)
                {
                    screen.touchButtons.insert(screen.touchButtons.begin(), {250, navPillX, navPillY, navPillW / 2, navPillH});
                    screen.touchButtons.insert(screen.touchButtons.begin(), {251, navPillX + navPillW / 2, navPillY, navPillW / 2, navPillH});
                }

                const int jPillW = 100, jPillX = navPillX - jPillW - 12;
                framebuffer.drawPill(jPillX, navPillY, jPillW, navPillH, Color(255, 255, 255, 40));
                framebuffer.drawText(jPillX + 16, navPillY + (navPillH - hNavH) / 2, "Y: Journal", Color(255, 255, 255),
                                     TextStyle::Caption);
                if (!overlay)
                {
                    screen.touchButtons.insert(screen.touchButtons.begin(), {260, jPillX, navPillY, jPillW, navPillH});
                }

                const int skyPillW = sw + 28, skyPillX = jPillX - skyPillW - 12;
                framebuffer.drawPill(skyPillX, navPillY, skyPillW, navPillH, Color(255, 255, 255, 28));
                framebuffer.drawText(skyPillX + 14, navPillY + (navPillH - sh) / 2, sky, Color(255, 255, 255), TextStyle::Caption);

                // The selected resident's current activity, under the header.
                if (selected >= 0)
                {
                    const auto &res = residents[selected];
                    const std::string doing = res.name + ":  " + res.currentActivity + "   ·   " + PokeparkManager::getMoodName(res.mood);
                    framebuffer.drawText(PANEL_X + 24, PANEL_Y + headerH + 8, doing, Color(255, 255, 255, 230), TextStyle::Caption);
                }
            }
            framebuffer.clearClip();

            if (photo)
            {
                if (!capturing)
                {
                    const char *hint = screen.pokeparkToastTimer > 0.0f ? screen.pokeparkToast.c_str()
                                                                        : "Photo mode   ·   A: Save photo   ·   B: Back";
                    int hw, hh;
                    framebuffer.measureText(hint, hw, hh, TextStyle::Body);
                    const int bx = (framebuffer.getWidth() - hw) / 2 - 18;
                    framebuffer.drawFilledRoundedRect(bx, PANEL_Y + PANEL_H - 50, hw + 36, 36, 10, Color(20, 25, 35, 200));
                    framebuffer.drawText(bx + 18, PANEL_Y + PANEL_H - 44, hint, Color(255, 255, 255));
                }
                return;
            }

            drawPopupHints(framebuffer, PANEL_X + 16, PANEL_Y + PANEL_H - 40, PANEL_W - 32, 34,
                           "L/R: Habitat  |  Left/Right: Select  |  A: Profile  |  X: Talk  |  Up: Call  |  ZL: Ball  |  "
                           "ZR: Berry  |  -: Photo  |  Y: Journal  |  B: Back");

            if (screen.pokeparkCardActive && selected >= 0)
            {
                PopIn pop(framebuffer, "pokeparkCard");
                drawProfileCard(screen, framebuffer, mgr, selected);
            }

            if (!screen.pokeparkDialogueText.empty())
            {
                PopIn pop(framebuffer, "pokeparkDialogue");
                constexpr int dW = 820;
                const auto lines = wrapTextToWidth(framebuffer, screen.pokeparkDialogueText, dW - 52, TextStyle::Body);
                const int dH = 60 + static_cast<int>(std::min<size_t>(lines.size(), 4)) * 28;
                const int dX = PANEL_X + (PANEL_W - dW) / 2, dY = PANEL_Y + PANEL_H - dH - 50;
                framebuffer.drawSoftShadow(dX, dY, dW, dH, 16);
                framebuffer.drawFilledRoundedRect(dX, dY, dW, dH, 16, Colors::Panel);
                framebuffer.drawRoundedRect(dX, dY, dW, dH, 16, Colors::Accent, 2);
                framebuffer.beginTyping();
                for (size_t i = 0; i < lines.size() && i < 4; ++i)
                    framebuffer.drawText(dX + 26, dY + 20 + static_cast<int>(i) * 28, lines[i], Colors::Text, TextStyle::Body);
                framebuffer.endTyping();
                if (framebuffer.typingFinished())
                    framebuffer.drawAdvanceArrow(dX + dW - 30, dY + dH - 14, Colors::Accent);
                screen.touchButtons.insert(screen.touchButtons.begin(), {350, dX, dY, dW, dH});
            }

            if (screen.pokeparkJournalActive)
            {
                PopIn pop(framebuffer, "pokeparkJournal");
                drawJournal(screen, framebuffer, mgr);
            }



            if (screen.pokeparkToastTimer > 0.0f && !screen.pokeparkToast.empty())
            {
                int tw, th;
                framebuffer.measureText(screen.pokeparkToast, tw, th, TextStyle::Body);
                const int bx = (framebuffer.getWidth() - tw) / 2 - 18;
                framebuffer.drawFilledRoundedRect(bx, PANEL_Y + 64, tw + 36, 36, 10, Color(20, 25, 35, 210));
                framebuffer.drawText(bx + 18, PANEL_Y + 70, screen.pokeparkToast, Color(255, 255, 255));
            }
        }
    }
}
