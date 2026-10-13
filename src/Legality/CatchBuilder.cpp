#include "Legality/CatchBuilder.h"

#include <algorithm>
#include <ctime>

#include "Legality/EncounterLookup.h"
#include "Names/MoveInfo.h"
#include "Names/MoveNames.h"
#include "Names/MovePresence.h"
#include "Pokemon/AbilityInfo.h"
#include "Pokemon/EvolutionTable.h"
#include "Pokemon/FormInfo.h"
#include "Pokemon/LearnsetTable.h"
#include "Pokemon/LevelUpTable.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/Pokemon.h"
#include "Trainer/Trainer.h"
#include "Utils/HelperUtilities.h"

namespace Legality
{
    namespace
    {
        int generationOf(Enums::GameVersion group)
        {
            return Enums::getVersionGeneration(Enums::getGroupRepVersion(group));
        }

        // Rows a trainer cannot have met themselves: trades and events carry someone else's OT / IDs,
        // raids are seeded, and Egg gifts follow the hatch rules instead.
        bool catchable(const EncounterRow &row)
        {
            if (row.kind == ENCOUNTER_KIND_TRADE || row.kind == ENCOUNTER_KIND_EVENT || row.kind == ENCOUNTER_KIND_RAID)
                return false;
            if (row.flags & (ENCOUNTER_FLAG_EGG | ENCOUNTER_FLAG_FATEFUL))
                return false;
            // Region sentinels are not real met locations.
            return row.location < 0xFFF0 && row.location != ENCOUNTER_LOCATION_EVENT3;
        }

        const char *speciesName(uint16_t species) { return Trainer::getSpeciesName(species); }

        // The Gen 3/4 RNG (PKHeX LCRNG).
        struct Lcrng
        {
            uint32_t seed;
            uint16_t next()
            {
                seed = seed * 0x41C64E6Du + 0x6073u;
                return static_cast<uint16_t>(seed >> 16);
            }
        };

        uint8_t genderFromPid(uint32_t pid, uint8_t ratio)
        {
            if (ratio == 255)
                return 2;
            if (ratio == 254)
                return 1;
            if (ratio == 0)
                return 0;
            return (pid & 0xFF) < ratio ? 1 : 0;
        }

        bool shinyFromPid(uint32_t pid, uint32_t id32, int generation)
        {
            const uint32_t xorValue = (id32 & 0xFFFF) ^ (id32 >> 16) ^ (pid & 0xFFFF) ^ (pid >> 16);
            return xorValue < (generation >= 6 ? 16u : 8u);
        }
    }

