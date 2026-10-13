#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "Legality/EncounterLookup.h"
#include "Legality/EncounterTable.h"
#include "Legality/CatchBuilder.h"
#include "Names/LocationNames.h"
#include "Names/FormNames.h"
#include "Names/MoveNames.h"
#include "Names/MoveInfo.h"
#include "Names/MovePresence.h"
#include "Trainer/Trainer.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/PokemonTypes.h"
#include "Pokemon/AbilityInfo.h"
#include "Pokemon/FormInfo.h"
#include "Pokemon/LearnsetTable.h"
#include "Conversion/Convert.h"
#include "Utils/StringHelpers.h"
#include "Utils/HelperUtilities.h"

namespace Legality
{
    struct GameMetadata
    {
        Enums::GameVersion group;
        Enums::GameVersion version;
        const char *name;
        int generation;
    };

    static const GameMetadata MAINLINE_GAMES[] = {
        // Gen 1
        {Enums::GameVersion::RBY, Enums::GameVersion::RD, "Red", 1},
        {Enums::GameVersion::RBY, Enums::GameVersion::BU, "Blue", 1},
        {Enums::GameVersion::RBY, Enums::GameVersion::YW, "Yellow", 1},

        // Gen 2
        {Enums::GameVersion::GSC, Enums::GameVersion::GD, "Gold", 2},
        {Enums::GameVersion::GSC, Enums::GameVersion::SI, "Silver", 2},
        {Enums::GameVersion::GSC, Enums::GameVersion::C, "Crystal", 2},

        // Gen 3
        {Enums::GameVersion::RSE, Enums::GameVersion::RU, "Ruby", 3},
        {Enums::GameVersion::RSE, Enums::GameVersion::SA, "Sapphire", 3},
        {Enums::GameVersion::RSE, Enums::GameVersion::EM, "Emerald", 3},
        {Enums::GameVersion::FRLG, Enums::GameVersion::FR, "FireRed", 3},
        {Enums::GameVersion::FRLG, Enums::GameVersion::LG, "LeafGreen", 3},

        // Gen 4
        {Enums::GameVersion::DP, Enums::GameVersion::D, "Diamond", 4},
        {Enums::GameVersion::DP, Enums::GameVersion::P, "Pearl", 4},
        {Enums::GameVersion::PT, Enums::GameVersion::Pt, "Platinum", 4},
        {Enums::GameVersion::HGSS, Enums::GameVersion::HG, "HeartGold", 4},
        {Enums::GameVersion::HGSS, Enums::GameVersion::SS, "SoulSilver", 4},

        // Gen 5
        {Enums::GameVersion::BW, Enums::GameVersion::B, "Black", 5},
        {Enums::GameVersion::BW, Enums::GameVersion::W, "White", 5},
        {Enums::GameVersion::B2W2, Enums::GameVersion::B2, "Black 2", 5},
        {Enums::GameVersion::B2W2, Enums::GameVersion::W2, "White 2", 5},

        // Gen 6
        {Enums::GameVersion::XY, Enums::GameVersion::X, "X", 6},
        {Enums::GameVersion::XY, Enums::GameVersion::Y, "Y", 6},
        {Enums::GameVersion::ORAS, Enums::GameVersion::OR, "Omega Ruby", 6},
        {Enums::GameVersion::ORAS, Enums::GameVersion::AS, "Alpha Sapphire", 6},

        // Gen 7
        {Enums::GameVersion::SM, Enums::GameVersion::SN, "Sun", 7},
        {Enums::GameVersion::SM, Enums::GameVersion::MN, "Moon", 7},
        {Enums::GameVersion::USUM, Enums::GameVersion::US, "Ultra Sun", 7},
        {Enums::GameVersion::USUM, Enums::GameVersion::UM, "Ultra Moon", 7},
        {Enums::GameVersion::GG, Enums::GameVersion::GP, "Let's Go Pikachu", 7},
        {Enums::GameVersion::GG, Enums::GameVersion::GE, "Let's Go Eevee", 7},

        // Gen 8
        {Enums::GameVersion::SWSH, Enums::GameVersion::SW, "Sword", 8},
        {Enums::GameVersion::SWSH, Enums::GameVersion::SH, "Shield", 8},
        {Enums::GameVersion::BDSP, Enums::GameVersion::BD, "Brilliant Diamond", 8},
        {Enums::GameVersion::BDSP, Enums::GameVersion::SP, "Shining Pearl", 8},
        {Enums::GameVersion::PLA, Enums::GameVersion::PLA, "Legends: Arceus", 8},

        // Gen 9
        {Enums::GameVersion::SV, Enums::GameVersion::SL, "Scarlet", 9},
        {Enums::GameVersion::SV, Enums::GameVersion::VL, "Violet", 9},
        {Enums::GameVersion::ZA, Enums::GameVersion::ZA, "Legends: Z-A", 9}
    };

