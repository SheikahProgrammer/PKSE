#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

#include "UI/Pokepark.h"
#include "Trainer/Trainer.h"
#include "Trainer/Bank.h"
#include "Pokemon/Pokemon.h"
#include "Pokemon/PokemonTypes.h"
#include "Utils/StringHelpers.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/PersonalRecord.h"
#include "Pokemon/EvolutionTable.h"
#include "Enums/GameVersion.h"

namespace UI
{
    std::string ParkRelationship::key() const
    {
        return makeKey(firstId, secondId);
    }

    std::string ParkRelationship::makeKey(const std::string &a, const std::string &b)
    {
        return (a <= b) ? (a + ":" + b) : (b + ":" + a);
    }

    bool ParkResident::enjoysHabitat(ParkEnvironmentId env) const
    {
        if (env == ParkEnvironmentId::Commons)
            return true;

        if (env == ParkEnvironmentId::WaterfallLake)
            return (type1 == Pokemon::TYPE_WATER || type2 == Pokemon::TYPE_WATER);

        if (env == ParkEnvironmentId::EmberGrove)
            return (type1 == Pokemon::TYPE_FIRE || type2 == Pokemon::TYPE_FIRE ||
                    type1 == Pokemon::TYPE_GROUND || type2 == Pokemon::TYPE_GROUND ||
                    type1 == Pokemon::TYPE_ROCK || type2 == Pokemon::TYPE_ROCK);

        if (env == ParkEnvironmentId::SkySummit)
            return (type1 == Pokemon::TYPE_FLYING || type2 == Pokemon::TYPE_FLYING ||
                    type1 == Pokemon::TYPE_ICE || type2 == Pokemon::TYPE_ICE ||
                    type1 == Pokemon::TYPE_DRAGON || type2 == Pokemon::TYPE_DRAGON ||
                    personality == ParkPersonality::Shy);

        return true;
    }

    static const ParkEnvironmentInfo s_environments[4] = {
        {
            ParkEnvironmentId::Commons,
            "Sunlit Commons",
            "A little of everything",
            "Meadow plaza",
            {},
            false,
            {
                "Wind combs the tall grass.",
                "A distant chime marks snack time.",
                "The commons hums with friendly footsteps."
            }
        },
        {
            ParkEnvironmentId::WaterfallLake,
            "Cascade Lake",
            "Waterfall play zone",
            "Moonfall cascade",
            {Pokemon::TYPE_WATER},
            false,
            {
                "The waterfall throws tiny rainbows across the lake.",
                "Smooth pebbles skip over the sunlit shallows.",
                "Cool mist drifts through the reeds."
            }
        },
        {
            ParkEnvironmentId::EmberGrove,
            "Ember Grove",
            "Warm stones and brave games",
            "Cinder ring",
            {Pokemon::TYPE_FIRE, Pokemon::TYPE_GROUND, Pokemon::TYPE_ROCK},
            false,
            {
                "Warm basalt stones tick softly as they cool.",
                "A safe ember glows beneath the shady canopy.",
                "The grove smells faintly of toasted berries."
            }
        },
        {
            ParkEnvironmentId::SkySummit,
            "Sky Summit",
            "Breezes above the clouds",
            "Snowcap overlook",
            {Pokemon::TYPE_FLYING, Pokemon::TYPE_ICE, Pokemon::TYPE_DRAGON},
            true,
            {
                "Cloud shadows drift silently below the plateau.",
                "A mountain breeze whispers between ancient rocks.",
                "Distant peaks glisten in the crisp alpine air."
            }
        }
    };

    const ParkEnvironmentInfo &PokeparkManager::getEnvironmentInfo(ParkEnvironmentId id)
    {
        int index = static_cast<int>(id);
        if (index < 0 || index >= 4)
            index = 0;
        return s_environments[index];
    }

    ParkPersonality PokeparkManager::derivePersonality(uint8_t nature, uint32_t pid)
    {
        switch (nature)
        {
        case 13: case 14: case 8:   // Jolly, Naive, Impish
            return ParkPersonality::Playful;
        case 21: case 15: case 16:  // Gentle, Modest, Mild
            return ParkPersonality::Gentle;
        case 3: case 5: case 2:     // Adamant, Bold, Brave
            return ParkPersonality::Bold;
        case 10: case 1:            // Timid, Lonely
            return ParkPersonality::Shy;
        case 11: case 19: case 24:  // Hasty, Rash, Quirky
            return ParkPersonality::Curious;
        case 4: case 22:            // Naughty, Sassy
            return ParkPersonality::Energetic;
        case 20: case 7: case 17:   // Calm, Relaxed, Quiet
            return ParkPersonality::Calm;
        case 23:                    // Careful
            return ParkPersonality::Helpful;
        case 9:                     // Lax
            return ParkPersonality::Proud;
        default:
            return static_cast<ParkPersonality>((pid % 10));
        }
    }

    const char *PokeparkManager::getPersonalityName(ParkPersonality p)
    {
        switch (p)
        {
        case ParkPersonality::Playful:   return "Playful";
        case ParkPersonality::Gentle:    return "Gentle";
        case ParkPersonality::Bold:      return "Bold";
        case ParkPersonality::Shy:       return "Shy";
        case ParkPersonality::Curious:   return "Curious";
        case ParkPersonality::Energetic: return "Energetic";
        case ParkPersonality::Calm:      return "Calm";
        case ParkPersonality::Helpful:   return "Helpful";
        case ParkPersonality::Proud:     return "Proud";
        case ParkPersonality::Balanced:
        default:                         return "Balanced";
        }
    }

    const char *PokeparkManager::getPersonalityDescription(ParkPersonality p)
    {
        switch (p)
        {
        case ParkPersonality::Playful:   return "Loves running around, chasing friends, and playing games.";
        case ParkPersonality::Gentle:    return "Always mindful of others, sharing snacks and peaceful moments.";
        case ParkPersonality::Bold:      return "Eager for friendly challenges, exploring every edge of the park.";
        case ParkPersonality::Shy:       return "Prefers quiet corners, slowly opening up when approached gently.";
        case ParkPersonality::Curious:   return "Inspects every leaf, pebble, and reflection with bright eyes.";
        case ParkPersonality::Energetic: return "Brimming with excitement; even sitting still turns into a dance.";
        case ParkPersonality::Calm:      return "Serene and steady, soaking in the breeze and soothing others.";
        case ParkPersonality::Helpful:   return "Distracts squabbling friends and keeps harmony in the park.";
        case ParkPersonality::Proud:     return "Strikes majestic poses and proudly displays its beauty.";
        case ParkPersonality::Balanced:
        default:                         return "Easygoing and friendly with everyone in the habitat.";
        }
    }

    const char *PokeparkManager::getMoodName(ParkMood m)
    {
        switch (m)
        {
        case ParkMood::Playful: return "Playful";
        case ParkMood::Sleepy:  return "Sleepy";
        case ParkMood::Excited: return "Excited";
        case ParkMood::Curious: return "Curious";
        case ParkMood::Grumpy:  return "A little grumpy";
        case ParkMood::Lonely:  return "A bit lonely";
        case ParkMood::Content:
        default:                return "Content";
        }
    }

