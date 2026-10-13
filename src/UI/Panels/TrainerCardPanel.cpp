#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "UI/Panels/TrainerCardPanel.h"
#include "UI/TrainerViewScreen.h"
#include "UI/Common.h"
#include "UI/PKSEFramebuffer.h"
#include "UI/SpriteManager.h"
#include "Trainer/Trainer.h"
#include "Trainer/Trainer1RBY.h"
#include "Trainer/Trainer2GSC.h"
#include "Trainer/Trainer3FRLG.h"
#include "Trainer/Trainer3RSE.h"
#include "Trainer/Trainer4DP.h"
#include "Trainer/Trainer4PT.h"
#include "Trainer/Trainer4HGSS.h"
#include "Pokemon/Pokemon.h"
#include "Enums/GameVersion.h"

namespace UI
{
    namespace Panels
    {
        namespace
        {
            // Regional styling metadata
            struct RegionTheme
            {
                const char *regionName;
                const char *leagueTitle;
                Color cardBg;
                Color cardBorder;
                Color ribbonBg;
                Color ribbonText;
                Color accentColor;
            };

            RegionTheme getRegionTheme(Enums::GameVersion group, Enums::GameVersion ver)
            {
                (void)ver;
                switch (group)
                {
                case Enums::GameVersion::RBY:
                case Enums::GameVersion::FRLG:
                case Enums::GameVersion::GG:
                    return {
                        "KANTO REGION",
                        "INDIGO PLATEAU POKÉMON LEAGUE",
                        Color(30, 24, 26),
                        Color(180, 50, 45),
                        Color(160, 35, 30),
                        Color(255, 240, 240),
                        Color(230, 60, 50)
                    };
                case Enums::GameVersion::GSC:
                case Enums::GameVersion::HGSS:
                    return {
                        "JOHTO REGION",
                        "JOHTO POKÉMON LEAGUE PASSPORT",
                        Color(32, 28, 20),
                        Color(190, 150, 40),
                        Color(165, 125, 25),
                        Color(255, 248, 220),
                        Color(220, 175, 45)
                    };
                case Enums::GameVersion::RSE:
                case Enums::GameVersion::ORAS:
                    return {
                        "HOENN REGION",
                        "EVER GRANDE POKÉMON LEAGUE",
                        Color(20, 30, 26),
                        Color(40, 160, 110),
                        Color(25, 130, 85),
                        Color(230, 255, 245),
                        Color(45, 195, 130)
                    };
                case Enums::GameVersion::DP:
                case Enums::GameVersion::PT:
                case Enums::GameVersion::BDSP:
                case Enums::GameVersion::PLA:
                    return {
                        "SINNOH REGION",
                        "SINNOH POKÉMON LEAGUE ID",
                        Color(22, 28, 35),
                        Color(70, 140, 195),
                        Color(45, 105, 160),
                        Color(235, 245, 255),
                        Color(85, 165, 230)
                    };
                case Enums::GameVersion::BW:
                case Enums::GameVersion::B2W2:
                    return {
                        "UNOVA REGION",
                        "UNOVA POKÉMON LEAGUE CARD",
                        Color(24, 24, 28),
                        Color(135, 135, 155),
                        Color(45, 45, 55),
                        Color(245, 245, 255),
                        Color(160, 160, 185)
                    };
                case Enums::GameVersion::XY:
                case Enums::GameVersion::ZA:
                    return {
                        "KALOS REGION",
                        "KALOS POKÉMON LEAGUE PASSPORT",
                        Color(20, 26, 38),
                        Color(50, 120, 210),
                        Color(35, 85, 170),
                        Color(235, 245, 255),
                        Color(65, 145, 245)
                    };
                case Enums::GameVersion::SM:
                case Enums::GameVersion::USUM:
                    return {
                        "ALOLA REGION",
                        "ISLAND CHALLENGE PASSPORT",
                        Color(35, 25, 20),
                        Color(210, 105, 40),
                        Color(180, 80, 25),
                        Color(255, 240, 225),
                        Color(240, 125, 45)
                    };
                case Enums::GameVersion::SWSH:
                    return {
                        "GALAR REGION",
                        "WYNDON STADIUM LEAGUE CARD",
                        Color(22, 24, 36),
                        Color(200, 45, 110),
                        Color(150, 30, 85),
                        Color(255, 235, 245),
                        Color(235, 55, 135)
                    };
                case Enums::GameVersion::SV:
                default:
                    return {
                        "PALDEA REGION",
                        "NARANJA / UVA ACADEMY ID",
                        Color(28, 22, 34),
                        Color(165, 65, 195),
                        Color(125, 40, 160),
                        Color(250, 235, 255),
                        Color(195, 75, 230)
                    };
                }
            }

            // Gym badge specifications for each region
            struct BadgeData
            {
                const char *name;
                Color primaryColor;
                Color secondaryColor;
                int shape; // 0=Octagon, 1=Teardrop, 2=Star, 3=Flower, 4=Heart, 5=Diamond, 6=Shield, 7=Feather
            };