    std::string encounterLocationName(uint8_t originVer, uint16_t locId)
    {
        switch (locId)
        {
        case ENCOUNTER_LOCATION_SWSH_WILDAREA:
            return "Galar Wild Area";
        case ENCOUNTER_LOCATION_SWSH_WILDAREA_ISLE_OF_ARMOR:
            return "Isle of Armor";
        case ENCOUNTER_LOCATION_SWSH_WILDAREA_ALL:
            return "Crown Tundra / Wild Area";
        case ENCOUNTER_LOCATION_ROAMER3:
            return "Hoenn Roamer (Routes 101-138)";
        case ENCOUNTER_LOCATION_ROAMER4_SINNOH:
            return "Sinnoh Roamer";
        case ENCOUNTER_LOCATION_ROAMER4_JOHTO:
            return "Johto Roamer";
        case ENCOUNTER_LOCATION_ROAMER4_KANTO:
            return "Kanto Roamer";
        case ENCOUNTER_LOCATION_ROAMER5:
            return "Unova Roamer";
        case ENCOUNTER_LOCATION_ROAMER2:
            return "Johto Roamer";
        case ENCOUNTER_LOCATION_EVENT3:
            return "Special Distribution";
        default:
            break;
        }

        const char *name = Names::getMetLocationName(originVer, locId);
        if (name && name[0] != '\0' && std::strcmp(name, "(none)") != 0)
            return name;

        return "Route / Area #" + std::to_string(locId);
    }

    std::string getEncounterMethodDescription(EncounterKind kind, uint16_t locationId, uint8_t flags)
    {
        (void)locationId;
        if (flags & ENCOUNTER_FLAG_ALPHA)
            return "Alpha Pokémon";
        if (flags & ENCOUNTER_FLAG_EGG)
            return "Egg / Nursery Hatch";

        switch (kind)
        {
        case ENCOUNTER_KIND_WILD:
            return "Wild Encounter";
        case ENCOUNTER_KIND_STATIC:
            return "Static / Overworld";
        case ENCOUNTER_KIND_GIFT:
            return "In-Game Gift";
        case ENCOUNTER_KIND_TRADE:
            return "In-Game Trade";
        case ENCOUNTER_KIND_RAID:
            return "Tera / Max Raid";
        case ENCOUNTER_KIND_EVENT:
            return "Mystery Gift Event";
        default:
            return "Encounter";
        }
    }

