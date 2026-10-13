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
        case ParkMood::Content:
        default:                return "Content";
        }
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

        loadState();

        if (residents.empty() && trainer)
        {
            autoFillRoster(trainer, bank);
        }

        uint64_t now = static_cast<uint64_t>(std::time(nullptr));
        if (lastSimulationTime > 0 && now > lastSimulationTime)
        {
            uint64_t elapsed = now - lastSimulationTime;
            if (elapsed >= 1200) // >= 20 minutes
            {
                runOfflineSimulation(lastSimulationTime, now);
            }
        }
        lastSimulationTime = now;
        saveState();
    }

    void PokeparkManager::cycleHabitat(int direction) noexcept
    {
        int id = static_cast<int>(currentHabitat) + direction;
        if (id < 0) id = 3;
        else if (id > 3) id = 0;
        currentHabitat = static_cast<ParkEnvironmentId>(id);
    }

    std::vector<int> PokeparkManager::getVisibleResidentIndices(ParkEnvironmentId env) const
    {
        std::vector<int> result;
        for (size_t i = 0; i < residents.size(); ++i)
        {
            if (residents[i].enjoysHabitat(env))
            {
                result.push_back(static_cast<int>(i));
            }
        }
        return result;
    }

    void PokeparkManager::clearJournal()
    {
        journal.clear();
        saveState();
    }

    ParkRelationship PokeparkManager::getRelationship(const std::string &idA, const std::string &idB)
    {
        std::string key = ParkRelationship::makeKey(idA, idB);
        auto it = relationships.find(key);
        if (it != relationships.end())
            return it->second;

        ParkRelationship fresh;
        fresh.firstId = idA;
        fresh.secondId = idB;
        return fresh;
    }

    void PokeparkManager::updateRelationship(const std::string &idA, const std::string &idB,
                                             int dFriendship, int dAffection, int dRivalry)
    {
        std::string key = ParkRelationship::makeKey(idA, idB);
        ParkRelationship &rel = relationships[key];
        rel.firstId = idA;
        rel.secondId = idB;
        rel.friendship = std::clamp(rel.friendship + dFriendship, -100, 100);
        rel.affection = std::clamp(rel.affection + dAffection, 0, 100);
        rel.rivalry = std::clamp(rel.rivalry + dRivalry, 0, 100);
        rel.sharedMoments++;
        rel.lastEventAt = static_cast<uint64_t>(std::time(nullptr));
    }

    void PokeparkManager::setResidentEmotion(int residentIndex, ParkEmotion emotion, float duration)
    {
        if (residentIndex >= 0 && residentIndex < static_cast<int>(residents.size()))
        {
            residents[residentIndex].emotion = emotion;
            residents[residentIndex].emotionTimer = duration;
        }
    }

    void PokeparkManager::autoFillRoster(Trainer::Trainer *trainer, Trainer::Bank *bank)
    {
        if (!trainer) return;
        residents.clear();

        // 1. Prioritize live party members
        for (size_t i = 0; i < trainer->party.size() && residents.size() < DEFAULT_RESIDENTS; ++i)
        {
            const auto &mon = trainer->party[i];
            if (mon && mon->speciesID() != 0 && !mon->isEgg())
            {
                ParkResident res;
                res.id = "party:" + std::to_string(i);
                res.species = mon->speciesID();
                res.form = mon->form();
                res.shiny = mon->isShiny(mon->id32(), mon->species());
                res.name = mon->nickname().empty() ? std::string(mon->species()) : Utils::utf16ToUtf8(mon->nickname());
                res.sourceInfo = "Party Slot " + std::to_string(i + 1);

                auto types = Pokemon::getPokemonTypes(res.species, res.form, trainer->getGameGroup());
                res.type1 = types.type1;
                res.type2 = types.type2;
                res.personality = derivePersonality(mon->nature(), mon->id32());

                if (res.type1 == Pokemon::TYPE_WATER || res.type2 == Pokemon::TYPE_WATER)
                    res.homeHabitat = ParkEnvironmentId::WaterfallLake;
                else if (res.type1 == Pokemon::TYPE_FIRE || res.type1 == Pokemon::TYPE_GROUND || res.type1 == Pokemon::TYPE_ROCK)
                    res.homeHabitat = ParkEnvironmentId::EmberGrove;
                else if (res.type1 == Pokemon::TYPE_FLYING || res.type1 == Pokemon::TYPE_ICE)
                    res.homeHabitat = ParkEnvironmentId::SkySummit;
                else
                    res.homeHabitat = ParkEnvironmentId::Commons;

                res.x = 100.0f + (residents.size() * 120.0f);
                res.y = 280.0f + ((residents.size() % 3) * 60.0f);
                res.targetX = res.x;
                res.targetY = res.y;
                residents.push_back(res);
            }
        }

        // 2. Sample unique box Pokémon
        for (size_t b = 0; b < trainer->boxes.size() && residents.size() < DEFAULT_RESIDENTS; ++b)
        {
            for (size_t s = 0; s < trainer->boxes[b].size() && residents.size() < DEFAULT_RESIDENTS; ++s)
            {
                const auto &mon = trainer->boxes[b][s];
                if (!mon || mon->speciesID() == 0 || mon->isEgg())
                    continue;

                // Avoid duplicate species in initial roster
                bool duplicate = false;
                for (const auto &r : residents)
                {
                    if (r.species == mon->speciesID())
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;

                ParkResident res;
                res.id = "box:" + std::to_string(b) + ":" + std::to_string(s);
                res.species = mon->speciesID();
                res.form = mon->form();
                res.shiny = mon->isShiny(mon->id32(), mon->species());
                res.name = mon->nickname().empty() ? std::string(mon->species()) : Utils::utf16ToUtf8(mon->nickname());
                res.sourceInfo = "Box " + std::to_string(b + 1) + " Slot " + std::to_string(s + 1);

                auto types = Pokemon::getPokemonTypes(res.species, res.form, trainer->getGameGroup());
                res.type1 = types.type1;
                res.type2 = types.type2;
                res.personality = derivePersonality(mon->nature(), mon->id32());

                if (res.type1 == Pokemon::TYPE_WATER || res.type2 == Pokemon::TYPE_WATER)
                    res.homeHabitat = ParkEnvironmentId::WaterfallLake;
                else if (res.type1 == Pokemon::TYPE_FIRE || res.type1 == Pokemon::TYPE_GROUND || res.type1 == Pokemon::TYPE_ROCK)
                    res.homeHabitat = ParkEnvironmentId::EmberGrove;
                else if (res.type1 == Pokemon::TYPE_FLYING || res.type1 == Pokemon::TYPE_ICE)
                    res.homeHabitat = ParkEnvironmentId::SkySummit;
                else
                    res.homeHabitat = ParkEnvironmentId::Commons;

                res.x = 120.0f + (residents.size() * 110.0f);
                res.y = 260.0f + ((residents.size() % 4) * 55.0f);
                res.targetX = res.x;
                res.targetY = res.y;
                residents.push_back(res);
            }
        }

        saveState();
    }

    void PokeparkManager::removeResident(int residentIndex)
    {
        if (residentIndex >= 0 && residentIndex < static_cast<int>(residents.size()))
        {
            residents.erase(residents.begin() + residentIndex);
            saveState();
        }
    }

    void PokeparkManager::addResidentFromSave(int box, int slot, const Trainer::Trainer &trainer)
    {
        if (residents.size() >= MAX_RESIDENTS) return;
        if (box < 0 || box >= static_cast<int>(trainer.boxes.size())) return;
        if (slot < 0 || slot >= static_cast<int>(trainer.boxes[box].size())) return;

        const auto &mon = trainer.boxes[box][slot];
        if (!mon || mon->speciesID() == 0 || mon->isEgg()) return;

        ParkResident res;
        res.id = "box:" + std::to_string(box) + ":" + std::to_string(slot);
        res.species = mon->speciesID();
        res.form = mon->form();
        res.shiny = mon->isShiny(mon->id32(), mon->species());
        res.name = mon->nickname().empty() ? std::string(mon->species()) : Utils::utf16ToUtf8(mon->nickname());
        res.sourceInfo = "Box " + std::to_string(box + 1) + " Slot " + std::to_string(slot + 1);

        auto types = Pokemon::getPokemonTypes(res.species, res.form, trainer.getGameGroup());
        res.type1 = types.type1;
        res.type2 = types.type2;
        res.personality = derivePersonality(mon->nature(), mon->id32());

        res.x = 640.0f;
        res.y = 350.0f;
        res.targetX = res.x;
        res.targetY = res.y;
        residents.push_back(res);
        saveState();
    }

    void PokeparkManager::stepResidentMotion(ParkResident &res, float dt, float minX, float maxX, float minY, float maxY)
    {
        res.animClock += dt;

        if (res.emotionTimer > 0.0f)
        {
            res.emotionTimer -= dt;
            if (res.emotionTimer <= 0.0f)
                res.emotion = ParkEmotion::None;
        }

        if (res.interactionCooldown > 0.0f)
            res.interactionCooldown -= dt;

        res.wanderTimer -= dt;
        if (res.wanderTimer <= 0.0f)
        {
            // Pick new wander target or pause
            if (!res.isMoving)
            {
                // Start moving
                res.targetX = minX + static_cast<float>(rand() % static_cast<int>(maxX - minX + 1.0f));
                res.targetY = minY + static_cast<float>(rand() % static_cast<int>(maxY - minY + 1.0f));
                res.isMoving = true;
                res.wanderTimer = 3.0f + static_cast<float>(rand() % 50) / 10.0f;
                res.facingLeft = (res.targetX < res.x);
            }
            else
            {
                // Pause and rest/observe
                res.isMoving = false;
                res.wanderTimer = 2.0f + static_cast<float>(rand() % 60) / 10.0f;

                // Random casual mood shift
                int roll = rand() % 100;
                if (roll < 20) res.mood = ParkMood::Content;
                else if (roll < 35) res.mood = ParkMood::Curious;
                else if (roll < 45) res.mood = ParkMood::Sleepy;
            }
        }

        if (res.isMoving)
        {
            float dx = res.targetX - res.x;
            float dy = res.targetY - res.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > 5.0f)
            {
                float speed = 40.0f; // pixels per second
                res.vx = (dx / dist) * speed;
                res.vy = (dy / dist) * speed;
                res.x += res.vx * dt;
                res.y += res.vy * dt;
                res.facingLeft = (res.vx < 0.0f);
            }
            else
            {
                res.isMoving = false;
                res.wanderTimer = 2.0f + static_cast<float>(rand() % 40) / 10.0f;
            }
        }

        // Clamp to habitat play bounds
        res.x = std::clamp(res.x, minX, maxX);
        res.y = std::clamp(res.y, minY, maxY);
    }

    void PokeparkManager::checkRandomInteractions(float dt)
    {
        auto visible = getVisibleResidentIndices(currentHabitat);
        if (visible.size() < 2) return;

        for (size_t i = 0; i < visible.size(); ++i)
        {
            int idxA = visible[i];
            ParkResident &resA = residents[idxA];
            if (resA.interactionCooldown > 0.0f) continue;

            for (size_t j = i + 1; j < visible.size(); ++j)
            {
                int idxB = visible[j];
                ParkResident &resB = residents[idxB];
                if (resB.interactionCooldown > 0.0f) continue;

                float dx = resA.x - resB.x;
                float dy = resA.y - resB.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 75.0f)
                {
                    // Encounter interaction!
                    resA.interactionCooldown = 8.0f;
                    resB.interactionCooldown = 8.0f;
                    resA.isMoving = false;
                    resB.isMoving = false;
                    resA.wanderTimer = 4.0f;
                    resB.wanderTimer = 4.0f;
                    resA.facingLeft = (resB.x < resA.x);
                    resB.facingLeft = (resA.x < resB.x);

                    int roll = rand() % 100;
                    if (roll < 35)
                    {
                        // Playful encounter
                        resA.emotion = ParkEmotion::Music;
                        resB.emotion = ParkEmotion::Music;
                        resA.emotionTimer = 3.5f;
                        resB.emotionTimer = 3.5f;
                        updateRelationship(resA.id, resB.id, 5, 2, 0);
                    }
                    else if (roll < 65)
                    {
                        // Affectionate encounter
                        resA.emotion = ParkEmotion::Heart;
                        resB.emotion = ParkEmotion::Heart;
                        resA.emotionTimer = 3.5f;
                        resB.emotionTimer = 3.5f;
                        updateRelationship(resA.id, resB.id, 3, 5, 0);
                    }
                    else if (roll < 85)
                    {
                        // Curious inspection
                        resA.emotion = ParkEmotion::Question;
                        resB.emotion = ParkEmotion::Exclamation;
                        resA.emotionTimer = 3.0f;
                        resB.emotionTimer = 3.0f;
                        updateRelationship(resA.id, resB.id, 2, 1, 0);
                    }
                    else
                    {
                        // Playful sparring / rivalry
                        resA.emotion = ParkEmotion::Angry;
                        resB.emotion = ParkEmotion::Angry;
                        resA.emotionTimer = 2.5f;
                        resB.emotionTimer = 2.5f;
                        updateRelationship(resA.id, resB.id, 1, 0, 4);
                    }
                }
            }
        }
    }

    void PokeparkManager::update(float dt)
    {
        realTimeUpdateClock += dt;

        // Habitat bounding areas (inside the 1248x588 content panel)
        constexpr float minX = 80.0f;
        constexpr float maxX = 1160.0f;
        constexpr float minY = 200.0f;
        constexpr float maxY = 510.0f;

        for (auto &res : residents)
        {
            stepResidentMotion(res, dt, minX, maxX, minY, maxY);
        }

        checkRandomInteractions(dt);
    }

    std::string PokeparkManager::interact(int residentIndex, const std::string &action)
    {
        if (residentIndex < 0 || residentIndex >= static_cast<int>(residents.size()))
            return "";

        ParkResident &res = residents[residentIndex];
        std::string nearbyName;

        auto visible = getVisibleResidentIndices(currentHabitat);
        for (int vIdx : visible)
        {
            if (vIdx != residentIndex)
            {
                nearbyName = residents[vIdx].name;
                break;
            }
        }

        if (action == "talk")
        {
            setResidentEmotion(residentIndex, ParkEmotion::Music, 3.5f);
            res.mood = ParkMood::Content;
        }
        else if (action == "play")
        {
            setResidentEmotion(residentIndex, ParkEmotion::Heart, 3.5f);
            res.mood = ParkMood::Playful;
        }
        else if (action == "snack")
        {
            setResidentEmotion(residentIndex, ParkEmotion::Heart, 4.0f);
            res.mood = ParkMood::Content;
        }
        else if (action == "relax")
        {
            setResidentEmotion(residentIndex, ParkEmotion::Sleep, 3.5f);
            res.mood = ParkMood::Sleepy;
        }

        std::string line = ParkDialogue::generateLine(res, action, nearbyName, currentHabitat);
        saveState();
        return line;
    }

    void PokeparkManager::runOfflineSimulation(uint64_t previous, uint64_t now)
    {
        if (residents.empty()) return;

        uint64_t elapsedSecs = now - previous;
        // Limit max offline time to 24 hours (86400s)
        if (elapsedSecs > 86400) elapsedSecs = 86400;

        // 1 event per ~70 minutes (4200s), capped at 8 events
        int eventCount = std::clamp(static_cast<int>(elapsedSecs / 4200), 1, 8);

        char timeBuffer[64];
        std::time_t rawNow = static_cast<std::time_t>(now);
        struct tm *tmInfo = std::localtime(&rawNow);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%b %d, %H:%M", tmInfo);

        for (int i = 0; i < eventCount; ++i)
        {
            uint64_t eventTime = previous + (elapsedSecs * (i + 1)) / (eventCount + 1);
            std::time_t rawEventTime = static_cast<std::time_t>(eventTime);
            struct tm *eTm = std::localtime(&rawEventTime);
            char eTimeBuf[64];
            std::strftime(eTimeBuf, sizeof(eTimeBuf), "%H:%M", eTm);

            ParkJournalEvent ev;
            ev.timestamp = eventTime;
            ev.timeStr = eTimeBuf;

            int firstIdx = rand() % residents.size();
            const auto &first = residents[firstIdx];
            const auto &habitatInfo = getEnvironmentInfo(first.homeHabitat);
            ev.habitatName = habitatInfo.name;
            ev.species1 = first.species;

            // Decide whether solo or social
            if (residents.size() > 1 && (rand() % 100) < 60)
            {
                int secondIdx = rand() % residents.size();
                while (secondIdx == firstIdx)
                    secondIdx = rand() % residents.size();
                const auto &second = residents[secondIdx];
                ev.species2 = second.species;
                ev.residentIds = {first.id, second.id};

                int roll = rand() % 100;
                if (roll < 20)
                {
                    ev.kind = ParkJournalEventKind::Reconciliation;
                    ev.text = first.name + " and " + second.name + " had a small dispute over the best shade at " +
                              habitatInfo.landmark + ", but shared a snack and quickly made amends.";
                    updateRelationship(first.id, second.id, 6, 2, -2);
                }
                else if (roll < 45)
                {
                    ev.kind = ParkJournalEventKind::Affection;
                    ev.text = first.name + " and " + second.name + " stayed close together near " +
                              habitatInfo.landmark + " watching the clouds drift by.";
                    updateRelationship(first.id, second.id, 4, 8, 0);
                }
                else
                {
                    ev.kind = ParkJournalEventKind::Friendship;
                    ev.text = first.name + " invited " + second.name + " to explore " +
                              habitatInfo.landmark + ". Both returned in high spirits!";
                    updateRelationship(first.id, second.id, 8, 3, 0);
                }
            }
            else
            {
                ev.residentIds = {first.id};
                int roll = rand() % 100;
                if (roll < 50)
                {
                    ev.kind = ParkJournalEventKind::Discovery;
                    ev.text = first.name + " discovered a sparkling, sun-warmed stone near " +
                              habitatInfo.landmark + " and proudly inspected it.";
                }
                else
                {
                    ev.kind = ParkJournalEventKind::Activity;
                    ev.text = first.name + " took a leisurely stroll across " +
                              habitatInfo.name + " and relaxed in the gentle breeze.";
                }
            }

            journal.push_back(ev);
        }

        // Keep journal bounded
        while (journal.size() > MAX_JOURNAL_ENTRIES)
        {
            journal.erase(journal.begin());
        }
    }

    void PokeparkManager::loadState()
    {
        std::ifstream file("/switch/PKSE/pokepark.txt");
        if (!file.is_open())
            file.open("sdmc:/PKSE/pokepark.txt");
        if (!file.is_open())
            return;

        residents.clear();
        journal.clear();
        relationships.clear();

        std::string section;
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;
            if (line == "[Settings]") { section = "settings"; continue; }
            if (line == "[Residents]") { section = "residents"; continue; }
            if (line == "[Journal]") { section = "journal"; continue; }
            if (line == "[Relationships]") { section = "relationships"; continue; }

            if (section == "settings")
            {
                if (line.rfind("lastSimulationTime=", 0) == 0)
                {
                    lastSimulationTime = std::strtoull(line.substr(19).c_str(), nullptr, 10);
                }
                else if (line.rfind("currentHabitat=", 0) == 0)
                {
                    currentHabitat = static_cast<ParkEnvironmentId>(std::atoi(line.substr(15).c_str()));
                }
            }
            else if (section == "residents")
            {
                // Format: id|species|form|shiny|name|source|type1|type2|habitat|personality
                std::stringstream ss(line);
                std::string item;
                std::vector<std::string> parts;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 10)
                {
                    ParkResident r;
                    r.id = parts[0];
                    r.species = static_cast<uint16_t>(std::atoi(parts[1].c_str()));
                    r.form = static_cast<uint8_t>(std::atoi(parts[2].c_str()));
                    r.shiny = (parts[3] == "1");
                    r.name = parts[4];
                    r.sourceInfo = parts[5];
                    r.type1 = static_cast<uint8_t>(std::atoi(parts[6].c_str()));
                    r.type2 = static_cast<uint8_t>(std::atoi(parts[7].c_str()));
                    r.homeHabitat = static_cast<ParkEnvironmentId>(std::atoi(parts[8].c_str()));
                    r.personality = static_cast<ParkPersonality>(std::atoi(parts[9].c_str()));

                    r.x = 150.0f + (residents.size() * 110.0f);
                    r.y = 280.0f + ((residents.size() % 3) * 60.0f);
                    r.targetX = r.x;
                    r.targetY = r.y;
                    residents.push_back(r);
                }
            }
            else if (section == "journal")
            {
                // Format: timestamp|timeStr|kind|habitatName|species1|species2|text
                std::stringstream ss(line);
                std::string item;
                std::vector<std::string> parts;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 7)
                {
                    ParkJournalEvent ev;
                    ev.timestamp = std::strtoull(parts[0].c_str(), nullptr, 10);
                    ev.timeStr = parts[1];
                    ev.kind = static_cast<ParkJournalEventKind>(std::atoi(parts[2].c_str()));
                    ev.habitatName = parts[3];
                    ev.species1 = static_cast<uint16_t>(std::atoi(parts[4].c_str()));
                    ev.species2 = static_cast<uint16_t>(std::atoi(parts[5].c_str()));
                    ev.text = parts[6];
                    journal.push_back(ev);
                }
            }
            else if (section == "relationships")
            {
                // Format: idA|idB|friendship|affection|rivalry|moments|lastEvent
                std::stringstream ss(line);
                std::string item;
                std::vector<std::string> parts;
                while (std::getline(ss, item, '|')) parts.push_back(item);
                if (parts.size() >= 7)
                {
                    ParkRelationship rel;
                    rel.firstId = parts[0];
                    rel.secondId = parts[1];
                    rel.friendship = std::atoi(parts[2].c_str());
                    rel.affection = std::atoi(parts[3].c_str());
                    rel.rivalry = std::atoi(parts[4].c_str());
                    rel.sharedMoments = std::atoi(parts[5].c_str());
                    rel.lastEventAt = std::strtoull(parts[6].c_str(), nullptr, 10);
                    relationships[rel.key()] = rel;
                }
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
        file << "lastSimulationTime=" << lastSimulationTime << "\n";
        file << "currentHabitat=" << static_cast<int>(currentHabitat) << "\n\n";

        file << "[Residents]\n";
        for (const auto &r : residents)
        {
            file << r.id << "|" << r.species << "|" << static_cast<int>(r.form) << "|"
                 << (r.shiny ? "1" : "0") << "|" << r.name << "|" << r.sourceInfo << "|"
                 << static_cast<int>(r.type1) << "|" << static_cast<int>(r.type2) << "|"
                 << static_cast<int>(r.homeHabitat) << "|" << static_cast<int>(r.personality) << "\n";
        }
        file << "\n";

        file << "[Journal]\n";
        for (const auto &ev : journal)
        {
            file << ev.timestamp << "|" << ev.timeStr << "|" << static_cast<int>(ev.kind) << "|"
                 << ev.habitatName << "|" << ev.species1 << "|" << ev.species2 << "|"
                 << ev.text << "\n";
        }
        file << "\n";

        file << "[Relationships]\n";
        for (const auto &pair : relationships)
        {
            const auto &rel = pair.second;
            file << rel.firstId << "|" << rel.secondId << "|" << rel.friendship << "|"
                 << rel.affection << "|" << rel.rivalry << "|" << rel.sharedMoments << "|"
                 << rel.lastEventAt << "\n";
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