    const char *PokeparkManager::getActivityName(ParkActivity a)
    {
        switch (a)
        {
        case ParkActivity::Idle:      return "Looking around";
        case ParkActivity::Wander:    return "Taking a stroll";
        case ParkActivity::Run:       return "Doing zoomies";
        case ParkActivity::Sit:       return "Resting";
        case ParkActivity::Nap:       return "Napping";
        case ParkActivity::Sniff:     return "Investigating";
        case ParkActivity::Approach:  return "Going to say hello";
        case ParkActivity::Follow:    return "Walking with a friend";
        case ParkActivity::Flee:      return "Keeping its distance";
        case ParkActivity::Play:      return "Playing";
        case ParkActivity::Spar:      return "Play-sparring";
        case ParkActivity::Sulk:      return "Sulking";
        case ParkActivity::Celebrate: return "Celebrating";
        case ParkActivity::Swim:      return "Swimming";
        case ParkActivity::Perch:     return "Perched up high";
        case ParkActivity::Mediate:   return "Making peace";
        case ParkActivity::Fetch:     return "Chasing a toy";
        case ParkActivity::Eat:       return "Eating a berry";
        case ParkActivity::Called:    return "Coming to you";
        case ParkActivity::Watch:     return "Watching quietly";
        }
        return "Wandering";
    }

    const char *PokeparkManager::getFlavorName(ParkFlavor f)
    {
        switch (f)
        {
        case ParkFlavor::Spicy:  return "spicy Cheri Berry";
        case ParkFlavor::Dry:    return "dry Chesto Berry";
        case ParkFlavor::Sweet:  return "sweet Pecha Berry";
        case ParkFlavor::Bitter: return "bitter Rawst Berry";
        case ParkFlavor::Sour:   return "sour Aspear Berry";
        }
        return "berry";
    }

    const char *PokeparkManager::getWeatherName(ParkWeather w)
    {
        switch (w)
        {
        case ParkWeather::Rain: return "Rain";
        case ParkWeather::Snow: return "Snow";
        case ParkWeather::Heat: return "Heatwave";
        case ParkWeather::Wind: return "Windy";
        case ParkWeather::Fog:  return "Fog";
        case ParkWeather::Clear:
        default:                return "Clear";
        }
    }

    // A nature raises one stat and lowers one (index = 5 * raised + lowered, in the order Atk, Def,
    // Spe, SpA, SpD), and the games tie each stat to a flavour: the raised one is liked, the lowered
    // one disliked, and a neutral nature (raised == lowered) minds nothing.
    static constexpr ParkFlavor FLAVOR_OF_STAT[5] = {ParkFlavor::Spicy, ParkFlavor::Sour, ParkFlavor::Sweet,
                                                     ParkFlavor::Dry, ParkFlavor::Bitter};

    static int flavorReaction(uint8_t nature, ParkFlavor flavor)
    {
        const int raised = (nature % 25) / 5, lowered = (nature % 25) % 5;
        if (raised == lowered)
            return 0;
        if (FLAVOR_OF_STAT[raised] == flavor)
            return 1;
        if (FLAVOR_OF_STAT[lowered] == flavor)
            return -1;
        return 0;
    }

    // Night-active species beyond the Ghost and Dark types (which all are).
    static bool isNocturnalSpecies(uint16_t species)
    {
        static const uint16_t list[] = {41,  42,  43,  92,  93,  94,  163, 164, 169, 170, 171, 197, 198, 200, 215,
                                        302, 313, 314, 355, 356, 401, 402, 429, 430, 477, 607, 608, 609, 714, 715};
        for (uint16_t s : list)
            if (s == species)
                return true;
        return false;
    }

    // ---- Compatibility (PKForge ParkSocialSimulation.Compatibility) --------------------------------

    static int personalityCompatibility(ParkPersonality a, ParkPersonality b)
    {
        using P = ParkPersonality;
        if (a == b)
            return a == P::Proud ? -4 : 10;
        auto pair = [&](P x, P y) { return (a == x && b == y) || (a == y && b == x); };
        if (pair(P::Playful, P::Energetic) || pair(P::Playful, P::Curious))
            return 14;
        if (pair(P::Shy, P::Gentle) || pair(P::Shy, P::Calm) || pair(P::Shy, P::Helpful))
            return 16;
        if (pair(P::Bold, P::Proud))
            return -16;
        if (pair(P::Calm, P::Energetic))
            return -3;
        return 2;
    }

    int PokeparkManager::compatibility(const ParkResident &a, const ParkResident &b)
    {
        int score = 0;
        if (a.species == b.species)
            score += 22;
        if (a.familyRoot != 0 && a.familyRoot == b.familyRoot)
            score += 16;
        if (a.hasType(b.type1) || (b.type2 != 255 && a.hasType(b.type2)))
            score += 12;
        if (a.homeHabitat == b.homeHabitat)
            score += 8;
        score += personalityCompatibility(a.personality, b.personality);
        return std::clamp(score, -30, 60);
    }

    // ---- Residents ---------------------------------------------------------------------------------

    void PokeparkManager::refreshDerived(ParkResident &res)
    {
        // Cross-game facts, so the Scarlet/Violet table (which spans the whole National Dex).
        const Pokemon::PersonalRecord &rec = Pokemon::getPersonalRecord(Enums::GameVersion::SV, res.species, res.form);
        res.baseSpeed = rec.spe > 0 ? rec.spe : 60;
        const Pokemon::PersonalInfo &info = Pokemon::getPersonalInfo(res.species, res.form);
        res.weightKg = info.weight / 10.0f;
        res.small = info.height <= 5; // half a metre or less: hops rather than walks

        uint16_t chain[8];
        uint8_t forms[8];
        const int n = Pokemon::getPreEvolutionChain(Enums::GameVersion::SV, res.species, res.form, chain, forms, 8);
        res.familyRoot = n > 0 ? chain[n - 1] : res.species;

        res.traversal = TraverseLand;
        if (res.hasType(Pokemon::TYPE_WATER))
            res.traversal |= TraverseWater;
        if (res.hasType(Pokemon::TYPE_FIRE) || res.hasType(Pokemon::TYPE_GROUND) || res.hasType(Pokemon::TYPE_ROCK) ||
            res.hasType(Pokemon::TYPE_STEEL) || res.hasType(Pokemon::TYPE_DRAGON))
            res.traversal |= TraverseWarm;
        if (res.hasType(Pokemon::TYPE_FLYING))
            res.traversal |= TraverseAir | TraverseWater | TraverseWarm; // flies over everything
        res.nocturnal = res.hasType(Pokemon::TYPE_GHOST) || res.hasType(Pokemon::TYPE_DARK) ||
                        isNocturnalSpecies(res.species);
    }