    std::vector<CatchOption> catchOptions(const Trainer::Trainer &trainer, uint8_t version, uint16_t species,
                                          uint8_t form)
    {
        std::vector<CatchOption> options;
        const Enums::GameVersion group = trainer.getGameGroup();
        const EncounterTable *table = getEncounterTable(group);
        if (!table || species == 0)
            return options;
        const uint8_t versionBit = encounterVersionBit(static_cast<Enums::GameVersion>(version));

        // The species itself, then each earlier stage (nearest first).
        uint16_t chain[8];
        uint8_t forms[8];
        const int chainLength = Pokemon::getPreEvolutionChain(group, species, form, chain, forms, 7);
        struct Stage
        {
            uint16_t species;
            uint8_t form;
            int stepsBack;
        };
        std::vector<Stage> stages = {{species, form, 0}};
        for (int i = 0; i < chainLength; ++i)
            stages.push_back({chain[i], forms[i], i + 1});

        for (const Stage &stage : stages)
        {
            if (stage.species >= table->speciesIndexLength - 1)
                continue;
            const uint8_t floor = stage.stepsBack ? Pokemon::getEvolutionLevelFloor(group, species, form, stage.stepsBack) : 0;
            for (uint32_t r = table->speciesIndex[stage.species]; r < table->speciesIndex[stage.species + 1]; ++r)
            {
                const EncounterRow &row = table->rows[r];
                if (!catchable(row) || (versionBit != 0 && (row.versions & versionBit) == 0))
                    continue;
                if (row.form != ENCOUNTER_FORM_ANY && row.form != stage.form)
                    continue;
                CatchOption option;
                option.origin = stage.stepsBack ? CatchOption::Origin::Evolved : CatchOption::Origin::Encounter;
                option.row = row;
                option.metSpecies = stage.species;
                option.metForm = stage.form;
                option.minMetLevel = std::max<uint8_t>(1, row.levelMin);
                option.maxMetLevel = std::max(option.minMetLevel, row.levelMax);
                option.minLevel = std::max(option.minMetLevel, floor);
                option.metLocation = row.location;
                option.title = encounterLocationName(version, row.location);
                option.method = getEncounterMethodDescription(static_cast<EncounterKind>(row.kind), row.location, row.flags);
                option.shinyAllowed = row.shiny != ENCOUNTER_SHINY_NEVER;
                option.shinyForced = row.shiny == ENCOUNTER_SHINY_ALWAYS;
                std::string note;
                if (stage.stepsBack)
                    note = std::string("Caught as ") + speciesName(stage.species) +
                           (floor > 0 ? ", evolved by Lv. " + std::to_string(floor) : std::string(", evolved since"));
                if (!option.shinyAllowed)
                    note += std::string(note.empty() ? "" : "  ·  ") + "Never shiny";
                if (option.shinyForced)
                    note += std::string(note.empty() ? "" : "  ·  ") + "Always shiny";
                if (row.flawlessIVs)
                    note += std::string(note.empty() ? "" : "  ·  ") + std::to_string(row.flawlessIVs) + " perfect IVs";
                option.note = note;
                // The same place listed twice (slots of one table) is one choice: widen its levels.
                auto same = std::find_if(options.begin(), options.end(), [&](const CatchOption &o) {
                    return o.origin == option.origin && o.metSpecies == option.metSpecies && o.metLocation == option.metLocation &&
                           o.row.kind == option.row.kind && o.row.fixedBall == option.row.fixedBall &&
                           o.row.shiny == option.row.shiny && o.row.flawlessIVs == option.row.flawlessIVs;
                });
                if (same != options.end())
                {
                    same->minMetLevel = std::min(same->minMetLevel, option.minMetLevel);
                    same->maxMetLevel = std::max(same->maxMetLevel, option.maxMetLevel);
                    same->minLevel = std::max(same->minMetLevel, floor);
                    continue;
                }
                options.push_back(option);
            }
        }

        // Hatched: any stage the game lets you breed (Gen 4+; Gen 3 Egg PIDs are their own split method).
        const uint16_t base = chainLength > 0 ? chain[chainLength - 1] : species;
        const bool breedable = table->breedable && table->egg.hasBreeding && base < table->speciesIndexLength &&
                               (table->breedable[base >> 3] & (1u << (base & 7)));
        if (breedable && generationOf(group) >= 4)
        {
            CatchOption egg;
            egg.origin = CatchOption::Origin::Hatched;
            egg.metSpecies = base;
            egg.metForm = 0;
            egg.minMetLevel = egg.maxMetLevel = std::max<uint8_t>(1, table->egg.hatchLevel);
            const uint8_t floor = species != base ? Pokemon::getEvolutionLevelFloor(group, species, form, chainLength) : 0;
            egg.minLevel = std::max<uint8_t>(std::max(egg.minMetLevel, table->egg.hatchCurrentLevel), floor);
            egg.metLocation = table->egg.hatchLocation ? table->egg.hatchLocation
                              : table->egg.hatchLocationCount ? table->egg.hatchLocations[0]
                                                              : 0;
            egg.title = "Hatched from an Egg";
            egg.method = "Egg";
            egg.note = std::string("Hatched at ") + encounterLocationName(version, egg.metLocation) +
                       (species != base ? std::string(", from a ") + speciesName(base) + " Egg" : std::string());
            options.push_back(egg);
        }

        // Most natural first: as itself before evolved, wild before static and gift, then by level.
        std::stable_sort(options.begin(), options.end(), [](const CatchOption &a, const CatchOption &b) {
            if (a.origin != b.origin)
                return static_cast<int>(a.origin) < static_cast<int>(b.origin);
            if (a.row.kind != b.row.kind)
                return a.row.kind < b.row.kind;
            return a.minMetLevel < b.minMetLevel;
        });
        return options;
    }

