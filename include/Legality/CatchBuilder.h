#ifndef LEGALITY_CATCH_BUILDER_H
#define LEGALITY_CATCH_BUILDER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
    /**
     * One way the open game could have given you a Pokémon: caught wild / met as a static or gift,
     * caught as an earlier stage and evolved since, or hatched from an Egg.
     *
     * The creator lists these for the chosen species and builds the Pokémon from the one picked, the
     * way the game would have: met there, at a level that encounter allows, in a ball it allows, with
     * the moves a wild one of that level knows, and -- in Gen 3 and 4, where the games tie nature,
     * gender, ability and IVs to one RNG chain -- a PID and IVs generated through that same chain.
     */
    struct CatchOption
    {
        enum class Origin : uint8_t
        {
            Encounter, // caught / met as the species itself
            Evolved,   // caught as an earlier stage, evolved since
            Hatched    // hatched from an Egg
        };
        Origin origin = Origin::Encounter;
        EncounterRow row{};          // the encounter (unset for Hatched)
        uint16_t metSpecies = 0;     // what was met: the species itself or the earlier stage
        uint8_t metForm = 0;
        uint8_t minMetLevel = 1, maxMetLevel = 1;
        uint8_t minLevel = 1;        // the lowest level it can be now (evolution floor, hatch level)
        uint16_t metLocation = 0;
        std::string title;           // "Route 101", "Hatched from an Egg"
        std::string method;          // "Wild (Grass)", "Static", "Gift"...
        std::string note;            // "Evolved from Treecko", "Shiny locked", "Comes in a Master Ball"
        bool shinyAllowed = true, shinyForced = false;
    };

    /// The ways the open save's game could have produced (species, form), most natural first.
    /// `version` is the save's exact game (version exclusives are filtered out).
    std::vector<CatchOption> catchOptions(const Trainer::Trainer &trainer, uint8_t version, uint16_t species,
                                          uint8_t form);

    /// Makes a freshly created Pokémon (the creator's format-correct default) the one `option` gives:
    /// met at `metLevel` (clamped to the option), now at `level` (raised to the option's floor), shiny
    /// if asked and allowed. Rewrites PID / IVs / nature / gender / ability / moves / met data / ball.
    /// False (Pokemon untouched beyond the default) when no PID satisfies the encounter's rules.
    bool applyCatch(Pokemon::Pokemon &pokemon, const Trainer::Trainer &trainer, uint8_t version, uint16_t species,
                    uint8_t form, const CatchOption &option, uint8_t metLevel, uint8_t level, bool shiny);

    /// The location's name in `version`'s own location table (sentinels named too).
    std::string encounterLocationName(uint8_t version, uint16_t location);
}

#endif