            const BadgeData *getRegionalBadges(Enums::GameVersion group)
            {
                static const BadgeData kantoBadges[8] = {
                    {"Boulder", Color(145, 150, 160), Color(200, 205, 215), 0},
                    {"Cascade", Color(45, 140, 225),  Color(150, 220, 255), 1},
                    {"Thunder", Color(235, 185, 30),  Color(255, 240, 120), 2},
                    {"Rainbow", Color(60, 185, 95),   Color(220, 100, 180), 3},
                    {"Soul",    Color(195, 55, 145),  Color(245, 135, 205), 4},
                    {"Marsh",   Color(220, 160, 40),  Color(255, 245, 180), 0},
                    {"Volcano", Color(230, 60, 40),   Color(255, 150, 70),  5},
                    {"Earth",   Color(70, 165, 60),   Color(195, 235, 110), 7}
                };

                static const BadgeData johtoBadges[8] = {
                    {"Zephyr",  Color(120, 175, 225), Color(215, 235, 255), 7},
                    {"Hive",    Color(195, 160, 40),  Color(245, 215, 110), 0},
                    {"Plain",   Color(220, 130, 160), Color(255, 200, 220), 5},
                    {"Fog",     Color(135, 75, 185),  Color(205, 155, 245), 4},
                    {"Storm",   Color(60, 125, 210),  Color(165, 205, 255), 6},
                    {"Mineral", Color(165, 175, 190), Color(225, 235, 245), 0},
                    {"Glacier", Color(80, 205, 225),  Color(200, 245, 255), 5},
                    {"Rising",  Color(85, 60, 195),   Color(175, 145, 245), 1}
                };

                static const BadgeData hoennBadges[8] = {
                    {"Stone",   Color(140, 140, 150), Color(195, 195, 205), 0},
                    {"Knuckle", Color(185, 125, 55),  Color(235, 185, 120), 6},
                    {"Dynamo",  Color(235, 200, 40),  Color(255, 245, 140), 2},
                    {"Heat",    Color(235, 65, 45),   Color(255, 160, 75),  5},
                    {"Balance", Color(85, 115, 170),  Color(215, 145, 75),  0},
                    {"Feather", Color(105, 195, 235), Color(220, 245, 255), 7},
                    {"Mind",    Color(185, 75, 185),  Color(245, 155, 245), 4},
                    {"Rain",    Color(45, 115, 225),  Color(140, 195, 255), 1}
                };

                static const BadgeData sinnohBadges[8] = {
                    {"Coal",    Color(115, 115, 125), Color(180, 180, 190), 0},
                    {"Forest",  Color(65, 175, 95),   Color(185, 240, 145), 3},
                    {"Cobble",  Color(175, 135, 80),  Color(230, 195, 140), 6},
                    {"Fen",     Color(45, 130, 215),  Color(150, 215, 255), 1},
                    {"Relic",   Color(145, 65, 180),  Color(215, 145, 245), 4},
                    {"Mine",    Color(135, 150, 170), Color(205, 215, 230), 0},
                    {"Icicle",  Color(75, 200, 225),  Color(200, 245, 255), 5},
                    {"Beacon",  Color(235, 195, 35),  Color(255, 245, 135), 2}
                };

                static const BadgeData unovaBadges[8] = {
                    {"Trio",    Color(85, 185, 115),  Color(235, 85, 85),   0},
                    {"Basic",   Color(165, 145, 105), Color(225, 215, 175), 0},
                    {"Insect",  Color(115, 190, 65),  Color(200, 245, 120), 3},
                    {"Bolt",    Color(235, 210, 45),  Color(255, 250, 155), 2},
                    {"Quake",   Color(185, 115, 55),  Color(235, 175, 110), 6},
                    {"Jet",     Color(95, 185, 235),  Color(215, 245, 255), 7},
                    {"Freeze",  Color(80, 195, 225),  Color(195, 245, 255), 5},
                    {"Legend",  Color(125, 55, 195),  Color(220, 125, 245), 1}
                };

                static const BadgeData kalosBadges[8] = {
                    {"Bug",     Color(95, 180, 65),   Color(195, 240, 120), 3},
                    {"Cliff",   Color(180, 125, 65),  Color(235, 185, 125), 0},
                    {"Rumble",  Color(215, 65, 75),   Color(255, 155, 145), 6},
                    {"Plant",   Color(55, 175, 85),   Color(175, 240, 135), 7},
                    {"Voltage", Color(235, 205, 45),  Color(255, 245, 145), 2},
                    {"Fairy",   Color(225, 115, 175), Color(255, 195, 230), 4},
                    {"Psychic", Color(145, 65, 195),  Color(225, 155, 255), 5},
                    {"Iceberg", Color(80, 200, 230),  Color(205, 245, 255), 0}
                };

                static const BadgeData alolaBadges[8] = {
                    {"Melemele",Color(235, 195, 45),  Color(255, 245, 145), 2},
                    {"Akala",   Color(225, 95, 155),  Color(255, 185, 215), 4},
                    {"Ula'ula", Color(225, 60, 50),   Color(255, 155, 125), 5},
                    {"Poni",    Color(125, 65, 195),  Color(215, 155, 255), 1},
                    {"Trial",   Color(60, 180, 115),  Color(175, 240, 165), 3},
                    {"Kahuna",  Color(185, 135, 55),  Color(235, 195, 125), 0},
                    {"Champion",Color(235, 175, 35),  Color(255, 235, 110), 6},
                    {"Master",  Color(65, 145, 235),  Color(175, 225, 255), 7}
                };

                static const BadgeData galarBadges[8] = {
                    {"Grass",   Color(65, 180, 85),   Color(185, 240, 145), 3},
                    {"Water",   Color(45, 135, 225),  Color(160, 220, 255), 1},
                    {"Fire",    Color(230, 75, 45),   Color(255, 165, 85),  5},
                    {"Combat",  Color(195, 65, 95),   Color(245, 145, 165), 6},
                    {"Fairy",   Color(225, 120, 180), Color(255, 205, 235), 4},
                    {"Rock",    Color(175, 135, 75),  Color(230, 195, 135), 0},
                    {"Dark",    Color(95, 55, 115),   Color(175, 125, 195), 0},
                    {"Dragon",  Color(75, 65, 205),   Color(165, 155, 255), 7}
                };

                static const BadgeData paldeaBadges[8] = {
                    {"Bug",     Color(115, 180, 60),  Color(200, 240, 125), 3},
                    {"Grass",   Color(65, 180, 95),   Color(180, 240, 155), 7},
                    {"Electric",Color(235, 210, 45),  Color(255, 250, 145), 2},
                    {"Water",   Color(50, 145, 230),  Color(165, 225, 255), 1},
                    {"Normal",  Color(155, 155, 165), Color(215, 215, 225), 0},
                    {"Ghost",   Color(135, 65, 185),  Color(215, 145, 255), 4},
                    {"Psychic", Color(215, 75, 165),  Color(255, 165, 225), 5},
                    {"Ice",     Color(75, 205, 235),  Color(195, 245, 255), 0}
                };

                switch (group)
                {
                case Enums::GameVersion::RBY:
                case Enums::GameVersion::FRLG:
                case Enums::GameVersion::GG:
                    return kantoBadges;
                case Enums::GameVersion::GSC:
                case Enums::GameVersion::HGSS:
                    return johtoBadges;
                case Enums::GameVersion::RSE:
                case Enums::GameVersion::ORAS:
                    return hoennBadges;
                case Enums::GameVersion::DP:
                case Enums::GameVersion::PT:
                case Enums::GameVersion::BDSP:
                case Enums::GameVersion::PLA:
                    return sinnohBadges;
                case Enums::GameVersion::BW:
                case Enums::GameVersion::B2W2:
                    return unovaBadges;
                case Enums::GameVersion::XY:
                case Enums::GameVersion::ZA:
                    return kalosBadges;
                case Enums::GameVersion::SM:
                case Enums::GameVersion::USUM:
                    return alolaBadges;
                case Enums::GameVersion::SWSH:
                    return galarBadges;
                case Enums::GameVersion::SV:
                default:
                    return paldeaBadges;
                }
            }
        } // namespace

