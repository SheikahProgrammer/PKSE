#ifndef POKEMON_LIVING_DEX_H
#define POKEMON_LIVING_DEX_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Enums/GameVersion.h"

namespace Trainer
{
    class Trainer;
    class Bank;
}

namespace Pokemon
{
    inline constexpr uint16_t LIVING_DEX_MAX_SPECIES = 1025;

    struct GenRange
    {
        int generation;
        uint16_t first;
        uint16_t last;
    };

    inline constexpr std::array<GenRange, 9> GEN_RANGES = {{
        {1, 1, 151},
        {2, 152, 251},
        {3, 252, 386},
        {4, 387, 493},
        {5, 494, 649},
        {6, 650, 721},
        {7, 722, 809},
        {8, 810, 905},
        {9, 906, 1025}
    }};

    struct PokemonLocation
    {
        bool inBank = false;
        int box = 0;           // -1 for party, 0..N for box index
        int slot = 0;          // 0..29
        std::string boxName;   // "Party", "Box 1", "Bank 3", etc.
        uint8_t level = 1;
        bool isShiny = false;
        std::string nickname;
    };

    struct SpeciesRecord
    {
        uint16_t speciesId = 0;
        bool owned = false;
        bool ownedInSave = false;
        bool ownedInBank = false;
        bool shiny = false;
        std::vector<PokemonLocation> locations;
    };

    struct GenProgress
    {
        int generation = 1;
        uint16_t first = 1;
        uint16_t last = 151;
        int owned = 0;
        int shiny = 0;
        int total = 151;
    };

    struct LivingDexProgress
    {
        int totalSpecies = LIVING_DEX_MAX_SPECIES;
        int ownedCount = 0;
        int shinyCount = 0;
        int saveOwnedCount = 0;
        int bankOwnedCount = 0;
        std::array<GenProgress, 9> genSegments{};
    };

    /**
     * Determines which generation (1..9) introduced the given species ID.
     * Returns 0 for invalid/out-of-range species.
     */
    int getSpeciesGeneration(uint16_t speciesId);

    /**
     * Returns the maximum national dex species ID that can exist natively in the given game group.
     */
    uint16_t getMaxSpeciesForGame(Enums::GameVersion group);

    /**
     * Scans the active save file (party + all boxes) and the unified PKSE Bank (all boxes).
     * Populates outRecords with exact species presence and locations, and computes overall/gen progress.
     */
    LivingDexProgress scanCollection(
        const Trainer::Trainer &trainer,
        const Trainer::Bank *bank,
        std::array<SpeciesRecord, LIVING_DEX_MAX_SPECIES + 1> &outRecords);
}

#endif // POKEMON_LIVING_DEX_H
