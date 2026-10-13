#ifndef LEGALITY_ENCOUNTER_LOOKUP_H
#define LEGALITY_ENCOUNTER_LOOKUP_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "Enums/GameVersion.h"
#include "Legality/EncounterTable.h"

namespace Trainer
{
    class Trainer;
}

namespace Pokemon
{
    class Pokemon;
}

namespace Legality
{
    struct EncounterRouteEntry
    {
        Enums::GameVersion gameVersion = Enums::GameVersion::Any;
        std::string gameName;
        int generation = 0;
        uint16_t locationId = 0;
        std::string locationName;
        EncounterKind kind = ENCOUNTER_KIND_WILD;
        std::string kindStr;
        uint8_t minLevel = 1;
        uint8_t maxLevel = 1;
        EncounterShiny shiny = ENCOUNTER_SHINY_RANDOM;
        std::string shinyStr;
        uint8_t form = 0;
        uint8_t flags = 0;
        EncounterRow rawRow{};
    };

    struct EncounterSpeciesResult
    {
        uint16_t species = 0;
        std::string speciesName;
        bool hasBreeding = false;
        std::vector<EncounterRouteEntry> routes;
    };

    /**
     * Queries all 36 mainline games across Gen 1-9 to find where this species can be caught.
     * @param speciesId National Dex ID (1..1025)
     * @param formId Form ID (0 for default)
     * @param scopeFilter -1 = All Games, 0 = Current Save Game only, 1..9 = Specific Gen only
     * @param currentGame The game version of the current active save
     */
    EncounterSpeciesResult queryEncountersForSpecies(uint16_t speciesId, uint8_t formId = 0, int scopeFilter = -1, Enums::GameVersion currentGame = Enums::GameVersion::Any);

    /**
     * Formats a friendly encounter method description (e.g. "Wild (Grass)", "Surfing", "Fishing", "Static", "Tera Raid", "Gift").
     */
    std::string getEncounterMethodDescription(EncounterKind kind, uint16_t locationId, uint8_t flags);

    /**
     * Checks if a species can exist in the active save file.
     */
    bool canMaterializeInSave(const Trainer::Trainer &trainer, uint16_t speciesId, uint8_t formId);

    /**
     * Materializes the encounter into a legal Pokemon instance placed in the save.
     */
    std::unique_ptr<Pokemon::Pokemon> materializeEncounter(Trainer::Trainer &trainer, uint64_t titleId, const EncounterRouteEntry &entry);
}

#endif // LEGALITY_ENCOUNTER_LOOKUP_H