    static ParkEnvironmentId homeFor(uint8_t type1, uint8_t type2)
    {
        auto has = [&](uint8_t t) { return type1 == t || type2 == t; };
        if (has(Pokemon::TYPE_WATER))
            return ParkEnvironmentId::WaterfallLake;
        if (has(Pokemon::TYPE_FIRE) || type1 == Pokemon::TYPE_GROUND || type1 == Pokemon::TYPE_ROCK)
            return ParkEnvironmentId::EmberGrove;
        if (has(Pokemon::TYPE_FLYING) || has(Pokemon::TYPE_ICE))
            return ParkEnvironmentId::SkySummit;
        return ParkEnvironmentId::Commons;
    }

    ParkResident PokeparkManager::makeResident(const std::string &id, const std::string &source,
                                               const Pokemon::Pokemon &mon, const Trainer::Trainer &trainer)
    {
        ParkResident res;
        res.id = id;
        res.species = mon.speciesID();
        res.form = mon.form();
        res.shiny = mon.isShiny(mon.id32(), mon.species());
        res.name = mon.nickname().empty() ? std::string(mon.species()) : Utils::utf16ToUtf8(mon.nickname());
        res.sourceInfo = source;
        const auto types = Pokemon::getPokemonTypes(res.species, res.form, trainer.getGameGroup());
        res.type1 = types.type1;
        res.type2 = types.type2;
        res.nature = mon.nature();
        res.personality = derivePersonality(mon.nature(), mon.id32());
        res.homeHabitat = homeFor(res.type1, res.type2);
        res.bond = 20;
        refreshDerived(res);
        return res;
    }

    void PokeparkManager::autoFillRoster(Trainer::Trainer *trainer, Trainer::Bank *bank)
    {
        (void)bank;
        if (!trainer)
            return;
        residents.clear();

        // Live party members first, then one of each species from the boxes.
        for (size_t i = 0; i < trainer->party.size() && residents.size() < DEFAULT_RESIDENTS; ++i)
        {
            const auto &mon = trainer->party[i];
            if (mon && mon->speciesID() != 0 && !mon->isEgg())
                residents.push_back(makeResident("party:" + std::to_string(i), "Party Slot " + std::to_string(i + 1),
                                                 *mon, *trainer));
        }
        for (size_t b = 0; b < trainer->boxes.size() && residents.size() < DEFAULT_RESIDENTS; ++b)
        {
            for (size_t s = 0; s < trainer->boxes[b].size() && residents.size() < DEFAULT_RESIDENTS; ++s)
            {
                const auto &mon = trainer->boxes[b][s];
                if (!mon || mon->speciesID() == 0 || mon->isEgg())
                    continue;
                const bool duplicate = std::any_of(residents.begin(), residents.end(),
                                                   [&](const ParkResident &r) { return r.species == mon->speciesID(); });
                if (duplicate)
                    continue;
                residents.push_back(makeResident("box:" + std::to_string(b) + ":" + std::to_string(s),
                                                 "Box " + std::to_string(b + 1) + " Slot " + std::to_string(s + 1),
                                                 *mon, *trainer));
            }
        }
        for (auto &res : residents)
            pickSpot(res, res.x, res.y);
        saveState();
    }

    void PokeparkManager::removeResident(int residentIndex)
    {
        if (residentIndex >= 0 && residentIndex < static_cast<int>(residents.size()))
        {
            residents.erase(residents.begin() + residentIndex);
            for (auto &r : residents)
                r.partner = r.leader = r.toy = -1; // indices shifted
            saveState();
        }
    }

    void PokeparkManager::addResidentFromSave(int box, int slot, const Trainer::Trainer &trainer)
    {
        if (residents.size() >= MAX_RESIDENTS)
            return;
        if (box < 0 || box >= static_cast<int>(trainer.boxes.size()))
            return;
        if (slot < 0 || slot >= static_cast<int>(trainer.boxes[box].size()))
            return;
        const auto &mon = trainer.boxes[box][slot];
        if (!mon || mon->speciesID() == 0 || mon->isEgg())
            return;
        ParkResident res = makeResident("box:" + std::to_string(box) + ":" + std::to_string(slot),
                                        "Box " + std::to_string(box + 1) + " Slot " + std::to_string(slot + 1), *mon,
                                        trainer);
        pickSpot(res, res.x, res.y);
        residents.push_back(res);
        saveState();
    }

    bool PokeparkManager::hasResident(const std::string &id) const
    {
        return std::any_of(residents.begin(), residents.end(), [&](const ParkResident &r) { return r.id == id; });
    }

    bool PokeparkManager::addResident(const std::string &id, const std::string &source, const Pokemon::Pokemon &mon,
                                      const Trainer::Trainer &trainer)
    {
        if (residents.size() >= MAX_RESIDENTS || hasResident(id) || mon.speciesID() == 0 || mon.isEgg())
            return false;
        ParkResident res = makeResident(id, source, mon, trainer);
        // A newcomer arrives at the front of the scene and looks around.
        res.x = PLAYER_X + static_cast<float>((rand() % 200) - 100);
        res.y = PLAYER_Y - 20.0f;
        if (!canStand(res, res.x, res.y))
            pickSpot(res, res.x, res.y);
        res.targetX = res.x;
        res.targetY = res.y;
        res.activityTimer = 1.0f;
        res.emotion = ParkEmotion::Exclamation;
        res.emotionTimer = 2.0f;
        residents.push_back(res);
        addJournal(ParkJournalEventKind::Activity, res.name + " moved into the park.",
                   {static_cast<int>(residents.size()) - 1});
        saveState();
        return true;
    }

    void PokeparkManager::removeResidentById(const std::string &id)
    {
        for (size_t i = 0; i < residents.size(); ++i)
            if (residents[i].id == id)
            {
                removeResident(static_cast<int>(i));
                return;
            }
    }

    void PokeparkManager::adoptLegacyResidents(const Trainer::Trainer &trainer, const std::string &gameKey,
                                               const std::string &gameName)
    {
        bool changed = false;
        for (auto &res : residents)
        {
            if (res.id.find('#') != std::string::npos)
                continue; // already belongs to a game
            const Pokemon::Pokemon *mon = nullptr;
            std::string where;
            int box = -1, slot = -1;
            if (std::sscanf(res.id.c_str(), "party:%d", &slot) == 1)
            {
                if (slot >= 0 && slot < static_cast<int>(trainer.party.size()))
                    mon = trainer.party[slot].get();
                where = "Party " + std::to_string(slot + 1);
            }
            else if (std::sscanf(res.id.c_str(), "box:%d:%d", &box, &slot) == 2)
            {
                if (box >= 0 && box < static_cast<int>(trainer.boxes.size()) && slot >= 0 &&
                    slot < static_cast<int>(trainer.getSlotsPerBox()))
                    mon = trainer.boxes[box][slot].get();
                where = "Box " + std::to_string(box + 1) + ", slot " + std::to_string(slot + 1);
            }
            if (!mon || mon->speciesID() != res.species)
                continue;
            const std::string newId = gameKey + "#" + res.id;
            // Its relationships move with it.
            std::map<std::string, ParkRelationship> moved;
            for (auto &[key, rel] : relationships)
            {
                ParkRelationship copy = rel;
                if (copy.firstId == res.id)
                    copy.firstId = newId;
                if (copy.secondId == res.id)
                    copy.secondId = newId;
                moved[copy.key()] = copy;
            }
            relationships.swap(moved);
            for (auto &ev : journal)
                for (auto &id : ev.residentIds)
                    if (id == res.id)
                        id = newId;
            res.id = newId;
            res.sourceInfo = gameName + "  ·  " + where;
            changed = true;
        }
        if (changed)
            saveState();
    }