        uint8_t readSaveBadges(TrainerViewScreen &screen)
            {
                auto &t = screen.trainer;
                const auto g = t.getGameGroup();

                if (g == Enums::GameVersion::RBY)
                {
                    return static_cast<const Trainer::Trainer1RBY &>(t).badgeFlags;
                }
                if (g == Enums::GameVersion::GSC)
                {
                    const auto &gsc = static_cast<const Trainer::Trainer2GSC &>(t);
                    const auto &save = gsc.getSaveData();
                    if (save.size() > 0x23E4)
                        return save[0x23E4];
                }
                if (g == Enums::GameVersion::FRLG)
                {
                    const auto &frlg = static_cast<const Trainer::Trainer3FRLG &>(t);
                    uint8_t mask = 0;
                    for (int i = 0; i < 8; ++i)
                    {
                        uint8_t b = 0;
                        frlg.readBlock(Trainer::Trainer3FRLG::LARGE_ID, 0xEE0 + ((0x820 + i) >> 3), &b, 1);
                        if (b & (1 << ((0x820 + i) & 7)))
                            mask |= (1 << i);
                    }
                    return mask;
                }
                if (g == Enums::GameVersion::RSE)
                {
                    const auto &rse = static_cast<const Trainer::Trainer3RSE &>(t);
                    const bool isEmerald = (t.getGameVersion() == Enums::GameVersion::EM);
                    const size_t baseOfs = isEmerald ? 0x1270 : 0x1220;
                    uint8_t mask = 0;
                    for (int i = 0; i < 8; ++i)
                    {
                        uint8_t b = 0;
                        rse.readBlock(Trainer::Trainer3RSE::LARGE_ID, baseOfs + ((0x800 + i) >> 3), &b, 1);
                        if (b & (1 << ((0x800 + i) & 7)))
                            mask |= (1 << i);
                    }
                    return mask;
                }
                if (g == Enums::GameVersion::DP || g == Enums::GameVersion::PT || g == Enums::GameVersion::HGSS)
                {
                    const uint8_t *gen = nullptr;
                    size_t flagBase = 0;
                    if (g == Enums::GameVersion::DP) {
                        gen = static_cast<const Trainer::Trainer4DP &>(t).getGeneralBlock();
                        flagBase = 0xD9C + 0x10;
                    } else if (g == Enums::GameVersion::PT) {
                        gen = static_cast<const Trainer::Trainer4PT &>(t).getGeneralBlock();
                        flagBase = 0xDAC + 0x10;
                    } else {
                        gen = static_cast<const Trainer::Trainer4HGSS &>(t).getGeneralBlock();
                        flagBase = 0xDE4 + 0x10;
                    }
                    if (gen)
                    {
                        uint8_t mask = 0;
                        for (int i = 0; i < 8; ++i)
                        {
                            const int flagNum = 0x0A0 + i;
                            if (gen[flagBase + (flagNum >> 3)] & (1 << (flagNum & 7)))
                                mask |= (1 << i);
                        }
                        return mask;
                    }
                }

                return screen.trainerCardBadgesCache;
            }

