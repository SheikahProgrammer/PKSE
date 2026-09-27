#include "Pokemon/Evolution.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Enums/GameVersion.h"
#include "Enums/LanguageID.h"
#include "Names/FormNames.h"
#include "Names/ItemNames.h"
#include "Names/MoveNames.h"
#include "Names/NameLanguage.h"
#include "Pokemon/AbilityInfo.h"
#include "Pokemon/Experience.h"
#include "Pokemon/Pokemon.h"
#include "Utils/StringHelpers.h"

namespace Trainer
{
    const char *getSpeciesName(uint16_t speciesId);
    const char *getAbilityName(uint16_t abilityId);
}

namespace Names
{
    // The species table read in a NAMED language rather than the display one, forward-declared the
    // way Conversion/Convert.cpp declares it: the table is declared in Trainer/Trainer.h, and a
    // Pokemon-layer source must not include the trainer layer to reach it.
    const char *getSpeciesNameLocalized(uint16_t speciesId, size_t languageIndex);
}

namespace Pokemon
{

    namespace
    {

        /**
         * True where the game stores a species name in UPPER CASE, which every game before
         * Generation 5 does -- an un-nicknamed Kadabra is KADABRA on a Game Boy, a GBA and a DS.
         * Japanese and Korean are excluded because their scripts have no case, exactly as PKHeX's
         * SpeciesName.GetSpeciesName1234 excludes them.
         */
        bool storesUpperCaseSpeciesNames(Enums::GameVersion gameGroup, uint8_t languageId)
        {
            if (Enums::getVersionGeneration(Enums::getGroupRepVersion(gameGroup)) > 4)
            {
                return false;
            }
            return languageId != static_cast<uint8_t>(Enums::LanguageID::Japanese) &&
                   languageId != static_cast<uint8_t>(Enums::LanguageID::Korean);
        }

        /** A species name as the record's own game and language would have stored it. */
        std::u16string speciesNameAsStored(uint16_t speciesId, Enums::GameVersion gameGroup, uint8_t languageId)
        {
            const size_t languageIndex = Names::languageIndexFor(static_cast<Enums::LanguageID>(languageId));
            const std::u16string speciesName =
                Utils::utf8ToUtf16(std::string(Names::getSpeciesNameLocalized(speciesId, languageIndex)));
            if (storesUpperCaseSpeciesNames(gameGroup, languageId))
            {
                return Utils::upperCaseLatinLetters(speciesName);
            }
            return speciesName;
        }

        /**
         * True when the nickname field is holding the species' own name rather than a name the
         * player chose -- the only case in which an evolution may rewrite it.
         *
         * TWO TESTS, AND BOTH ARE NEEDED. The flag alone is wrong for the three formats that do not
         * have one: Pokemon3FRLG and Pokemon3RSE never override isNicknamed(), so it answers false
         * for every Gen 3 record, and renaming on that alone would destroy a real Gen 3 nickname.
         * The string test alone is wrong for the formats that DO have a flag, where a Pokemon
         * deliberately nicknamed "Kadabra" keeps that name through an evolution.
         *
         * The comparison uppercases and compares, rather than going through normalizeGameBoyName():
         * that helper also strips every character it cannot uppercase, so two different Japanese
         * names both reduce to "" and compare equal. Gen 1 and Gen 2 need no such care here anyway
         * -- their isNicknamed() already does this comparison, and already answers true for a
         * Japanese record it has no table to check.
         */
        bool nicknameIsSpeciesName(const Pokemon &pokemon)
        {
            if (pokemon.isNicknamed())
            {
                return false;
            }
            const std::u16string storedNickname = Utils::upperCaseLatinLetters(pokemon.nickname());
            if (storedNickname.empty())
            {
                return false;
            }
            const std::u16string currentSpeciesName = Utils::upperCaseLatinLetters(
                speciesNameAsStored(pokemon.speciesID(), pokemon.getGameGroup(), pokemon.language()));
            return storedNickname == currentSpeciesName;
        }