    PokeparkManager &PokeparkManager::instance()
    {
        static PokeparkManager s_instance;
        return s_instance;
    }

    void PokeparkManager::initialize(Trainer::Trainer *trainer, Trainer::Bank *bank)
    {
        activeTrainer = trainer;
        activeBank = bank;

        // The park is shared by every game, and each game chooses who it sends: nobody moves in
        // just because a save was opened.
        loadState();

        refreshClock();
        uint64_t now = static_cast<uint64_t>(std::time(nullptr));
        if (lastSimulationTime > 0 && now > lastSimulationTime && now - lastSimulationTime >= 1200)
            runOfflineSimulation(lastSimulationTime, now);
        lastSimulationTime = now;

        toys.clear();
        particles.clear();
        visitor = ParkVisitor{};
        for (size_t i = 0; i < residents.size(); ++i)
        {
            ParkResident &res = residents[i];
            if (!canStand(res, res.x, res.y))
                pickSpot(res, res.x, res.y);
            res.targetX = res.x;
            res.targetY = res.y;
            res.activity = ParkActivity::Idle;
            res.activityTimer = 0.5f + static_cast<float>(i) * 0.3f; // stagger first decisions
            res.partner = res.leader = res.toy = -1;
        }
        saveState();
    }

    void PokeparkManager::cycleHabitat(int direction) noexcept
    {
        int id = static_cast<int>(currentHabitat) + direction;
        if (id < 0)
            id = 3;
        else if (id > 3)
            id = 0;
        currentHabitat = static_cast<ParkEnvironmentId>(id);
        toys.clear();
        particles.clear();
        visitor = ParkVisitor{};
        // Residents arriving in a habitat find their footing there (the lake is not the summit).
        for (auto &res : residents)
        {
            if (!canStand(res, res.x, res.y))
                pickSpot(res, res.x, res.y);
            res.targetX = res.x;
            res.targetY = res.y;
            res.partner = res.leader = res.toy = -1;
            res.activity = ParkActivity::Idle;
            res.activityTimer = 0.4f;
        }
    }

    std::vector<int> PokeparkManager::getVisibleResidentIndices(ParkEnvironmentId env) const
    {
        std::vector<int> result;
        for (size_t i = 0; i < residents.size(); ++i)
            if (residents[i].enjoysHabitat(env))
                result.push_back(static_cast<int>(i));
        return result;
    }

    void PokeparkManager::clearJournal()
    {
        journal.clear();
        saveState();
    }

    ParkRelationship PokeparkManager::getRelationship(const std::string &idA, const std::string &idB)
    {
        auto it = relationships.find(ParkRelationship::makeKey(idA, idB));
        if (it != relationships.end())
            return it->second;
        ParkRelationship fresh;
        fresh.firstId = idA;
        fresh.secondId = idB;
        return fresh;
    }

    void PokeparkManager::updateRelationship(const std::string &idA, const std::string &idB, int dFriendship,
                                             int dAffection, int dRivalry)
    {
        ParkRelationship &rel = relationships[ParkRelationship::makeKey(idA, idB)];
        rel.firstId = idA;
        rel.secondId = idB;
        rel.friendship = std::clamp(rel.friendship + dFriendship, -100, 100);
        rel.affection = std::clamp(rel.affection + dAffection, 0, 100);
        rel.rivalry = std::clamp(rel.rivalry + dRivalry, 0, 100);
        rel.sharedMoments++;
        rel.lastEventAt = static_cast<uint64_t>(std::time(nullptr));
    }

    std::string PokeparkManager::bondSummary(int residentIndex)
    {
        if (residentIndex < 0 || residentIndex >= static_cast<int>(residents.size()))
            return "";
        const ParkResident &me = residents[residentIndex];
        const ParkResident *best = nullptr, *rival = nullptr;
        int bestScore = 0, rivalScore = 0;
        for (const auto &other : residents)
        {
            if (other.id == me.id)
                continue;
            const ParkRelationship rel = getRelationship(me.id, other.id);
            if (rel.friendship >= 60 && rel.sharedMoments >= 12 && rel.friendship > bestScore)
            {
                bestScore = rel.friendship;
                best = &other;
            }
            if (rel.rivalry >= 50 && rel.rivalry > rivalScore)
            {
                rivalScore = rel.rivalry;
                rival = &other;
            }
        }
        std::string out;
        if (best)
            out = "Best friends with " + best->name;
        if (rival)
            out += (out.empty() ? "" : "  ·  ") + std::string("Rivals with ") + rival->name;
        return out;
    }

    void PokeparkManager::setResidentEmotion(int residentIndex, ParkEmotion emotion, float duration)
    {
        if (residentIndex >= 0 && residentIndex < static_cast<int>(residents.size()))
        {
            ParkResident &res = residents[residentIndex];
            if (res.emotion != emotion)
                res.emotionAge = 0.0f;
            res.emotion = emotion;
            res.emotionTimer = duration;
        }
    }

    // ---- Journal -----------------------------------------------------------------------------------

    void PokeparkManager::addJournal(ParkJournalEventKind kind, const std::string &text, const std::vector<int> &who,
                                     uint64_t when)
    {
        ParkJournalEvent ev;
        ev.timestamp = when ? when : static_cast<uint64_t>(std::time(nullptr));
        const std::time_t raw = static_cast<std::time_t>(ev.timestamp);
        char buffer[16];
        std::strftime(buffer, sizeof(buffer), "%H:%M", std::localtime(&raw));
        ev.timeStr = buffer;
        ev.kind = kind;
        ev.text = text;
        ev.habitatName = getEnvironmentInfo(currentHabitat).name;
        for (size_t i = 0; i < who.size() && i < 2; ++i)
        {
            if (who[i] < 0 || who[i] >= static_cast<int>(residents.size()))
                continue;
            ev.residentIds.push_back(residents[who[i]].id);
            (i == 0 ? ev.species1 : ev.species2) = residents[who[i]].species;
        }
        journal.push_back(ev);
        std::stable_sort(journal.begin(), journal.end(),
                         [](const ParkJournalEvent &a, const ParkJournalEvent &b) { return a.timestamp < b.timestamp; });
        while (journal.size() > MAX_JOURNAL_ENTRIES)
            journal.erase(journal.begin());
    }