    void applyCatch(Pokemon::Pokemon &pokemon, const Trainer::Trainer &trainer, uint8_t version, uint16_t species,
                    uint8_t form, const CatchOption &option, uint8_t metLevel, uint8_t level, bool shiny)
    {
        const Enums::GameVersion group = trainer.getGameGroup();
        const int generation = generationOf(group);
        const Pokemon::PersonalInfo &pi = Pokemon::getPersonalInfo(species, form);
        const Pokemon::AbilitySlots slots = Pokemon::getAbilitySlots(species, form, group);
        const bool hatched = option.origin == CatchOption::Origin::Hatched;
        const bool wild = !hatched && option.row.kind == ENCOUNTER_KIND_WILD;
        const uint8_t fixedNature = hatched ? ENCOUNTER_NATURE_ANY : option.row.nature;
        const uint8_t fixedGender = hatched ? ENCOUNTER_GENDER_ANY : option.row.gender;
        shiny = option.shinyForced || (shiny && option.shinyAllowed);

        metLevel = std::clamp(metLevel, option.minMetLevel, option.maxMetLevel);
        level = std::max<uint8_t>(std::max(level, metLevel), option.minLevel);
        level = std::min<uint8_t>(level, 100);

        uint32_t pid = 0;
        uint8_t ivs[6] = {0, 0, 0, 0, 0, 0}; // HP ATK DEF SPE SPA SPD
        auto genderOk = [&](uint32_t candidate) {
            return fixedGender == ENCOUNTER_GENDER_ANY || genderFromPid(candidate, pi.genderRatio) == fixedGender;
        };

        if ((generation == 3 || generation == 4) && !hatched)
        {
            // Gen 3/4: nature, gender, ability and IVs all come out of one LCRNG chain. Wild slots run
            // slot -> level -> nature -> PID loop (until PID % 25 is that nature) -> IVs: Method 1 in
            // Gen 3, Method J in Diamond/Pearl/Platinum (nature = rand / 0xA3E), Method K in HG/SS
            // (nature = rand % 25). Statics and gifts are plain Method 1: PID then IVs.
            const bool methodJ = group == Enums::GameVersion::DP || group == Enums::GameVersion::PT;
            const int levelSpan = option.maxMetLevel - option.minMetLevel + 1;
            for (int attempt = 0; attempt < 4000000; ++attempt)
            {
                Lcrng rng{Utils::rand32()};
                uint32_t candidate;
                if (wild)
                {
                    rng.next();                                   // slot
                    const uint16_t levelRand = rng.next();        // level
                    if (levelSpan > 1 && option.minMetLevel + levelRand % levelSpan != metLevel)
                        continue;
                    const uint16_t natureRand = rng.next();
                    const uint8_t nature = methodJ ? static_cast<uint8_t>(natureRand / 0xA3E) : static_cast<uint8_t>(natureRand % 25);
                    if (nature > 24 || (fixedNature != ENCOUNTER_NATURE_ANY && nature != fixedNature))
                        continue;
                    do
                    {
                        const uint16_t low = rng.next(), high = rng.next();
                        candidate = (static_cast<uint32_t>(high) << 16) | low;
                    } while (candidate % 25 != nature);
                }
                else
                {
                    const uint16_t low = rng.next(), high = rng.next();
                    candidate = (static_cast<uint32_t>(high) << 16) | low;
                    if (fixedNature != ENCOUNTER_NATURE_ANY && candidate % 25 != fixedNature)
                        continue;
                }
                if (!genderOk(candidate) || shinyFromPid(candidate, trainer.ID32, generation) != shiny)
                    continue;
                const uint16_t iv1 = rng.next(), iv2 = rng.next();
                ivs[0] = iv1 & 31;
                ivs[1] = (iv1 >> 5) & 31;
                ivs[2] = (iv1 >> 10) & 31;
                ivs[3] = iv2 & 31;
                ivs[4] = (iv2 >> 5) & 31;
                ivs[5] = (iv2 >> 10) & 31;
                pid = candidate;
                break;
            }
        }
        else
        {
            // Gen 5+: PID and IVs are independent. Gen 5 still reads gender from the PID's low byte and
            // the ability from bit 16, and keeps PID bit 31 tied to the trainer's id parity.
            const int abilitySlot = static_cast<int>(Utils::rand32() & 1);
            for (int attempt = 0; attempt < 4000000; ++attempt)
            {
                uint32_t candidate = Utils::rand32();
                if (generation <= 5)
                {
                    candidate = (candidate & ~0x10000u) | (static_cast<uint32_t>(abilitySlot) << 16);
                    const uint32_t parity = (candidate & 1) ^ (trainer.ID32 & 1) ^ ((trainer.ID32 >> 16) & 1);
                    candidate = (candidate & 0x7FFFFFFFu) | (parity << 31);
                    if (!genderOk(candidate))
                        continue;
                }
                if (generation < 6 && shinyFromPid(candidate, trainer.ID32, generation) != shiny)
                    continue;
                pid = candidate;
                break;
            }
            for (uint8_t &iv : ivs)
                iv = static_cast<uint8_t>(Utils::rand32() % 32);
        }

        // Guaranteed perfect IVs (legendaries, Max Raid style statics): that many random stats at 31.
        if (!hatched && option.row.flawlessIVs > 0)
        {
            int order[6] = {0, 1, 2, 3, 4, 5};
            for (int i = 5; i > 0; --i)
                std::swap(order[i], order[Utils::rand32() % (i + 1)]);
            for (int i = 0; i < std::min<int>(6, option.row.flawlessIVs); ++i)
                ivs[order[i]] = 31;
        }

        pokemon.setSpecies(species);
        pokemon.setForm(form);
        if (generation >= 5)
            pokemon.setEncryptionConstant(Pokemon::correctEncryptionConstantForForm(species, form, Utils::rand32()));
        pokemon.setPID(pid);
        for (int i = 0; i < 6; ++i)
            pokemon.setIV(i, ivs[i]);
        for (int i = 0; i < 6; ++i)
            pokemon.setEV(i, 0);

        // Nature, gender, ability -- written so they agree with the PID instead of re-rolling it.
        if (generation >= 5)
        {
            uint8_t nature = fixedNature != ENCOUNTER_NATURE_ANY ? fixedNature : static_cast<uint8_t>(Utils::rand32() % 25);
            pokemon.setNature(nature);
            pokemon.setStatNature(nature);
            uint8_t gender = generation == 5 ? genderFromPid(pid, pi.genderRatio)
                             : fixedGender != ENCOUNTER_GENDER_ANY ? fixedGender
                                                                   : genderFromPid(Utils::rand32(), pi.genderRatio);
            pokemon.setGender(gender);
            const int slot = generation == 5 ? static_cast<int>((pid >> 16) & 1) : static_cast<int>(Utils::rand32() & 1);
            const uint16_t ability = slots.slot[slot] ? slots.slot[slot] : slots.slot[0];
            pokemon.setAbility(ability);
            pokemon.setAbilityNumber(static_cast<uint8_t>(1 << slot));
            if (generation >= 6)
                pokemon.setShiny(shiny, trainer.ID32);
        }
        else
        {
            const int slot = static_cast<int>(pid & 1);
            if (generation == 4)
            {
                pokemon.setGender(genderFromPid(pid, pi.genderRatio)); // a stored flag in Gen 4
                pokemon.setAbility(slots.slot[slot] ? slots.slot[slot] : slots.slot[0]);
            }
            else
                pokemon.setAbilityNumber(static_cast<uint8_t>(slot + 1)); // Gen 3: the IV-word bit; PID already agrees
        }

        // Level, met data, ball.
        pokemon.setLevel(level);
        pokemon.setMetLevel(hatched && generation <= 4 ? 0 : metLevel);
        pokemon.setMetLocation(option.metLocation);
        pokemon.setOriginGame(version);
        pokemon.setFatefulEncounter(false);
        uint8_t ball = (!hatched && option.row.fixedBall) ? option.row.fixedBall : (group == Enums::GameVersion::PLA ? 28 : 4);
        pokemon.setBall(ball);
        std::time_t now = std::time(nullptr);
        const std::tm *today = std::localtime(&now);
        if (hatched)
        {
            pokemon.setEggLocation(option.row.eggLocation ? option.row.eggLocation : getEncounterTable(group)->egg.eggLocation);
            if (today)
            {
                pokemon.setEggYear(static_cast<uint8_t>(today->tm_year + 1900 - 2000));
                pokemon.setEggMonth(static_cast<uint8_t>(today->tm_mon + 1));
                pokemon.setEggDay(static_cast<uint8_t>(today->tm_mday));
            }
        }
        if (today)
        {
            pokemon.setMetYear(static_cast<uint8_t>(today->tm_year + 1900 - 2000));
            pokemon.setMetMonth(static_cast<uint8_t>(today->tm_mon + 1));
            pokemon.setMetDay(static_cast<uint8_t>(today->tm_mday));
        }

        // Moves: what one of this level knows by levelling (any level-up move at or below its level is
        // legal to know), falling back to the first learnable move where PKSE has no level-up table.
        uint16_t moves[4] = {0, 0, 0, 0};
        int moveCount = Pokemon::getWildMoveset(group, species, form, level, moves);
        if (moveCount == 0)
            moveCount = Pokemon::getWildMoveset(group, option.metSpecies, option.metForm, metLevel, moves);
        if (moveCount > 0)
        {
            for (int i = 0; i < 4; ++i)
            {
                pokemon.setMove(i, i < moveCount ? moves[i] : 0);
                pokemon.setMovePP(i, i < moveCount ? Names::getMoveBasePP(moves[i], group) : 0);
                pokemon.setMovePPUps(i, 0);
            }
        }
        pokemon.setFriendship(hatched ? 120 : pi.baseFriendship);
        pokemon.recalculateStats();
        pokemon.refreshChecksum();
    }
}