            void writeSaveBadges(TrainerViewScreen &screen, uint8_t badgeMask)
            {
                auto &t = screen.trainer;
                const auto g = t.getGameGroup();
                screen.trainerCardBadgesCache = badgeMask;
                screen.hasUnsavedChanges = true;

                if (g == Enums::GameVersion::RBY)
                {
                    static_cast<Trainer::Trainer1RBY &>(t).badgeFlags = badgeMask;
                    return;
                }
                if (g == Enums::GameVersion::GSC)
                {
                    auto &gsc = static_cast<Trainer::Trainer2GSC &>(t);
                    auto &save = gsc.getSaveDataMutable();
                    if (save.size() > 0x23E4)
                        save[0x23E4] = badgeMask;
                    return;
                }
                if (g == Enums::GameVersion::FRLG)
                {
                    auto &frlg = static_cast<Trainer::Trainer3FRLG &>(t);
                    for (int i = 0; i < 8; ++i)
                    {
                        uint8_t b = 0;
                        frlg.readBlock(Trainer::Trainer3FRLG::LARGE_ID, 0xEE0 + ((0x820 + i) >> 3), &b, 1);
                        if ((badgeMask >> i) & 1)
                            b |= (1 << ((0x820 + i) & 7));
                        else
                            b &= ~(1 << ((0x820 + i) & 7));
                        frlg.writeBlock(Trainer::Trainer3FRLG::LARGE_ID, 0xEE0 + ((0x820 + i) >> 3), &b, 1);
                    }
                    return;
                }
                if (g == Enums::GameVersion::RSE)
                {
                    auto &rse = static_cast<Trainer::Trainer3RSE &>(t);
                    const bool isEmerald = (t.getGameVersion() == Enums::GameVersion::EM);
                    const size_t baseOfs = isEmerald ? 0x1270 : 0x1220;
                    for (int i = 0; i < 8; ++i)
                    {
                        uint8_t b = 0;
                        rse.readBlock(Trainer::Trainer3RSE::LARGE_ID, baseOfs + ((0x800 + i) >> 3), &b, 1);
                        if ((badgeMask >> i) & 1)
                            b |= (1 << ((0x800 + i) & 7));
                        else
                            b &= ~(1 << ((0x800 + i) & 7));
                        rse.writeBlock(Trainer::Trainer3RSE::LARGE_ID, baseOfs + ((0x800 + i) >> 3), &b, 1);
                    }
                    return;
                }
                if (g == Enums::GameVersion::DP || g == Enums::GameVersion::PT || g == Enums::GameVersion::HGSS)
                {
                    uint8_t *gen = nullptr;
                    size_t flagBase = 0;
                    if (g == Enums::GameVersion::DP) {
                        gen = static_cast<Trainer::Trainer4DP &>(t).getGeneralBlock();
                        flagBase = 0xD9C + 0x10;
                    } else if (g == Enums::GameVersion::PT) {
                        gen = static_cast<Trainer::Trainer4PT &>(t).getGeneralBlock();
                        flagBase = 0xDAC + 0x10;
                    } else {
                        gen = static_cast<Trainer::Trainer4HGSS &>(t).getGeneralBlock();
                        flagBase = 0xDE4 + 0x10;
                    }
                    if (gen)
                    {
                        for (int i = 0; i < 8; ++i)
                        {
                            const int flagNum = 0x0A0 + i;
                            uint8_t &b = gen[flagBase + (flagNum >> 3)];
                            if ((badgeMask >> i) & 1)
                                b |= (1 << (flagNum & 7));
                            else
                                b &= ~(1 << (flagNum & 7));
                        }
                    }
                    return;
                }
            }

            // Draw jewel-like vector shape for gym badges
            void drawBadgeGlyph(PKSEFramebuffer &framebuffer, int cx, int cy, int size, int shape,
                                Color primary, Color secondary, bool earned, bool focused)
            {
                const int r = size / 2;
                if (!earned)
                {
                    // Dark, elegant silhouette
                    framebuffer.drawFilledCircle(cx, cy, r + 2, Color(22, 26, 38));
                    framebuffer.drawCircle(cx, cy, r + 2, Color(60, 68, 88), 1);
                    framebuffer.drawFilledCircle(cx, cy, r - 3, Color(30, 35, 50));
                    return;
                }

                // Earned: radiant aura + metallic luster
                if (focused)
                {
                    framebuffer.drawFilledCircle(cx, cy, r + 7, Color(primary.red, primary.green, primary.blue, 70));
                    framebuffer.drawCircle(cx, cy, r + 7, Colors::White, 2);
                }
                else
                {
                    framebuffer.drawFilledCircle(cx, cy, r + 4, Color(primary.red, primary.green, primary.blue, 45));
                }

                // Outer golden bezel
                framebuffer.drawFilledCircle(cx, cy, r + 2, Color(245, 215, 110));
                framebuffer.drawCircle(cx, cy, r + 2, Color(165, 125, 40), 1);

                // Main jewel body
                switch (shape)
                {
                case 1: // Teardrop (Cascade, Rain, etc.)
                {
                    framebuffer.drawFilledCircle(cx, cy + 2, r - 1, primary);
                    // top point
                    framebuffer.drawFilledCircle(cx, cy - r / 2, r / 2, secondary);
                    framebuffer.drawFilledCircle(cx - 3, cy - 2, r / 3, Color(255, 255, 255, 180));
                    break;
                }
                case 2: // Star / Sunburst (Thunder, Beacon, Dynamo)
                {
                    framebuffer.drawFilledCircle(cx, cy, r, primary);
                    framebuffer.drawFilledCircle(cx, cy, r - 4, secondary);
                    // Specular rays
                    framebuffer.drawFilledCircle(cx, cy, r / 3, Color(255, 255, 255, 220));
                    break;
                }
                case 3: // Flower / Petal (Rainbow, Forest, Insect)
                {
                    framebuffer.drawFilledCircle(cx - 4, cy, r / 2 + 1, secondary);
                    framebuffer.drawFilledCircle(cx + 4, cy, r / 2 + 1, secondary);
                    framebuffer.drawFilledCircle(cx, cy - 4, r / 2 + 1, secondary);
                    framebuffer.drawFilledCircle(cx, cy + 4, r / 2 + 1, secondary);
                    framebuffer.drawFilledCircle(cx, cy, r / 2, primary);
                    framebuffer.drawFilledCircle(cx - 2, cy - 2, 2, Color(255, 255, 255, 220));
                    break;
                }
                case 4: // Heart / Soul (Soul, Fog, Fairy)
                {
                    framebuffer.drawFilledCircle(cx - 4, cy - 2, r / 2, primary);
                    framebuffer.drawFilledCircle(cx + 4, cy - 2, r / 2, primary);
                    framebuffer.drawFilledCircle(cx, cy + 3, r / 2 + 1, secondary);
                    framebuffer.drawFilledCircle(cx - 3, cy - 3, 2, Color(255, 255, 255, 220));
                    break;
                }
                case 5: // Diamond (Volcano, Icicle, Heat)
                {
                    framebuffer.drawFilledCircle(cx, cy, r, primary);
                    framebuffer.drawFilledCircle(cx, cy, r - 5, secondary);
                    framebuffer.drawFilledCircle(cx - 3, cy - 3, 3, Color(255, 255, 255, 220));
                    break;
                }
                case 7: // Feather / Wing (Earth, Zephyr, Feather)
                {
                    framebuffer.drawFilledCircle(cx, cy, r, primary);
                    framebuffer.drawFilledCircle(cx + 2, cy - 2, r - 3, secondary);
                    framebuffer.drawFilledCircle(cx - 2, cy - 2, 3, Color(255, 255, 255, 220));
                    break;
                }
                case 0: // Octagon / Geometric (Boulder, Stone, Coal, Plain)
                default:
                {
                    framebuffer.drawFilledCircle(cx, cy, r, primary);
                    framebuffer.drawFilledCircle(cx + 1, cy + 1, r - 3, secondary);
                    // Gloss highlight
                    framebuffer.drawFilledCircle(cx - 4, cy - 4, 3, Color(255, 255, 255, 200));
                    break;
                }
                }
            }