    void PokeparkManager::recordPhoto(const std::string &fileName)
    {
        addJournal(ParkJournalEventKind::Photo,
                   std::string("You took a photo at ") + getEnvironmentInfo(currentHabitat).name + ": " + fileName, {});
        saveState();
    }

    // ---- Player interactions -----------------------------------------------------------------------

    static std::string fill(std::string text, const std::string &name)
    {
        for (size_t pos = text.find("{name}"); pos != std::string::npos; pos = text.find("{name}", pos + name.size()))
            text.replace(pos, 6, name);
        return text;
    }

    std::string PokeparkManager::interact(int residentIndex, const std::string &action)
    {
        if (residentIndex < 0 || residentIndex >= static_cast<int>(residents.size()))
            return "";
        ParkResident &res = residents[residentIndex];
        std::string nearbyName;
        for (int vIdx : getVisibleResidentIndices(currentHabitat))
            if (vIdx != residentIndex)
            {
                nearbyName = residents[vIdx].name;
                break;
            }

        std::string line;
        if (action == "snack")
            return feedBerry(residentIndex, FLAVOR_OF_STAT[(res.nature % 25) / 5]);
        if (action == "talk")
        {
            // A shy resident warms up slowly; one you are close to lights up.
            const bool warm = res.personality != ParkPersonality::Shy || res.bond >= 30;
            res.bond = std::min(100, res.bond + (warm ? 3 : 1));
            res.social = std::max(0.0f, res.social - 30.0f);
            setResidentEmotion(residentIndex, warm ? ParkEmotion::Music : ParkEmotion::Sweat, 3.5f);
            if (!warm)
                line = fill("{name} glances at you, then away -- but it stays where it is. That is something.",
                            res.name);
        }
        else if (action == "play")
        {
            if (res.energy < 20.0f)
            {
                setResidentEmotion(residentIndex, ParkEmotion::Sleep, 3.5f);
                return fill("{name} gives a huge yawn. It is far too tired to play right now.", res.name);
            }
            res.play = std::max(0.0f, res.play - 40.0f);
            res.energy = std::max(0.0f, res.energy - 10.0f);
            res.bond = std::min(100, res.bond + 3);
            setResidentEmotion(residentIndex, ParkEmotion::Heart, 3.5f);
            startActivity(residentIndex, ParkActivity::Run, 3.0f);
        }
        else if (action == "relax")
        {
            if (res.personality == ParkPersonality::Energetic && res.energy > 70.0f)
            {
                setResidentEmotion(residentIndex, ParkEmotion::Sweat, 3.0f);
                return fill("{name} tries to sit still. It manages two seconds before bouncing back up.", res.name);
            }
            res.energy = std::min(100.0f, res.energy + 15.0f);
            res.bond = std::min(100, res.bond + 2);
            setResidentEmotion(residentIndex, ParkEmotion::Sleep, 3.5f);
            startActivity(residentIndex, ParkActivity::Sit, 6.0f);
        }
        if (line.empty())
            line = ParkDialogue::generateLine(res, action, nearbyName, currentHabitat);
        saveState();
        return line;
    }

    std::string PokeparkManager::feedBerry(int residentIndex, ParkFlavor flavor)
    {
        if (residentIndex < 0 || residentIndex >= static_cast<int>(residents.size()))
            return "";
        ParkResident &res = residents[residentIndex];
        const int reaction = flavorReaction(res.nature, flavor);
        const std::string berry = getFlavorName(flavor);
        std::string line;
        if (reaction > 0)
        {
            res.bond = std::min(100, res.bond + 6);
            res.energy = std::min(100.0f, res.energy + 10.0f);
            setResidentEmotion(residentIndex, ParkEmotion::Heart, 4.0f);
            startActivity(residentIndex, ParkActivity::Celebrate, 1.6f);
            line = res.name + " lights up at the " + berry + " -- clearly its favourite! It eats every last bit.";
        }
        else if (reaction < 0)
        {
            res.bond = std::max(0, res.bond - 1);
            setResidentEmotion(residentIndex, ParkEmotion::Sweat, 3.0f);
            line = res.name + " sniffs the " + berry + " and turns its nose up. Not its kind of flavour at all.";
        }
        else
        {
            res.bond = std::min(100, res.bond + 3);
            res.energy = std::min(100.0f, res.energy + 6.0f);
            setResidentEmotion(residentIndex, ParkEmotion::Music, 3.5f);
            line = res.name + " munches the " + berry + " contentedly.";
        }
        saveState();
        return line;
    }

    std::string PokeparkManager::callResident(int residentIndex)
    {
        if (residentIndex < 0 || residentIndex >= static_cast<int>(residents.size()))
            return "";
        ParkResident &res = residents[residentIndex];
        if (res.activity == ParkActivity::Nap)
        {
            setResidentEmotion(residentIndex, ParkEmotion::Sleep, 3.0f);
            return res.name + " is fast asleep. It mumbles something and rolls over.";
        }
        startActivity(residentIndex, ParkActivity::Called, 12.0f);
        res.targetX = PLAYER_X + static_cast<float>((residentIndex % 3) - 1) * 50.0f;
        res.targetY = PLAYER_Y;
        setResidentEmotion(residentIndex, res.bond >= 40 ? ParkEmotion::Exclamation : ParkEmotion::Question, 2.0f);
        return "";
    }

    // ---- Away from the park ------------------------------------------------------------------------
    // The same needs, compatibility and conflict rules as the live park, run in ten-minute steps, so
    // the journal reads like what these particular Pokemon would get up to.