    EncounterSpeciesResult queryEncountersForSpecies(uint16_t speciesId, uint8_t formId, int scopeFilter, Enums::GameVersion currentGame)
    {
        EncounterSpeciesResult result;
        result.species = speciesId;
        const char *sName = Trainer::getSpeciesName(speciesId);
        result.speciesName = sName ? sName : ("#" + std::to_string(speciesId));

        if (speciesId == 0 || speciesId > Pokemon::PERSONAL_MAX_SPECIES)
            return result;

        for (const auto &game : MAINLINE_GAMES)
        {
            // Scope Filtering
            if (scopeFilter >= 1 && scopeFilter <= 9 && game.generation != scopeFilter)
                continue;
            if (scopeFilter == 0)
            {
                // Current Game Only
                if (currentGame != Enums::GameVersion::Any &&
                    game.version != currentGame &&
                    game.group != currentGame)
                {
                    continue;
                }
            }

            const EncounterTable *table = getEncounterTable(game.group);
            if (!table || !table->speciesIndex)
                continue;

            if (speciesId + 1 >= table->speciesIndexLength)
                continue;

            // Breeding check
            if (table->breedable && (table->breedable[speciesId / 8] & (1 << (speciesId % 8))))
                result.hasBreeding = true;

            const uint16_t begin = table->speciesIndex[speciesId];
            const uint16_t end = table->speciesIndex[speciesId + 1];
            const uint8_t vBit = encounterVersionBit(game.version);

            for (uint16_t r = begin; r < end; ++r)
            {
                const auto &row = table->rows[r];

                // Check version bit
                if (vBit != 0 && (row.versions & vBit) == 0)
                    continue;

                // Check form if specified
                if (formId != 0 && row.form != ENCOUNTER_FORM_ANY && row.form != formId)
                    continue;

                EncounterRouteEntry entry;
                entry.gameVersion = game.version;
                entry.gameName = game.name;
                entry.generation = game.generation;
                entry.locationId = row.location;
                entry.locationName = encounterLocationName(static_cast<uint8_t>(game.version), row.location);
                entry.kind = static_cast<EncounterKind>(row.kind);
                entry.kindStr = getEncounterMethodDescription(entry.kind, row.location, row.flags);
                entry.minLevel = row.levelMin;
                entry.maxLevel = row.levelMax;
                entry.shiny = static_cast<EncounterShiny>(row.shiny);
                entry.shinyStr = (entry.shiny == ENCOUNTER_SHINY_NEVER) ? "Shiny Locked" :
                                 (entry.shiny == ENCOUNTER_SHINY_ALWAYS) ? "★ Guaranteed" : "★ Possible";
                entry.form = (row.form == ENCOUNTER_FORM_ANY) ? 0 : row.form;
                entry.flags = row.flags;
                entry.rawRow = row;

                // De-duplicate matching locations within same game
                bool duplicate = false;
                for (const auto &existing : result.routes)
                {
                    if (existing.gameVersion == entry.gameVersion &&
                        existing.locationId == entry.locationId &&
                        existing.kind == entry.kind &&
                        existing.minLevel == entry.minLevel &&
                        existing.maxLevel == entry.maxLevel &&
                        existing.form == entry.form)
                    {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate)
                    result.routes.push_back(std::move(entry));
            }
        }

        return result;
    }

    static uint8_t personalPresenceBit(Enums::GameVersion group)
    {
        switch (group)
        {
        case Enums::GameVersion::GG:
            return Pokemon::PERSONAL_GAME_GG;
        case Enums::GameVersion::SWSH:
            return Pokemon::PERSONAL_GAME_SWSH;
        case Enums::GameVersion::BDSP:
            return Pokemon::PERSONAL_GAME_BDSP;
        case Enums::GameVersion::PLA:
            return Pokemon::PERSONAL_GAME_PLA;
        case Enums::GameVersion::SV:
            return Pokemon::PERSONAL_GAME_SV;
        case Enums::GameVersion::ZA:
            return Pokemon::PERSONAL_GAME_ZA;
        default:
            return 0;
        }
    }

    static uint16_t highestSpeciesIdForGroup(Enums::GameVersion group)
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
            return 151;
        case Enums::GameVersion::GSC:
            return 251;
        case Enums::GameVersion::RSE:
        case Enums::GameVersion::FRLG:
            return 386;
        case Enums::GameVersion::DP:
        case Enums::GameVersion::PT:
        case Enums::GameVersion::HGSS:
            return 493;
        case Enums::GameVersion::BW:
        case Enums::GameVersion::B2W2:
            return 649;
        case Enums::GameVersion::XY:
        case Enums::GameVersion::ORAS:
            return 721;
        case Enums::GameVersion::SM:
        case Enums::GameVersion::USUM:
            return 807;
        case Enums::GameVersion::GG:
            return 153;
        default:
            return Pokemon::PERSONAL_MAX_SPECIES;
        }
    }

    bool canMaterializeInSave(const Trainer::Trainer &trainer, uint16_t speciesId, uint8_t formId)
    {
        if (speciesId == 0 || speciesId > Pokemon::PERSONAL_MAX_SPECIES)
            return false;

        const auto group = trainer.getGameGroup();
        const uint8_t bit = personalPresenceBit(group);
        if (bit != 0)
        {
            const auto &pInfo = Pokemon::getPersonalInfo(speciesId, formId);
            return (pInfo.presence & bit) != 0;
        }

        return speciesId <= highestSpeciesIdForGroup(group);
    }

    std::unique_ptr<Pokemon::Pokemon> materializeEncounter(Trainer::Trainer &trainer, uint64_t titleId, const EncounterRouteEntry &entry)
    {
        (void)titleId;
        if (!canMaterializeInSave(trainer, entry.rawRow.species, entry.form))
            return nullptr;

        auto pokemon = trainer.createBlankPokemon();
        if (!pokemon)
            return nullptr;

        uint16_t species = entry.rawRow.species;
        uint8_t form = entry.form;
        pokemon->setSpecies(species);
        pokemon->setForm(form);

        uint32_t ec = Utils::rand32();
        pokemon->setEncryptionConstant(ec);
        pokemon->setEncryptionConstant(Pokemon::correctEncryptionConstantForForm(species, form, ec));

        // Trainer info
        pokemon->setId32(trainer.ID32);
        pokemon->setOTName(Utils::utf8ToUtf16(trainer.trainerName));
        pokemon->setOTGender(trainer.trainerGender);
        pokemon->setLanguage(trainer.language());
        uint8_t originVer = static_cast<uint8_t>(entry.gameVersion);
        if (Enums::getGameGroup(entry.gameVersion) != trainer.getGameGroup())
            originVer = Enums::getGroupRepVersion(trainer.getGameGroup());
        pokemon->setOriginGame(originVer);

        // Generate PID / Shiny respecting encounter flags
        pokemon->setPID(Utils::rand32());
        if (entry.shiny == ENCOUNTER_SHINY_NEVER)
        {
            pokemon->setShiny(false, trainer.ID32);
        }
        else if (entry.shiny == ENCOUNTER_SHINY_ALWAYS)
        {
            pokemon->setShiny(true, trainer.ID32);
        }

        // Level & Met parameters
        uint8_t lvl = entry.minLevel > 0 ? entry.minLevel : 1;
        pokemon->setLevel(lvl);
        pokemon->setMetLevel(lvl);
        pokemon->setMetLocation(entry.locationId);

        std::time_t nowT = std::time(nullptr);
        if (const std::tm *localTime = std::localtime(&nowT))
        {
            pokemon->setMetYear(static_cast<uint8_t>(((localTime->tm_year + 1900) - 2000) & 0xFF));
            pokemon->setMetMonth(static_cast<uint8_t>(localTime->tm_mon + 1));
            pokemon->setMetDay(static_cast<uint8_t>(localTime->tm_mday));
        }

        // Nature & Gender
        uint8_t nature = (entry.rawRow.nature != ENCOUNTER_NATURE_ANY) ? entry.rawRow.nature : static_cast<uint8_t>(Utils::rand32() % 25);
        pokemon->setNature(nature);
        pokemon->setStatNature(nature);

        const Pokemon::PersonalInfo &pi = Pokemon::getPersonalInfo(species, form);
        uint8_t gender = (entry.rawRow.gender != ENCOUNTER_GENDER_ANY) ? entry.rawRow.gender :
                         (pi.genderRatio == 255) ? 2 :
                         (pi.genderRatio == 254) ? 1 :
                         (((Utils::rand32() & 0xFF) < pi.genderRatio) ? 1 : 0);
        pokemon->setGender(gender);

        // Ability
        const Pokemon::AbilitySlots slots = Pokemon::getAbilitySlots(species, form, trainer.getGameGroup());
        pokemon->setAbility(slots.slot[0]);
        if (!Enums::isGen3Group(trainer.getGameGroup()))
            pokemon->setAbilityNumber(1);

        pokemon->setFriendship(pi.baseFriendship);

        // Ball
        uint8_t ball = (entry.rawRow.fixedBall != 0) ? entry.rawRow.fixedBall :
                       (trainer.getGameGroup() == Enums::GameVersion::PLA ? 28 : 4); // Poké Ball default
        pokemon->setBall(ball);

        // Egg fields
        pokemon->setEgg(false);
        pokemon->setEggLocation(trainer.getGameGroup() == Enums::GameVersion::BDSP ? 0xFFFF : 0x0000);
        pokemon->setEggYear(0);
        pokemon->setEggMonth(0);
        pokemon->setEggDay(0);

        // Clear affixed ribbons
        if (const size_t affix = Conversion::affixedRibbonOffset(trainer.getGameGroup()); affix != 0)
        {
            auto dataBytes = pokemon->getData();
            if (dataBytes.size() >= 0x46)
                std::memset(dataBytes.data() + 0x34, 0, 0x46 - 0x34);
            if (dataBytes.size() > affix)
                dataBytes.data()[affix] = std::byte{Conversion::AFFIXED_RIBBON_NONE};
        }

        // Flawless IVs if encounter specifies
        if (entry.rawRow.flawlessIVs > 0)
        {
            int flawlessNeeded = std::min(6, static_cast<int>(entry.rawRow.flawlessIVs));
            for (int i = 0; i < flawlessNeeded; ++i)
                pokemon->setIV(i, 31);
        }

        // Nickname
        std::string nickname = pokemon->species();
        if (Enums::isGen3Group(pokemon->getGameGroup()))
        {
            for (char &c : nickname)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        pokemon->setNickname(Utils::utf8ToUtf16(nickname));

        // Default learnable move
        uint16_t defMove = 0;
        for (uint16_t moveIndex = 1; moveIndex < Names::getMoveCount(); ++moveIndex)
        {
            if (Pokemon::isLearnable(species, form, trainer.getGameGroup(), moveIndex) &&
                Names::isMovePresent(moveIndex, trainer.getGameGroup()))
            {
                defMove = moveIndex;
                break;
            }
        }
        if (defMove != 0)
        {
            pokemon->setMove(0, defMove);
            pokemon->setMovePP(0, Names::getMoveBasePP(defMove, trainer.getGameGroup()));
        }

        pokemon->recalculateStats();
        pokemon->refreshChecksum();
        return pokemon;
    }

    std::vector<GameChoice> gameChoicesForGroup(Enums::GameVersion group)
    {
        std::vector<GameChoice> out;
        for (const GameMetadata &game : MAINLINE_GAMES)
            if (game.group == group)
                out.push_back({static_cast<uint8_t>(game.version), game.name});
        return out;
    }

    const EncounterSpeciesResult &queryEncountersCached(uint16_t speciesId, uint8_t formId, int scopeFilter,
                                                        Enums::GameVersion currentGame)
    {
        static EncounterSpeciesResult cached;
        static uint16_t cachedSpecies = 0xFFFF;
        static uint8_t cachedForm = 0;
        static int cachedScope = -2;
        static Enums::GameVersion cachedGame = Enums::GameVersion::Invalid;
        if (cachedSpecies != speciesId || cachedForm != formId || cachedScope != scopeFilter || cachedGame != currentGame)
        {
            cached = queryEncountersForSpecies(speciesId, formId, scopeFilter, currentGame);
            cachedSpecies = speciesId;
            cachedForm = formId;
            cachedScope = scopeFilter;
            cachedGame = currentGame;
        }
        return cached;
    }
}