        void drawTrainerCardView(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer)
        {
            screen.touchButtons.clear();

            auto &trainer = screen.trainer;
            const auto group = trainer.getGameGroup();
            const auto version = trainer.getGameVersion();
            const auto theme = getRegionTheme(group, version);

            if (!screen.trainerCardBadgesLoaded)
            {
                screen.trainerCardBadgesCache = readSaveBadges(screen);
                screen.trainerCardBadgesLoaded = true;
            }

            const uint8_t badgesMask = screen.trainerCardBadgesCache;
            int earnedCount = 0;
            for (int i = 0; i < 8; ++i)
                if ((badgesMask >> i) & 1)
                    earnedCount++;

            // =========================================================================
            // 1. MAIN CARD CANVAS (1140 x 580, centered)
            // =========================================================================
            const int cardX = 70, cardY = 74, cardW = 1140, cardH = 586;

            // Outer drop shadow
            framebuffer.drawSoftShadow(cardX, cardY, cardW, cardH, 22);

            // Card background & elegant border
            framebuffer.drawFilledRoundedRect(cardX, cardY, cardW, cardH, 18, theme.cardBg);
            framebuffer.drawRoundedRect(cardX, cardY, cardW, cardH, 18, theme.cardBorder, 2);

            // Subtle inner card bevel
            framebuffer.drawRoundedRect(cardX + 4, cardY + 4, cardW - 8, cardH - 8, 14,
                                        Color(theme.cardBorder.red, theme.cardBorder.green, theme.cardBorder.blue, 60), 1);

            // =========================================================================
            // 2. TOP METALLIC HEADER RIBBON
            // =========================================================================
            const int ribbonH = 46;
            framebuffer.drawFilledRoundedRect(cardX + 2, cardY + 2, cardW - 4, ribbonH, 16, theme.ribbonBg);
            framebuffer.drawFilledRect(cardX + 2, cardY + ribbonH - 14, cardW - 4, 14, theme.ribbonBg);

            // Region & League Title
            char headerBuf[128];
            std::snprintf(headerBuf, sizeof(headerBuf), "%s  ·  %s", theme.regionName, theme.leagueTitle);
            framebuffer.drawText(cardX + 24, cardY + 12, headerBuf, theme.ribbonText, TextStyle::Heading);

            // Star Rating / Rank
            const char *stars[6] = { "☆☆☆☆☆", "★☆☆☆☆", "★★☆☆☆", "★★★☆☆", "★★★★☆", "★★★★★" };
            int starIdx = std::clamp(earnedCount * 5 / 8, 0, 5);
            std::string starStr = stars[starIdx];
            int starW, starH;
            framebuffer.measureText(starStr, starW, starH, TextStyle::Heading);
            framebuffer.drawText(cardX + cardW - starW - 24, cardY + 12, starStr, Color(255, 220, 80), TextStyle::Heading);

            // =========================================================================
            // 3. UPPER LEFT: TRAINER IDENTITY & PROFILE (Avatar, Name, IDs, Gender)
            // =========================================================================
            const int idBoxX = cardX + 24, idBoxY = cardY + ribbonH + 16, idBoxW = 520, idBoxH = 176;
            framebuffer.drawFilledRoundedRect(idBoxX, idBoxY, idBoxW, idBoxH, 14, Colors::Panel);
            framebuffer.drawRoundedRect(idBoxX, idBoxY, idBoxW, idBoxH, 14, Colors::Border, 1);

            // Trainer Avatar Bust (Radius 36)
            const int avCx = idBoxX + 54, avCy = idBoxY + 54;
            const bool isFemale = (trainer.trainerGender == 1);
            const Color genderColor = isFemale ? Color(235, 75, 145) : Color(45, 140, 235);
            const Color genderBg = isFemale ? Color(75, 25, 45) : Color(20, 45, 75);

            // Avatar background ring
            framebuffer.drawFilledCircle(avCx, avCy, 38, Color(40, 45, 60));
            framebuffer.drawFilledCircle(avCx, avCy, 35, genderBg);
            framebuffer.drawCircle(avCx, avCy, 35, genderColor, 2);

            // Stylized vector bust
            framebuffer.drawFilledCircle(avCx, avCy - 10, 14, genderColor);
            framebuffer.drawFilledRoundedRect(avCx - 20, avCy + 8, 40, 22, 11, genderColor);

            // Gender Toggle Badge (Focus Index 2)
            const bool genderFocused = (screen.trainerSelectedRow == 2);
            const int gPillX = idBoxX + 16, gPillY = idBoxY + 104, gPillW = 76, gPillH = 30;
            const char *genderLabel = isFemale ? "♀ FEMALE" : "♂ MALE";
            framebuffer.drawPill(gPillX, gPillY, gPillW, gPillH, genderFocused ? Colors::Primary : genderBg);
            framebuffer.drawText(gPillX + (isFemale ? 8 : 12), gPillY + 7, genderLabel,
                                 genderFocused ? Colors::PrimaryText : genderColor, TextStyle::Caption);
            if (genderFocused)
                framebuffer.drawRoundedRect(gPillX - 2, gPillY - 2, gPillW + 4, gPillH + 4, (gPillH + 4) / 2, Colors::Accent, 2);
            screen.touchButtons.push_back({2, gPillX, gPillY, gPillW, gPillH});

            // Name Block (Focus Index 0)
            const bool nameFocused = (screen.trainerSelectedRow == 0);
            const int nameX = idBoxX + 116, nameY = idBoxY + 16;
            framebuffer.drawText(nameX, nameY, "TRAINER NAME", Colors::TextDim, TextStyle::Caption);

            const std::string displayName = trainer.trainerName.empty() ? "(NO NAME)" : trainer.trainerName;
            framebuffer.drawText(nameX, nameY + 18, displayName, Colors::Text, TextStyle::Heading);

            // Edit button badge next to name
            int dNameW, dNameH;
            framebuffer.measureText(displayName, dNameW, dNameH, TextStyle::Heading);
            const int editPillX = nameX + dNameW + 14, editPillY = nameY + 18, editPillW = 54, editPillH = 26;
            framebuffer.drawPill(editPillX, editPillY, editPillW, editPillH, nameFocused ? Colors::Primary : Colors::PanelAlt);
            framebuffer.drawText(editPillX + 11, editPillY + 5, "EDIT", nameFocused ? Colors::PrimaryText : Colors::TextDim, TextStyle::Caption);
            if (nameFocused)
                framebuffer.drawRoundedRect(editPillX - 2, editPillY - 2, editPillW + 4, editPillH + 4, (editPillH + 4) / 2, Colors::Accent, 2);
            screen.touchButtons.push_back({0, nameX, nameY, dNameW + 72, 48});

            // IDs Block: TID16 (Focus Index 3) and SID16 (Focus Index 4)
            const int idRowY = idBoxY + 82;
            framebuffer.drawHDivider(nameX, idRowY, idBoxW - 130);

            // TID16
            const bool tidFocused = (screen.trainerSelectedRow == 3);
            const int tidX = nameX, tidY = idRowY + 12;
            char tidBuf[32];
            std::snprintf(tidBuf, sizeof(tidBuf), "TID: #%05u", trainer.TID16);
            framebuffer.drawPill(tidX, tidY, 110, 28, tidFocused ? Colors::Primary : Colors::Background);
            framebuffer.drawText(tidX + 10, tidY + 6, tidBuf, tidFocused ? Colors::PrimaryText : Colors::Text, TextStyle::Caption);
            if (tidFocused)
                framebuffer.drawRoundedRect(tidX - 2, tidY - 2, 114, 32, 16, Colors::Accent, 2);
            screen.touchButtons.push_back({3, tidX, tidY, 110, 28});

            // SID16
            const bool sidFocused = (screen.trainerSelectedRow == 4);
            const int sidX = tidX + 120, sidY = tidY;
            char sidBuf[32];
            std::snprintf(sidBuf, sizeof(sidBuf), "SID: #%05u", trainer.SID16);
            framebuffer.drawPill(sidX, sidY, 110, 28, sidFocused ? Colors::Primary : Colors::Background);
            framebuffer.drawText(sidX + 10, sidY + 6, sidBuf, sidFocused ? Colors::PrimaryText : Colors::TextDim, TextStyle::Caption);
            if (sidFocused)
                framebuffer.drawRoundedRect(sidX - 2, sidY - 2, 114, 32, 16, Colors::Accent, 2);
            screen.touchButtons.push_back({4, sidX, sidY, 110, 28});

            // Gen 7+ Display ID
            if (trainer.TID != 0 || trainer.SID != 0)
            {
                char dispIdBuf[48];
                std::snprintf(dispIdBuf, sizeof(dispIdBuf), "Display ID: %06u", trainer.TID);
                framebuffer.drawText(nameX, idRowY + 52, dispIdBuf, Colors::TextDim, TextStyle::Caption);
            }

            // =========================================================================
            // 4. UPPER RIGHT: WEALTH, POKÉDEX & BADGE STATS
            // =========================================================================
            const int statBoxX = idBoxX + idBoxW + 20, statBoxY = idBoxY, statBoxW = 552, statBoxH = idBoxH;
            framebuffer.drawFilledRoundedRect(statBoxX, statBoxY, statBoxW, statBoxH, 14, Colors::Panel);
            framebuffer.drawRoundedRect(statBoxX, statBoxY, statBoxW, statBoxH, 14, Colors::Border, 1);

            // Money Block (Focus Index 1: Edit, Focus Index 5: Max)
            const int mX = statBoxX + 24, mY = statBoxY + 16;
            framebuffer.drawText(mX, mY, "TOTAL MONEY / POKÉDOLLARS", Colors::TextDim, TextStyle::Caption);

            const bool moneyFocused = (screen.trainerSelectedRow == 1);
            char moneyBuf[48];
            std::snprintf(moneyBuf, sizeof(moneyBuf), "$ %u", trainer.money);
            framebuffer.drawText(mX, mY + 18, moneyBuf, Color(245, 205, 60), TextStyle::Heading);

            int mW, mH;
            framebuffer.measureText(moneyBuf, mW, mH, TextStyle::Heading);
            const int mPillX = mX + mW + 16, mPillY = mY + 18, mPillW = 54, mPillH = 26;
            framebuffer.drawPill(mPillX, mPillY, mPillW, mPillH, moneyFocused ? Colors::Primary : Colors::PanelAlt);
            framebuffer.drawText(mPillX + 11, mPillY + 5, "EDIT", moneyFocused ? Colors::PrimaryText : Colors::TextDim, TextStyle::Caption);
            if (moneyFocused)
                framebuffer.drawRoundedRect(mPillX - 2, mPillY - 2, mPillW + 4, mPillH + 4, (mPillH + 4) / 2, Colors::Accent, 2);
            screen.touchButtons.push_back({1, mX, mY, mW + 72, 48});

            // Quick Max Money Button (Focus Index 5)
            const bool maxFocused = (screen.trainerSelectedRow == 5);
            const int maxPillX = mPillX + mPillW + 10, maxPillY = mPillY, maxPillW = 88, maxPillH = 26;
            framebuffer.drawPill(maxPillX, maxPillY, maxPillW, maxPillH, maxFocused ? Color(230, 160, 30) : Color(60, 50, 30));
            framebuffer.drawText(maxPillX + 12, maxPillY + 5, "[Y] MAX", maxFocused ? Colors::White : Color(245, 205, 60), TextStyle::Caption);
            if (maxFocused)
                framebuffer.drawRoundedRect(maxPillX - 2, maxPillY - 2, maxPillW + 4, maxPillH + 4, (maxPillH + 4) / 2, Colors::Accent, 2);
            screen.touchButtons.push_back({5, maxPillX, maxPillY, maxPillW, maxPillH});

            framebuffer.drawHDivider(mX, statBoxY + 76, statBoxW - 48);

            // Pokédex Completion Calculation
            int ownedSpecies = 0;
            std::vector<bool> speciesSeen(1026, false);
            for (const auto &p : trainer.party)
            {
                if (p && p->speciesID() > 0 && p->speciesID() <= 1025)
                {
                    if (!speciesSeen[p->speciesID()])
                    {
                        speciesSeen[p->speciesID()] = true;
                        ownedSpecies++;
                    }
                }
            }
            for (const auto &box : trainer.boxes)
            {
                for (const auto &p : box)
                {
                    if (p && p->speciesID() > 0 && p->speciesID() <= 1025)
                    {
                        if (!speciesSeen[p->speciesID()])
                        {
                            speciesSeen[p->speciesID()] = true;
                            ownedSpecies++;
                        }
                    }
                }
            }

            // Dex Progress Bar
            const int dexY = statBoxY + 90;
            char dexBuf[64];
            std::snprintf(dexBuf, sizeof(dexBuf), "POKÉDEX PROGRESS:  %d SPECIES REGISTERED", ownedSpecies);
            framebuffer.drawText(mX, dexY, dexBuf, Colors::TextDim, TextStyle::Caption);

            const int barW = statBoxW - 48, barH = 14, barY = dexY + 22;
            framebuffer.drawFilledRoundedRect(mX, barY, barW, barH, 7, Colors::Background);
            const int maxDexTarget = (group == Enums::GameVersion::RBY || group == Enums::GameVersion::FRLG) ? 151 :
                                     (group == Enums::GameVersion::GSC) ? 251 :
                                     (group == Enums::GameVersion::RSE) ? 386 :
                                     (group == Enums::GameVersion::DP || group == Enums::GameVersion::PT || group == Enums::GameVersion::HGSS) ? 493 :
                                     (group == Enums::GameVersion::BW || group == Enums::GameVersion::B2W2) ? 649 :
                                     (group == Enums::GameVersion::XY || group == Enums::GameVersion::ORAS) ? 721 :
                                     (group == Enums::GameVersion::SM || group == Enums::GameVersion::USUM) ? 807 : 1025;
            const int filledW = std::clamp((ownedSpecies * barW) / maxDexTarget, 10, barW);
            framebuffer.drawFilledRoundedRect(mX, barY, filledW, barH, 7, theme.accentColor);

            // Badges Summary Text
            char badgeSummaryBuf[64];
            std::snprintf(badgeSummaryBuf, sizeof(badgeSummaryBuf), "GYM BADGES EARNED:  %d / 8", earnedCount);
            framebuffer.drawText(mX, barY + 22, badgeSummaryBuf, Colors::Text, TextStyle::Caption);

            // =========================================================================
            // 5. MIDDLE SECTION: SIGNATURE PARTY SHOWCASE (6 SLOTS)
            // =========================================================================
            const int partyBoxX = cardX + 24, partyBoxY = idBoxY + idBoxH + 16, partyBoxW = cardW - 48, partyBoxH = 126;
            framebuffer.drawFilledRoundedRect(partyBoxX, partyBoxY, partyBoxW, partyBoxH, 14, Color(16, 20, 28));
            framebuffer.drawRoundedRect(partyBoxX, partyBoxY, partyBoxW, partyBoxH, 14, Colors::Border, 1);

            // Mini header
            framebuffer.drawText(partyBoxX + 16, partyBoxY + 8, "CURRENT PARTY SHOWCASE", Colors::TextDim, TextStyle::Caption);

            const int slotW = 168, slotH = 88, slotGap = 12;
            const int slotStartX = partyBoxX + 14, slotY = partyBoxY + 28;

            for (size_t s = 0; s < 6; ++s)
            {
                const int curSlotX = slotStartX + static_cast<int>(s) * (slotW + slotGap);
                framebuffer.drawFilledRoundedRect(curSlotX, slotY, slotW, slotH, 10, Colors::Panel);
                framebuffer.drawRoundedRect(curSlotX, slotY, slotW, slotH, 10, Colors::Border, 1);

                if (s < trainer.party.size() && trainer.party[s] && trainer.party[s]->speciesID() != 0)
                {
                    const auto &pk = trainer.party[s];
                    const bool isShiny = pk->isShiny(pk->id32(), pk->species());
                    Sprite *icon = SpriteManager::getIconSprite(pk->speciesID(), pk->form(), isShiny);

                    // Sprite Icon
                    if (icon && icon->data)
                    {
                        const int sprSize = 44;
                        framebuffer.drawImageScaled(curSlotX + 6, slotY + (slotH - sprSize) / 2,
                                                    icon->width, icon->height, sprSize, sprSize,
                                                    icon->data, icon->channels);
                    }

                    // Species Name
                    const char *specName = Trainer::getSpeciesName(pk->speciesID());
                    std::string monName = specName ? specName : "Pokémon";
                    if (monName.length() > 9)
                        monName = monName.substr(0, 8) + "..";
                    framebuffer.drawText(curSlotX + 54, slotY + 12, monName, Colors::Text, TextStyle::Body);

                    // Level Pill & Shiny Star
                    char lvlBuf[16];
                    std::snprintf(lvlBuf, sizeof(lvlBuf), "Lv. %u", pk->level());
                    framebuffer.drawPill(curSlotX + 54, slotY + 38, 52, 20, Colors::Background);
                    framebuffer.drawText(curSlotX + 60, slotY + 41, lvlBuf, Colors::Primary, TextStyle::Caption);

                    if (isShiny)
                    {
                        framebuffer.drawText(curSlotX + 110, slotY + 40, "✨", Color(255, 215, 60), TextStyle::Caption);
                    }

                    // Gender symbol
                    const int pkGender = pk->gender();
                    if (pkGender == 0)
                        framebuffer.drawText(curSlotX + slotW - 20, slotY + 12, "♂", Color(60, 150, 245), TextStyle::Caption);
                    else if (pkGender == 1)
                        framebuffer.drawText(curSlotX + slotW - 20, slotY + 12, "♀", Color(245, 80, 160), TextStyle::Caption);

                    // Held item or ball
                    if (pk->heldItem() != 0)
                    {
                        framebuffer.drawText(curSlotX + 54, slotY + 64, "Holding Item", Colors::TextDim, TextStyle::Caption);
                    }
                }
                else
                {
                    // Empty party slot
                    const int ecX = curSlotX + slotW / 2, ecY = slotY + slotH / 2;
                    framebuffer.drawCircle(ecX, ecY, 16, Colors::Border, 1);
                    framebuffer.drawText(ecX - 24, ecY + 22, "Empty Slot", Colors::TextDim, TextStyle::Caption);
                }

                // Touch button to inspect party member
                screen.touchButtons.push_back({static_cast<int>(300 + s), curSlotX, slotY, slotW, slotH});
            }

            // =========================================================================
            // 6. LOWER SECTION: REGIONAL GYM BADGES VELVET CASE
            // =========================================================================
            const int badgeBoxX = partyBoxX, badgeBoxY = partyBoxY + partyBoxH + 14, badgeBoxW = partyBoxW, badgeBoxH = 152;
            framebuffer.drawFilledRoundedRect(badgeBoxX, badgeBoxY, badgeBoxW, badgeBoxH, 14, Color(12, 14, 24));
            framebuffer.drawRoundedRect(badgeBoxX, badgeBoxY, badgeBoxW, badgeBoxH, 14, Color(190, 155, 50, 140), 1);

            // Velvet Case Header & Unlock All Shortcut Button (Focus Index 6)
            framebuffer.drawText(badgeBoxX + 18, badgeBoxY + 10, "REGIONAL GYM BADGES CASE", Color(245, 215, 110), TextStyle::Caption);

            const bool unlockFocused = (screen.trainerSelectedRow == 6);
            const int unlPillW = 154, unlPillH = 26;
            const int unlPillX = badgeBoxX + badgeBoxW - unlPillW - 16, unlPillY = badgeBoxY + 8;
            framebuffer.drawPill(unlPillX, unlPillY, unlPillW, unlPillH, unlockFocused ? Colors::Primary : Color(45, 40, 30));
            framebuffer.drawText(unlPillX + 14, unlPillY + 5, "[X] TOGGLE ALL (8)", unlockFocused ? Colors::PrimaryText : Color(245, 215, 110), TextStyle::Caption);
            if (unlockFocused)
                framebuffer.drawRoundedRect(unlPillX - 2, unlPillY - 2, unlPillW + 4, unlPillH + 4, (unlPillH + 4) / 2, Colors::Accent, 2);
            screen.touchButtons.push_back({6, unlPillX, unlPillY, unlPillW, unlPillH});

            // 8 Badges Rendering
            const BadgeData *badges = getRegionalBadges(group);
            const int badgePitch = (badgeBoxW - 32) / 8;
            const int badgeStartY = badgeBoxY + 68;

            for (int b = 0; b < 8; ++b)
            {
                const int bCx = badgeBoxX + 16 + b * badgePitch + badgePitch / 2;
                const int bCy = badgeStartY;
                const bool hasBadge = (badgesMask & (1 << b)) != 0;
                const bool badgeFocused = (screen.trainerSelectedRow == (10 + b));

                drawBadgeGlyph(framebuffer, bCx, bCy, 44, badges[b].shape,
                               badges[b].primaryColor, badges[b].secondaryColor, hasBadge, badgeFocused);

                // Badge Name
                int bnW, bnH;
                framebuffer.measureText(badges[b].name, bnW, bnH, TextStyle::Caption);
                framebuffer.drawText(bCx - bnW / 2, bCy + 32, badges[b].name,
                                     hasBadge ? Colors::Text : Colors::TextDim, TextStyle::Caption);

                // Touch button for badge
                screen.touchButtons.push_back({200 + b, bCx - 32, bCy - 30, 64, 76});
            }
        }
    } // namespace Panels
} // namespace UI