    static const char *weekdayName(uint64_t t)
    {
        static const char *names[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const std::time_t raw = static_cast<std::time_t>(t);
        const std::tm *tm = std::localtime(&raw);
        return tm ? names[tm->tm_wday % 7] : "the other day";
    }

    static std::string playText(const std::string &a, const std::string &b, ParkEnvironmentId env)
    {
        switch (env)
        {
        case ParkEnvironmentId::WaterfallLake:
            return a + " and " + b + " raced drifting leaves across the shallows, cheering for both leaves at once.";
        case ParkEnvironmentId::EmberGrove:
            return a + " and " + b + " invented a careful game of hopping between comfortably warm stones.";
        case ParkEnvironmentId::SkySummit:
            return a + " and " + b + " chased cloud shadows over the plateau until they both needed a rest.";
        default:
            return a + " and " + b + " turned a fallen berry into the park's most important ball game.";
        }
    }

    static std::string restText(const std::string &a, const std::string &b, ParkEnvironmentId env)
    {
        switch (env)
        {
        case ParkEnvironmentId::WaterfallLake:
            return a + " and " + b + " dozed beside the waterfall, soothed by its steady rumble.";
        case ParkEnvironmentId::EmberGrove:
            return a + " and " + b + " shared a warm stone and watched harmless sparks drift upward.";
        case ParkEnvironmentId::SkySummit:
            return a + " and " + b + " rested in a sheltered nook while clouds sailed below them.";
        default:
            return a + " and " + b + " fell asleep back-to-back in a sunny patch of grass.";
        }
    }

    void PokeparkManager::runOfflineSimulation(uint64_t previous, uint64_t now)
    {
        if (residents.empty())
            return;
        uint64_t elapsed = std::min<uint64_t>(now - previous, 86400); // a day at most
        const int steps = static_cast<int>(elapsed / 600);             // ten-minute steps
        int written = 0;
        constexpr int maxEntries = 12;

        for (int step = 1; step <= steps; ++step)
        {
            const uint64_t t = previous + static_cast<uint64_t>(step) * 600;
            const std::time_t raw = static_cast<std::time_t>(t);
            const std::tm *tm = std::localtime(&raw);
            const int h = tm ? tm->tm_hour : 12;
            const bool night = h >= 21 || h < 6;

            // Needs drift: the day-active sleep at night, the night-active by day.
            for (auto &r : residents)
            {
                const bool resting = night != r.nocturnal;
                r.energy = std::clamp(r.energy + (resting ? 8.0f : -4.0f), 0.0f, 100.0f);
                r.social = std::min(100.0f, r.social + (r.personality == ParkPersonality::Shy ? 3.0f : 6.0f));
                r.play = std::min(100.0f, r.play + 5.0f);
                r.curiosity = std::min(100.0f, r.curiosity + 4.0f);
            }

            // Most steps nothing worth writing down happens.
            if (rand() % 100 >= 28 || residents.size() < 1)
                continue;

            const int a = rand() % static_cast<int>(residents.size());
            ParkResident &ra = residents[a];
            const ParkEnvironmentId where = ra.homeHabitat;
            const ParkEnvironmentInfo &env = getEnvironmentInfo(where);
            const bool awake = night == ra.nocturnal || ra.energy > 50.0f;

            // Choose a partner: someone it gets along with and who shares the habitat, usually.
            int b = -1, bestScore = -1000;
            for (int i = 0; i < static_cast<int>(residents.size()); ++i)
            {
                if (i == a)
                    continue;
                const ParkRelationship rel = getRelationship(ra.id, residents[i].id);
                int score = compatibility(ra, residents[i]) + rel.friendship / 2 + (rand() % 30);
                if (residents[i].homeHabitat == where)
                    score += 15;
                if (score > bestScore)
                {
                    bestScore = score;
                    b = i;
                }
            }
            const bool social = b >= 0 && ra.social > 35.0f && ra.personality != ParkPersonality::Shy
                                    ? true
                                    : (b >= 0 && rand() % 100 < 40);

            std::string text;
            ParkJournalEventKind kind = ParkJournalEventKind::Activity;
            std::vector<int> who = {a};
            if (!awake)
            {
                if (b >= 0 && getRelationship(ra.id, residents[b].id).friendship >= 40)
                {
                    text = restText(ra.name, residents[b].name, where);
                    kind = ParkJournalEventKind::Affection;
                    who.push_back(b);
                    updateRelationship(ra.id, residents[b].id, 2, 4, 0);
                }
                else
                    continue; // sleeping alone is not news
            }
            else if (social)
            {
                ParkResident &rb = residents[b];
                who.push_back(b);
                ParkRelationship rel = getRelationship(ra.id, rb.id);
                const int comp = compatibility(ra, rb);
                const int conflictChance = std::clamp(8 + rel.rivalry / 3 - comp / 3, 2, 38);
                ra.social = std::max(0.0f, ra.social - 40.0f);
                rb.social = std::max(0.0f, rb.social - 40.0f);
                if (rand() % 100 < conflictChance)
                {
                    const int mediator = findMediator(a, b);
                    updateRelationship(ra.id, rb.id, -4, 0, 6);
                    ParkRelationship &live = relationships[ParkRelationship::makeKey(ra.id, rb.id)];
                    if (mediator >= 0 && rand() % 100 < 60)
                    {
                        kind = ParkJournalEventKind::Reconciliation;
                        text = ra.name + " and " + rb.name + " squabbled over the best spot at " + env.landmark +
                               ", until " + residents[mediator].name + " stepped in and they made up.";
                        updateRelationship(ra.id, rb.id, 6, 2, -5);
                        live.lastConflictAt = 0;
                    }
                    else
                    {
                        kind = ParkJournalEventKind::Conflict;
                        text = ra.name + " and " + rb.name + " had a falling-out at " + env.landmark +
                               " and spent the afternoon pointedly ignoring each other.";
                        live.lastConflictAt = t;
                    }
                }
                else
                {
                    ParkRelationship &live = relationships[ParkRelationship::makeKey(ra.id, rb.id)];
                    if (live.lastConflictAt != 0 && t - live.lastConflictAt > 3600)
                    {
                        kind = ParkJournalEventKind::Story;
                        text = ra.name + " and " + rb.name + " finally made up after " +
                               weekdayName(live.lastConflictAt) + "'s argument.";
                        live.lastConflictAt = 0;
                        updateRelationship(ra.id, rb.id, 10, 4, -8);
                    }
                    else if (ra.play > 50.0f || ra.personality == ParkPersonality::Playful)
                    {
                        kind = ParkJournalEventKind::Friendship;
                        text = playText(ra.name, rb.name, where);
                        updateRelationship(ra.id, rb.id, 6, 3, 0);
                        ra.play = rb.play = 0.0f;
                    }
                    else
                    {
                        kind = ParkJournalEventKind::Affection;
                        text = ra.name + " and " + rb.name + " stayed close together near " + env.landmark +
                               ", watching the clouds drift by.";
                        updateRelationship(ra.id, rb.id, 4, 6, 0);
                    }
                }
                noteBonds(a, b);
            }
            else
            {
                if (ra.curiosity > 50.0f || ra.personality == ParkPersonality::Curious)
                {
                    kind = ParkJournalEventKind::Discovery;
                    static const char *finds[] = {"a sparkling, sun-warmed stone", "a feather caught on the breeze",
                                                  "a perfectly round pebble", "a trail of tiny footprints",
                                                  "a berry nobody else had spotted"};
                    text = ra.name + " found " + finds[rand() % 5] + " near " + env.landmark +
                           " and inspected it very seriously.";
                    ra.curiosity = 0.0f;
                }
                else
                {
                    text = ra.name + " took a long, unhurried stroll around " + env.name + ".";
                }
            }
            if (!text.empty() && written < maxEntries)
            {
                const ParkEnvironmentId saved = currentHabitat;
                currentHabitat = where; // the entry names where it happened
                addJournal(kind, text, who, t);
                currentHabitat = saved;
                ++written;
            }
        }
    }

    // ---- Persistence -------------------------------------------------------------------------------
    // Text, one record per line. Version 2 appends fields to the version 1 records, so an old file
    // still loads (its residents start with fresh needs).

    static std::vector<std::string> splitBars(const std::string &line)
    {
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '|'))
            parts.push_back(item);
        return parts;
    }

    void PokeparkManager::loadState()
    {
        std::ifstream file("sdmc:/PKSE/pokepark.txt");
        if (!file.is_open())
            file.open("/switch/PKSE/pokepark.txt");
        if (!file.is_open())
            return;

        residents.clear();
        journal.clear();
        relationships.clear();
        int version = 1;

        std::string section, line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;
            if (line == "[Settings]") { section = "settings"; continue; }
            if (line == "[Residents]") { section = "residents"; continue; }
            if (line == "[Journal]") { section = "journal"; continue; }
            if (line == "[Relationships]") { section = "relationships"; continue; }

            if (section == "settings")
            {
                if (line.rfind("lastSimulationTime=", 0) == 0)
                    lastSimulationTime = std::strtoull(line.substr(19).c_str(), nullptr, 10);
                else if (line.rfind("currentHabitat=", 0) == 0)
                    currentHabitat = static_cast<ParkEnvironmentId>(std::clamp(std::atoi(line.substr(15).c_str()), 0, 3));
                else if (line.rfind("version=", 0) == 0)
                    version = std::atoi(line.substr(8).c_str());
            }
            else if (section == "residents")
            {
                // v1: id|species|form|shiny|name|source|type1|type2|habitat|personality
                // v2: ...|nature|bond|energy|social|play|curiosity|favX|favY
                const auto parts = splitBars(line);
                if (parts.size() < 10)
                    continue;
                ParkResident r;
                r.id = parts[0];
                r.species = static_cast<uint16_t>(std::atoi(parts[1].c_str()));
                r.form = static_cast<uint8_t>(std::atoi(parts[2].c_str()));
                r.shiny = parts[3] == "1";
                r.name = parts[4];
                r.sourceInfo = parts[5];
                r.type1 = static_cast<uint8_t>(std::atoi(parts[6].c_str()));
                r.type2 = static_cast<uint8_t>(std::atoi(parts[7].c_str()));
                r.homeHabitat = static_cast<ParkEnvironmentId>(std::clamp(std::atoi(parts[8].c_str()), 0, 3));
                r.personality = static_cast<ParkPersonality>(std::clamp(std::atoi(parts[9].c_str()), 0, 9));
                if (parts.size() >= 18)
                {
                    r.nature = static_cast<uint8_t>(std::atoi(parts[10].c_str()));
                    r.bond = std::clamp(std::atoi(parts[11].c_str()), 0, 100);
                    r.energy = std::clamp(static_cast<float>(std::atof(parts[12].c_str())), 0.0f, 100.0f);
                    r.social = std::clamp(static_cast<float>(std::atof(parts[13].c_str())), 0.0f, 100.0f);
                    r.play = std::clamp(static_cast<float>(std::atof(parts[14].c_str())), 0.0f, 100.0f);
                    r.curiosity = std::clamp(static_cast<float>(std::atof(parts[15].c_str())), 0.0f, 100.0f);
                    r.favouriteX = static_cast<float>(std::atof(parts[16].c_str()));
                    r.favouriteY = static_cast<float>(std::atof(parts[17].c_str()));
                }
                refreshDerived(r);
                r.x = PLAY_MIN_X + 80.0f + static_cast<float>(residents.size()) * 90.0f;
                r.y = PLAY_MAX_Y - 40.0f - static_cast<float>(residents.size() % 3) * 50.0f;
                residents.push_back(r);
            }
            else if (section == "journal")
            {
                // v1: timestamp|timeStr|kind|habitat|species1|species2|text
                // v2: timestamp|timeStr|kind|habitat|species1|species2|idA,idB|text   (text may hold '|')
                const auto parts = splitBars(line);
                const size_t textAt = version >= 2 ? 7 : 6;
                if (parts.size() <= textAt)
                    continue;
                ParkJournalEvent ev;
                ev.timestamp = std::strtoull(parts[0].c_str(), nullptr, 10);
                ev.timeStr = parts[1];
                ev.kind = static_cast<ParkJournalEventKind>(std::atoi(parts[2].c_str()));
                ev.habitatName = parts[3];
                ev.species1 = static_cast<uint16_t>(std::atoi(parts[4].c_str()));
                ev.species2 = static_cast<uint16_t>(std::atoi(parts[5].c_str()));
                if (version >= 2 && !parts[6].empty())
                {
                    std::stringstream ids(parts[6]);
                    std::string id;
                    while (std::getline(ids, id, ','))
                        ev.residentIds.push_back(id);
                }
                for (size_t i = textAt; i < parts.size(); ++i)
                    ev.text += (i > textAt ? "|" : "") + parts[i];
                journal.push_back(ev);
            }
            else if (section == "relationships")
            {
                // idA|idB|friendship|affection|rivalry|moments|lastEvent[|lastConflict|announced]
                const auto parts = splitBars(line);
                if (parts.size() < 7)
                    continue;
                ParkRelationship rel;
                rel.firstId = parts[0];
                rel.secondId = parts[1];
                rel.friendship = std::atoi(parts[2].c_str());
                rel.affection = std::atoi(parts[3].c_str());
                rel.rivalry = std::atoi(parts[4].c_str());
                rel.sharedMoments = std::atoi(parts[5].c_str());
                rel.lastEventAt = std::strtoull(parts[6].c_str(), nullptr, 10);
                if (parts.size() >= 9)
                {
                    rel.lastConflictAt = std::strtoull(parts[7].c_str(), nullptr, 10);
                    rel.announced = static_cast<uint8_t>(std::atoi(parts[8].c_str()));
                }
                relationships[rel.key()] = rel;
            }
        }
    }

    void PokeparkManager::saveState()
    {
        std::ofstream file("sdmc:/PKSE/pokepark.txt");
        if (!file.is_open())
            file.open("/switch/PKSE/pokepark.txt");
        if (!file.is_open())
            return;

        file << "[Settings]\n";
        file << "version=2\n";
        file << "lastSimulationTime=" << lastSimulationTime << "\n";
        file << "currentHabitat=" << static_cast<int>(currentHabitat) << "\n\n";

        file << "[Residents]\n";
        for (const auto &r : residents)
        {
            file << r.id << "|" << r.species << "|" << static_cast<int>(r.form) << "|" << (r.shiny ? "1" : "0") << "|"
                 << r.name << "|" << r.sourceInfo << "|" << static_cast<int>(r.type1) << "|"
                 << static_cast<int>(r.type2) << "|" << static_cast<int>(r.homeHabitat) << "|"
                 << static_cast<int>(r.personality) << "|" << static_cast<int>(r.nature) << "|" << r.bond << "|"
                 << static_cast<int>(r.energy) << "|" << static_cast<int>(r.social) << "|"
                 << static_cast<int>(r.play) << "|" << static_cast<int>(r.curiosity) << "|"
                 << static_cast<int>(r.favouriteX) << "|" << static_cast<int>(r.favouriteY) << "\n";
        }
        file << "\n";

        file << "[Journal]\n";
        for (const auto &ev : journal)
        {
            std::string ids;
            for (size_t i = 0; i < ev.residentIds.size(); ++i)
                ids += (i ? "," : "") + ev.residentIds[i];
            file << ev.timestamp << "|" << ev.timeStr << "|" << static_cast<int>(ev.kind) << "|" << ev.habitatName
                 << "|" << ev.species1 << "|" << ev.species2 << "|" << ids << "|" << ev.text << "\n";
        }
        file << "\n";

        file << "[Relationships]\n";
        for (const auto &pair : relationships)
        {
            const auto &rel = pair.second;
            file << rel.firstId << "|" << rel.secondId << "|" << rel.friendship << "|" << rel.affection << "|"
                 << rel.rivalry << "|" << rel.sharedMoments << "|" << rel.lastEventAt << "|" << rel.lastConflictAt
                 << "|" << static_cast<int>(rel.announced) << "\n";
        }
    }

    namespace ParkDialogue
    {
        static std::string replacePlaceholders(const std::string &tpl, const std::string &name, const std::string &nearby)
        {
            std::string res = tpl;
            size_t pos = 0;
            while ((pos = res.find("{name}", pos)) != std::string::npos)
            {
                res.replace(pos, 6, name);
                pos += name.length();
            }
            pos = 0;
            std::string nearStr = nearby.empty() ? "a friend" : nearby;
            while ((pos = res.find("{nearby}", pos)) != std::string::npos)
            {
                res.replace(pos, 8, nearStr);
                pos += nearStr.length();
            }
            return res;
        }

        std::string generateLine(const ParkResident &resident, const std::string &action,
                                 const std::string &nearbyName, ParkEnvironmentId env)
        {
            // Species-specific signatures
            if (resident.species == 25) // Pikachu
            {
                if (action == "talk") return replacePlaceholders("{name}'s cheeks crackle softly as it leans closer. It seems delighted you stopped by!", resident.name, nearbyName);
                if (action == "play") return replacePlaceholders("{name} zigzags excitedly in the grass, challenging you to a cheerful sprint!", resident.name, nearbyName);
                if (action == "snack") return replacePlaceholders("{name} holds the berry with both paws and nibbles happily, emitting tiny happy sparks.", resident.name, nearbyName);
            }
            else if (resident.species == 133) // Eevee
            {
                if (action == "talk") return replacePlaceholders("{name} tilts its head with bright, curious eyes, listening intently to your voice.", resident.name, nearbyName);
                if (action == "play") return replacePlaceholders("{name} bounds in circles and pounces playfully at a drifting leaf!", resident.name, nearbyName);
                if (action == "relax") return replacePlaceholders("A mountain breeze ruffles {name}'s fluffy mane. It gives a happy shake and curls up beside you.", resident.name, nearbyName);
            }
            else if (resident.species == 143) // Snorlax
            {
                if (action == "snack") return replacePlaceholders("{name} accepts the snack with solemn gratitude, then immediately checks whether there is another.", resident.name, nearbyName);
                if (action == "relax" || action == "talk") return replacePlaceholders("{name} smiles serenely in its sleep. Whatever it is dreaming about must smell delicious.", resident.name, nearbyName);
            }
            else if (resident.species == 448) // Lucario
            {
                if (action == "talk") return replacePlaceholders("{name} closes its eyes. For a quiet moment, it senses your bond through a calm pulse of aura.", resident.name, nearbyName);
                if (action == "relax") return replacePlaceholders("{name} sits cross-legged beside you, meditating in perfect harmony with the breeze.", resident.name, nearbyName);
            }
            else if (resident.species == 94) // Gengar
            {
                if (action == "play") return replacePlaceholders("{name} sinks into your shadow and pops out behind {nearby} with a wide, harmless grin!", resident.name, nearbyName);
            }

            // Action-driven responses
            if (action == "snack")
            {
                const char *snackLines[] = {
                    "{name} happily accepts the treat, savoring every sweet bite!",
                    "{name} sniffs the berry carefully, then munches with joyful crunches.",
                    "{name} holds the berry proudly and offers half to {nearby} before eating.",
                    "{name} finishes the snack in an instant and licks its paws with a contented sigh."
                };
                return replacePlaceholders(snackLines[rand() % 4], resident.name, nearbyName);
            }
            else if (action == "play")
            {
                const char *playLines[] = {
                    "{name} darts away, pauses to look back, and starts an enthusiastic game of chase!",
                    "{name} does a joyful little pirouette and invites everyone to join in.",
                    "{name} playfully nudges a pebble toward {nearby} like a game of football!",
                    "{name} leaps gracefully into the air and lands with an energetic flourish."
                };
                return replacePlaceholders(playLines[rand() % 4], resident.name, nearbyName);
            }
            else if (action == "relax")
            {
                const char *relaxLines[] = {
                    "{name} settles down beside you. For a quiet moment, you breathe at the same unhurried pace.",
                    "{name} stretches out in the warm grass with a long, peaceful yawn.",
                    "{name} closes its eyes and listens to the gentle rustle of leaves.",
                    "{name} curls into a cozy ball, safe and happy in the sanctuary."
                };
                return replacePlaceholders(relaxLines[rand() % 4], resident.name, nearbyName);
            }

            // General talk lines based on personality
            switch (resident.personality)
            {
            case ParkPersonality::Playful:
                return replacePlaceholders("{name} circles behind you and taps your shoulder from the other side with a giggle!", resident.name, nearbyName);
            case ParkPersonality::Gentle:
                return replacePlaceholders("{name} gives you a warm, affectionate nuzzle, making sure {nearby} feels included too.", resident.name, nearbyName);
            case ParkPersonality::Bold:
                return replacePlaceholders("{name} strikes an impressive pose, showing off its strength and confidence!", resident.name, nearbyName);
            case ParkPersonality::Shy:
                return replacePlaceholders("{name} watches from a few steps away, but takes a brave step closer as you smile.", resident.name, nearbyName);
            case ParkPersonality::Curious:
                return replacePlaceholders("{name} tilts its head and studies your hands with deep, fascinated wonder.", resident.name, nearbyName);
            case ParkPersonality::Energetic:
                return replacePlaceholders("{name} stamps out a cheerful rhythm, full of boundless energy for the day!", resident.name, nearbyName);
            case ParkPersonality::Calm:
                return replacePlaceholders("The bustling energy of the park softens around {name}'s serene, peaceful presence.", resident.name, nearbyName);
            case ParkPersonality::Helpful:
                return replacePlaceholders("{name} brings over a fresh sprig of flowers and neatly places it near you.", resident.name, nearbyName);
            case ParkPersonality::Proud:
                return replacePlaceholders("{name} holds its head high and proudly awaits your well-deserved admiration.", resident.name, nearbyName);
            case ParkPersonality::Balanced:
            default:
                return replacePlaceholders("{name} leans closer as if sharing an important secret. Trust and friendship fill the air.", resident.name, nearbyName);
            }
        }
    }
}