        /**
         * Moves the ability to the same SLOT of the new species, so a hidden-ability Pokemon stays
         * hidden. The slot index, not the ability id, is what survives an evolution -- the ids are
         * per species and almost never match. Mirrors what the species picker does.
         */
        void carryAbilitySlot(Pokemon &pokemon, uint8_t previousAbilityNumber, uint16_t destinationSpeciesId,
                              uint8_t destinationFormId, Enums::GameVersion gameGroup)
        {
            if (!pokemon.hasAbility())
            {
                return;
            }
            const AbilitySlots slots = getAbilitySlots(destinationSpeciesId, destinationFormId, gameGroup);
            // abilityNumber is a BITMASK in the modern formats: 1 = slot 1, 2 = slot 2, 4 = hidden.
            int wantedSlot = (previousAbilityNumber == 4) ? 2 : (previousAbilityNumber == 2) ? 1
                                                                                            : 0;
            if (wantedSlot >= slots.count || slots.slot[wantedSlot] == 0)
            {
                wantedSlot = 0;
            }
            if (slots.slot[wantedSlot] == 0)
            {
                return;
            }
            pokemon.setAbility(slots.slot[wantedSlot]);
            if (!Enums::isGen3Group(gameGroup))
            {
                pokemon.setAbilityNumber(static_cast<uint8_t>(wantedSlot == 2 ? 4 : wantedSlot + 1));
            }
        }
    }

    TradeEvolutionOffer getTradeEvolutionOffer(const Pokemon &pokemon)
    {
        TradeEvolutionOffer offer;
        if (pokemon.speciesID() == 0)
        {
            return offer;
        }
        offer.candidateCount = getTradeEvolutions(pokemon.getGameGroup(), pokemon.speciesID(), pokemon.form(),
                                                  offer.candidates, TRADE_EVOLUTIONS_MAX_PER_SPECIES);
        // WHICH ROW THE ITEM NAMES, not whether the evolution may happen. A held item of 0 is
        // "holding nothing", so it must never match a row -- a HeldItem row with no item id would
        // be a table bug, and this is what stops it becoming a wrong evolution.
        const uint16_t heldItemId = pokemon.hasHeldItem() ? pokemon.heldItem() : 0;
        for (int candidateIndex = 0; heldItemId != 0 && candidateIndex < offer.candidateCount; ++candidateIndex)
        {
            const TradeEvolution &candidate = offer.candidates[candidateIndex];
            if (candidate.trigger == TradeEvolutionTrigger::HeldItem && candidate.requiredItemId == heldItemId)
            {
                offer.heldItemIndex = candidateIndex;
                break;
            }
        }
        return offer;
    }

    bool applyTradeEvolution(Pokemon &pokemon, int candidateIndex)
    {
        const TradeEvolutionOffer offer = getTradeEvolutionOffer(pokemon);
        if (candidateIndex < 0 || candidateIndex >= offer.candidateCount)
        {
            return false;
        }
        const TradeEvolution &evolution = offer.candidates[candidateIndex];
        const Enums::GameVersion gameGroup = pokemon.getGameGroup();

        // Asked BEFORE the species changes: the question is whether the nickname is the name of
        // what this Pokemon is now, and a moment later it will not be.
        const bool renameNickname = nicknameIsSpeciesName(pokemon);
        const uint8_t previousAbilityNumber = pokemon.abilityNumber();

        pokemon.setSpecies(evolution.destinationSpeciesId); // first: drives the base-stat lookup
        pokemon.setForm(evolution.destinationFormId);
        carryAbilitySlot(pokemon, previousAbilityNumber, evolution.destinationSpeciesId,
                         evolution.destinationFormId, gameGroup);

        // The item is destroyed by the evolution in every game that has one of these; it does not
        // go back to the bag. ONLY THE ITEM THE ROW ASKS FOR: the evolution runs whether or not it
        // is held, so clearing the field unconditionally would destroy an unrelated item a Scyther
        // happened to be carrying.
        if (evolution.trigger == TradeEvolutionTrigger::HeldItem && pokemon.hasHeldItem() &&
            pokemon.heldItem() == evolution.requiredItemId)
        {
            pokemon.setHeldItem(0);
        }

        if (renameNickname)
        {
            pokemon.setNickname(
                speciesNameAsStored(evolution.destinationSpeciesId, gameGroup, pokemon.language()));
        }

        // EXP and level are deliberately untouched -- see the header. recalculateStats() reads the
        // level that is already there and refills the battle-stat tail from the new base stats.
        pokemon.recalculateStats();
        pokemon.refreshChecksum();
        return true;
    }

