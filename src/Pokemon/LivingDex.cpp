#include "Pokemon/LivingDex.h"
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"
#include "Utils/StringHelpers.h"

namespace Pokemon
{
    int getSpeciesGeneration(uint16_t speciesId)
    {
        for (const auto &r : GEN_RANGES)
        {
            if (speciesId >= r.first && speciesId <= r.last)
                return r.generation;
        }
        return 0;
    }

    uint16_t getMaxSpeciesForGame(Enums::GameVersion group)
    {
        switch (group)
        {
        case Enums::GameVersion::RBY:
            return 151;
        case Enums::GameVersion::GSC:
            return 251;
        case Enums::GameVersion::FRLG:
        case Enums::GameVersion::RSE:
            return 386;
        case Enums::GameVersion::DP:
        case Enums::GameVersion::PT:
        case Enums::GameVersion::HGSS:
        case Enums::GameVersion::BDSP:
            return 493;
        case Enums::GameVersion::BW:
        case Enums::GameVersion::B2W2:
            return 649;
        case Enums::GameVersion::XY:
        case Enums::GameVersion::ORAS:
            return 721;
        case Enums::GameVersion::SM:
            return 802;
        case Enums::GameVersion::USUM:
            return 807;
        case Enums::GameVersion::GG:
            return 809;
        case Enums::GameVersion::SWSH:
            return 898;
        case Enums::GameVersion::PLA:
            return 905;
        case Enums::GameVersion::ZA:
            return 1000;
        case Enums::GameVersion::SV:
        default:
            return 1025;
        }
    }

    LivingDexProgress scanCollection(
        const Trainer::Trainer &trainer,
        const Trainer::Bank *bank,
        std::array<SpeciesRecord, LIVING_DEX_MAX_SPECIES + 1> &outRecords)
    {
        for (uint16_t id = 1; id <= LIVING_DEX_MAX_SPECIES; ++id)
        {
            outRecords[id].speciesId = id;
            outRecords[id].owned = false;
            outRecords[id].ownedInSave = false;
            outRecords[id].ownedInBank = false;
            outRecords[id].shiny = false;
            outRecords[id].locations.clear();
        }

        // 1. Scan active party
        for (size_t i = 0; i < trainer.party.size(); ++i)
        {
            const auto &p = trainer.party[i];
            if (!p || p->isEgg() || p->speciesID() == 0 || p->speciesID() > LIVING_DEX_MAX_SPECIES)
                continue;
            uint16_t id = p->speciesID();
            bool isShiny = p->isShiny(p->id32(), p->species());
            outRecords[id].owned = true;
            outRecords[id].ownedInSave = true;
            if (isShiny)
                outRecords[id].shiny = true;

            PokemonLocation loc;
            loc.inBank = false;
            loc.box = -1;
            loc.slot = static_cast<int>(i);
            loc.boxName = "Party";
            loc.level = p->level();
            loc.isShiny = isShiny;
            loc.nickname = Utils::utf16ToUtf8(p->nickname());
            outRecords[id].locations.push_back(std::move(loc));
        }

        // 2. Scan active save boxes
        for (size_t b = 0; b < trainer.boxes.size(); ++b)
        {
            const auto &box = trainer.boxes[b];
            std::string boxName = (b < trainer.boxNames.size() && !trainer.boxNames[b].empty())
                                      ? trainer.boxNames[b]
                                      : ("Box " + std::to_string(b + 1));
            for (size_t s = 0; s < box.size(); ++s)
            {
                const auto &p = box[s];
                if (!p || p->isEgg() || p->speciesID() == 0 || p->speciesID() > LIVING_DEX_MAX_SPECIES)
                    continue;
                uint16_t id = p->speciesID();
                bool isShiny = p->isShiny(p->id32(), p->species());
                outRecords[id].owned = true;
                outRecords[id].ownedInSave = true;
                if (isShiny)
                    outRecords[id].shiny = true;

                PokemonLocation loc;
                loc.inBank = false;
                loc.box = static_cast<int>(b);
                loc.slot = static_cast<int>(s);
                loc.boxName = boxName;
                loc.level = p->level();
                loc.isShiny = isShiny;
                loc.nickname = Utils::utf16ToUtf8(p->nickname());
                outRecords[id].locations.push_back(std::move(loc));
            }
        }

        // 3. Scan unified bank boxes
        if (bank)
        {
            for (size_t b = 0; b < bank->boxes.size(); ++b)
            {
                const auto &box = bank->boxes[b];
                std::string boxName = bank->boxDisplayName(b);
                for (size_t s = 0; s < box.size(); ++s)
                {
                    const auto &p = box[s];
                    if (!p || p->isEgg() || p->speciesID() == 0 || p->speciesID() > LIVING_DEX_MAX_SPECIES)
                        continue;
                    uint16_t id = p->speciesID();
                    bool isShiny = p->isShiny(p->id32(), p->species());
                    outRecords[id].owned = true;
                    outRecords[id].ownedInBank = true;
                    if (isShiny)
                        outRecords[id].shiny = true;

                    PokemonLocation loc;
                    loc.inBank = true;
                    loc.box = static_cast<int>(b);
                    loc.slot = static_cast<int>(s);
                    loc.boxName = boxName;
                    loc.level = p->level();
                    loc.isShiny = isShiny;
                    loc.nickname = Utils::utf16ToUtf8(p->nickname());
                    outRecords[id].locations.push_back(std::move(loc));
                }
            }
        }

        // 4. Compute progress
        LivingDexProgress progress{};
        progress.totalSpecies = LIVING_DEX_MAX_SPECIES;
        for (uint16_t id = 1; id <= LIVING_DEX_MAX_SPECIES; ++id)
        {
            if (outRecords[id].owned)
                ++progress.ownedCount;
            if (outRecords[id].shiny)
                ++progress.shinyCount;
            if (outRecords[id].ownedInSave)
                ++progress.saveOwnedCount;
            if (outRecords[id].ownedInBank)
                ++progress.bankOwnedCount;
        }

        for (size_t g = 0; g < GEN_RANGES.size(); ++g)
        {
            const auto &r = GEN_RANGES[g];
            GenProgress gp;
            gp.generation = r.generation;
            gp.first = r.first;
            gp.last = r.last;
            gp.total = static_cast<int>(r.last - r.first + 1);
            gp.owned = 0;
            gp.shiny = 0;
            for (uint16_t id = r.first; id <= r.last; ++id)
            {
                if (outRecords[id].owned)
                    ++gp.owned;
                if (outRecords[id].shiny)
                    ++gp.shiny;
            }
            progress.genSegments[g] = gp;
        }

        return progress;
    }
}