    std::string getEvolutionRequirementText(Enums::GameVersion group, const ForwardEvolution &evo, uint8_t currentLevel)
    {
        switch (evo.method)
        {
            case 1: return "Level up with high friendship";
            case 2: return "Level up with high friendship by day";
            case 3: return "Level up with high friendship at night";
            case 4:
                return (evo.level > 1) ? ("Level " + std::to_string(evo.level)) : "Level up";
            case 5: return "Trade";
            case 6:
                return "Trade holding " + std::string(Names::getItemNameFor(group, evo.argument));
            case 7:
                return (evo.speciesId == 588) ? "Trade with Shelmet" : "Trade with Karrablast";
            case 8:
                return "Use " + std::string(Names::getItemNameFor(group, evo.argument));
            case 9:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level) + " w/ ") : "Level up w/ ") + "Attack > Defense";
            case 10:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level) + " w/ ") : "Level up w/ ") + "Attack = Defense";
            case 11:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level) + " w/ ") : "Level up w/ ") + "Defense > Attack";
            case 12: return "Level up (Silcoon personality)";
            case 13: return "Level up (Cascoon personality)";
            case 14: return (evo.level > 1) ? ("Level " + std::to_string(evo.level)) : "Level up";
            case 15: return "Level up with extra Poké Ball & party slot";
            case 16: return "Level up with high Beauty";
            case 17:
                return "Use " + std::string(Names::getItemNameFor(group, evo.argument)) + " (male)";
            case 18:
                return "Use " + std::string(Names::getItemNameFor(group, evo.argument)) + " (female)";
            case 19:
                return "Level up holding " + std::string(Names::getItemNameFor(group, evo.argument)) + " by day";
            case 20:
                return "Level up holding " + std::string(Names::getItemNameFor(group, evo.argument)) + " at night";
            case 21:
                return "Level up knowing " + std::string(Names::getMoveName(evo.argument));
            case 22: return "Level up with teammate in party";
            case 23:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level)) : "Level up") + " (male)";
            case 24:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level)) : "Level up") + " (female)";
            case 25: return "Level up at a magnetic field";
            case 26: return "Level up near a Moss Rock";
            case 27: return "Level up near an Ice Rock";
            case 28:
                return (evo.level > 1 ? ("Level " + std::to_string(evo.level)) : "Level up") + " (upside down)";
            case 29: return "Level up with high affection knowing Fairy move";
            case 30: return "Level up with Dark-type in party";
            case 31: return "Level up in rain or fog";
            case 32: return "Level up by day";
            case 33: return "Level up at night";
            case 34: return (evo.level > 1 ? ("Level " + std::to_string(evo.level)) : "Level up") + " (female)";
            case 36: return "Level up in specific game version";
            case 39: return "Level up at Mount Lanakila";
            case 40: return "Level up at dusk";
            case 41: return "Level up in Ultra Space";
            case 42:
                return "Use " + std::string(Names::getItemNameFor(group, evo.argument)) + " in Ultra Space";
            case 43: return "Land 3 critical hits in one battle";
            case 44: return "Lose 49+ HP, then pass Dusty Bowl arch";
            case 45: return "Spin while holding a Sweet";
            case 46: return "Level up (Amped Nature)";
            case 47: return "Level up (Low Key Nature)";
            case 48: return "Train in Tower of Darkness";
            case 49: return "Train in Tower of Waters";
            case 50: return "Walk 1,000 steps in Let's Go mode";
            case 51: return "Level up in a Union Circle";
            case 52:
            case 53: return "Level up in battle";
            case 54: return "Collect 999 Gimmighoul Coins";
            case 55: return "Defeat 3 Bisharp leading Pawniard";
            case 56: return "Use Rage Fist 20 times";
            case 57:
            case 58: return "Level up knowing " + std::string(Names::getMoveName(evo.argument));
            case 59:
            case 60: return "Lose 294+ HP from recoil without fainting";
            case 61: return "Evolve in Hisui";
            case 62:
            case 92: return "Use Barb Barrage 20 times";
            case 90: return "Use Peat Block in full moon";
            case 91: return "Use Psyshield Bash in Agile Style 20 times";
            default:
                if (evo.level > 1)
                    return "Level " + std::to_string(evo.level);
                return "Special condition";
        }
    }

    bool applyEvolution(Pokemon &pokemon, const EvolutionCandidate &candidate)
    {
        if (pokemon.speciesID() == 0 || candidate.destinationSpeciesId == 0)
            return false;

        const Enums::GameVersion gameGroup = pokemon.getGameGroup();
        const bool renameNickname = nicknameIsSpeciesName(pokemon);
        const uint8_t previousAbilityNumber = pokemon.abilityNumber();

        pokemon.setSpecies(candidate.destinationSpeciesId);
        pokemon.setForm(candidate.destinationFormId);
        carryAbilitySlot(pokemon, previousAbilityNumber, candidate.destinationSpeciesId,
                         candidate.destinationFormId, gameGroup);

        // Consume held item if this evolution was a held-item evolution
        if ((candidate.method == 6 || candidate.method == 19 || candidate.method == 20) &&
            pokemon.hasHeldItem() && pokemon.heldItem() == candidate.argument)
        {
            pokemon.setHeldItem(0);
        }

        if (renameNickname)
        {
            pokemon.setNickname(
                speciesNameAsStored(candidate.destinationSpeciesId, gameGroup, pokemon.language()));
        }

        pokemon.recalculateStats();
        pokemon.refreshChecksum();
        return true;
    }

    EvolutionInfo getEvolutionInfo(const Pokemon &pokemon)
    {
        EvolutionInfo info;
        if (pokemon.speciesID() == 0)
            return info;

        const Enums::GameVersion group = pokemon.getGameGroup();
        info.speciesId = pokemon.speciesID();
        info.formId = pokemon.form();
        info.isShiny = pokemon.isShiny(pokemon.id32(), pokemon.species());

        uint8_t subLvl = pokemon.level();
        if (subLvl == 0)
            subLvl = getLevelFromExp(pokemon.exp(), getGrowthRate(pokemon.speciesID()));
        if (subLvl == 0)
            subLvl = 1;
        info.currentLevel = subLvl;

        if (pokemon.eggTextIsPlaceholder())
            info.currentNickname = "Egg";
        else
            info.currentNickname = Utils::utf16ToUtf8(pokemon.nickname());
        if (info.currentNickname.empty())
            info.currentNickname = Trainer::getSpeciesName(pokemon.speciesID());

        info.currentSpeciesName = Trainer::getSpeciesName(pokemon.speciesID());

        // Ancestor lineage
        uint16_t ancSpecies[EVO_MAX_CHAIN]{};
        uint8_t  ancForms[EVO_MAX_CHAIN]{};
        int ancCount = getPreEvolutionChain(group, pokemon.speciesID(), pokemon.form(),
                                            ancSpecies, ancForms, EVO_MAX_CHAIN);
        for (int i = ancCount - 1; i >= 0; --i)
        {
            const char *spName = Trainer::getSpeciesName(ancSpecies[i]);
            info.chain.push_back({ancSpecies[i], ancForms[i], false,
                                  Names::getDisplayName(ancSpecies[i], ancForms[i], spName)});
        }
        info.chain.push_back({pokemon.speciesID(), pokemon.form(), true,
                              Names::getDisplayName(pokemon.speciesID(), pokemon.form(), info.currentSpeciesName.c_str())});

        // Forward evolutions
        ForwardEvolution fwdList[FORWARD_EVOLUTIONS_MAX_PER_SPECIES]{};
        int fwdCount = getForwardEvolutions(group, pokemon.speciesID(), pokemon.form(),
                                           fwdList, FORWARD_EVOLUTIONS_MAX_PER_SPECIES);

        const uint16_t currentHeldItem = pokemon.hasHeldItem() ? pokemon.heldItem() : 0;
        const char *abilityBeforeName = pokemon.hasAbility() ? Trainer::getAbilityName(pokemon.ability()) : "—";

        for (int i = 0; i < fwdCount; ++i)
        {
            const ForwardEvolution &evo = fwdList[i];
            EvolutionCandidate cand;
            cand.destinationSpeciesId = evo.destinationSpeciesId;
            cand.destinationFormId = evo.destinationFormId;
            cand.method = evo.method;
            cand.argument = evo.argument;
            cand.level = evo.level;
            cand.isTrade = (evo.method == 5 || evo.method == 6 || evo.method == 7);

            const char *dstBaseName = Trainer::getSpeciesName(evo.destinationSpeciesId);
            cand.speciesName = Names::getDisplayName(evo.destinationSpeciesId, evo.destinationFormId, dstBaseName);
            cand.requirement = getEvolutionRequirementText(group, evo, info.currentLevel);

            // Condition met determination
            if (cand.isTrade)
            {
                cand.conditionMet = true;
                if (evo.method == 6 && currentHeldItem != evo.argument && evo.argument != 0)
                {
                    cand.statusText = "Link trade simulated · Gives " + std::string(Names::getItemNameFor(group, evo.argument));
                }
                else
                {
                    cand.statusText = "Link trade simulated · No second console needed";
                }
            }
            else if (evo.method == 8 || evo.method == 17 || evo.method == 18 || evo.method == 42 || evo.method == 90)
            {
                cand.conditionMet = true;
                cand.statusText = "Ready to evolve";
            }
            else if (evo.level > 0)
            {
                cand.conditionMet = (info.currentLevel >= evo.level);
                if (cand.conditionMet)
                    cand.statusText = "Requirement met (Lv. " + std::to_string(evo.level) + "+)";
                else
                    cand.statusText = "Current Lv. " + std::to_string(info.currentLevel) + " · Needs Lv. " + std::to_string(evo.level);
            }
            else
            {
                cand.conditionMet = true;
                cand.statusText = "Ready to evolve";
            }

            // Consumed item
            if (evo.method == 6 || evo.method == 19 || evo.method == 20 ||
                evo.method == 8 || evo.method == 17 || evo.method == 18 || evo.method == 90)
            {
                if (evo.argument != 0)
                    cand.consumedItem = Names::getItemNameFor(group, evo.argument);
            }

            cand.abilityBefore = abilityBeforeName;

            // Stats before
            cand.statsBefore[0] = pokemon.statHPMax();
            cand.statsBefore[1] = pokemon.statATK();
            cand.statsBefore[2] = pokemon.statDEF();
            cand.statsBefore[3] = pokemon.statSPA();
            cand.statsBefore[4] = pokemon.statSPD();
            cand.statsBefore[5] = pokemon.statSPE();

            // Preview evolved stats via clone
            std::unique_ptr<Pokemon> preview = pokemon.clone();
            if (preview)
            {
                preview->setSpecies(evo.destinationSpeciesId);
                preview->setForm(evo.destinationFormId);
                carryAbilitySlot(*preview, pokemon.abilityNumber(), evo.destinationSpeciesId,
                                 evo.destinationFormId, group);
                preview->recalculateStats();

                cand.abilityAfter = preview->hasAbility() ? Trainer::getAbilityName(preview->ability()) : "—";
                cand.statsAfter[0] = preview->statHPMax();
                cand.statsAfter[1] = preview->statATK();
                cand.statsAfter[2] = preview->statDEF();
                cand.statsAfter[3] = preview->statSPA();
                cand.statsAfter[4] = preview->statSPD();
                cand.statsAfter[5] = preview->statSPE();
            }
            else
            {
                cand.abilityAfter = cand.abilityBefore;
                for (int s = 0; s < 6; ++s)
                    cand.statsAfter[s] = cand.statsBefore[s];
            }

            info.candidates.push_back(std::move(cand));
        }

        return info;
    }
}
