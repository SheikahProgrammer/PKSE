// The live park: terrain, each resident's needs-driven behaviour, meetings and relationships,
// toys, wild visitors, weather and the little effects that go with them. Pokepark.cpp holds the
// roster, persistence, player care actions and the away-from-park simulation.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "UI/Pokepark.h"
#include "Pokemon/PokemonTypes.h"

namespace UI
{
    namespace
    {
        float frand() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
        float frange(float lo, float hi) { return lo + (hi - lo) * frand(); }
        float dist(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

        bool inEllipse(float x, float y, float cx, float cy, float rx, float ry)
        {
            const float dx = (x - cx) / rx, dy = (y - cy) / ry;
            return dx * dx + dy * dy <= 1.0f;
        }

        // Activities a resident is busy with and should not be pulled out of by a passer-by.
        bool absorbed(ParkActivity a)
        {
            switch (a)
            {
            case ParkActivity::Nap:
            case ParkActivity::Eat:
            case ParkActivity::Called:
            case ParkActivity::Spar:
            case ParkActivity::Mediate:
            case ParkActivity::Fetch:
            case ParkActivity::Flee:
                return true;
            default:
                return false;
            }
        }

        bool moves(ParkActivity a)
        {
            switch (a)
            {
            case ParkActivity::Idle:
            case ParkActivity::Sit:
            case ParkActivity::Nap:
            case ParkActivity::Eat:
            case ParkActivity::Watch:
            case ParkActivity::Sulk:
                return false;
            default:
                return true;
            }
        }

        // Wild Pokemon that may wander through, by habitat.
        const uint16_t VISITORS[4][6] = {
            {16, 19, 10, 399, 506, 819},   // Commons: Pidgey, Rattata, Caterpie, Bidoof, Lillipup, Skwovet
            {54, 60, 194, 270, 183, 418},  // Lake: Psyduck, Poliwag, Wooper, Lotad, Marill, Buizel
            {77, 58, 322, 667, 837, 27},   // Grove: Ponyta, Growlithe, Numel, Litleo, Rolycoly, Sandshrew
            {333, 361, 396, 519, 220, 821} // Summit: Swablu, Snorunt, Starly, Pidove, Swinub, Rookidee
        };
        const char *VISITOR_NAMES[4][6] = {
            {"Pidgey", "Rattata", "Caterpie", "Bidoof", "Lillipup", "Skwovet"},
            {"Psyduck", "Poliwag", "Wooper", "Lotad", "Marill", "Buizel"},
            {"Ponyta", "Growlithe", "Numel", "Litleo", "Rolycoly", "Sandshrew"},
            {"Swablu", "Snorunt", "Starly", "Pidove", "Swinub", "Rookidee"}};
    }

    // ---- Terrain -------------------------------------------------------------------------------
    // Traced from the scenery PokeparkPanel draws, in the same scene coordinates, so a resident's
    // feet never land on the cliff or the clouds and only Water types wade into the lake.

    ParkTerrain PokeparkManager::terrainAt(ParkEnvironmentId env, float x, float y)
    {
        if (x < PLAY_MIN_X || x > PLAY_MAX_X || y < PLAY_MIN_Y || y > PLAY_MAX_Y)
            return ParkTerrain::Blocked;
        switch (env)
        {
        case ParkEnvironmentId::Commons:
            if (inEllipse(x, y, 1010.0f, 268.0f, 30.0f, 16.0f)) // the berry tree's trunk
                return ParkTerrain::Blocked;
            return ParkTerrain::Land;
        case ParkEnvironmentId::WaterfallLake:
            if (x < 150.0f) // the waterfall cliff
                return ParkTerrain::Blocked;
            if (inEllipse(x, y, 580.0f, 518.0f, 470.0f, 112.0f))
                return ParkTerrain::Water;
            return ParkTerrain::Land;
        case ParkEnvironmentId::EmberGrove:
            if (inEllipse(x, y, 488.0f, 488.0f, 40.0f, 30.0f) || inEllipse(x, y, 748.0f, 503.0f, 46.0f, 34.0f))
                return ParkTerrain::Warm;
            return ParkTerrain::Land;
        case ParkEnvironmentId::SkySummit:
            if (inEllipse(x, y, 930.0f, 392.0f, 46.0f, 20.0f)) // the lookout rock's base
                return ParkTerrain::Blocked;
            return y < 398.0f ? ParkTerrain::Air : ParkTerrain::Land;
        }
        return ParkTerrain::Land;
    }

    const std::vector<ParkLandmark> &PokeparkManager::getLandmarks(ParkEnvironmentId env)
    {
        static const std::vector<ParkLandmark> marks[4] = {
            {{ParkLandmarkKind::BerryTree, 1010.0f, 300.0f, 80.0f, "the berry tree"}},
            {{ParkLandmarkKind::Waterfall, 200.0f, 470.0f, 70.0f, "the waterfall pool"}},
            {{ParkLandmarkKind::CinderRing, 618.0f, 470.0f, 150.0f, "the cinder ring"}},
            {{ParkLandmarkKind::Lookout, 930.0f, 352.0f, 50.0f, "the lookout"}},
        };
        return marks[std::clamp(static_cast<int>(env), 0, 3)];
    }

    bool PokeparkManager::canStand(const ParkResident &res, float x, float y) const
    {
        switch (terrainAt(currentHabitat, x, y))
        {
        case ParkTerrain::Land:
            return true;
        case ParkTerrain::Water:
            return (res.traversal & TraverseWater) != 0;
        case ParkTerrain::Warm:
            return (res.traversal & TraverseWarm) != 0;
        case ParkTerrain::Air:
            return (res.traversal & TraverseAir) != 0;
        default:
            return false;
        }
    }

    // A spot this resident would choose: one it can stand on, leaning toward the ground it likes
    // (water for swimmers at the lake, the warm stones for Fire types, the sky for flyers) and,
    // now and then, its own favourite spot.
    bool PokeparkManager::pickSpot(const ParkResident &res, float &outX, float &outY, int tries) const
    {
        if (res.favouriteX > 0 && frand() < 0.18f && canStand(res, res.favouriteX, res.favouriteY))
        {
            outX = res.favouriteX + frange(-30, 30);
            outY = res.favouriteY + frange(-15, 15);
            if (canStand(res, outX, outY))
                return true;
        }
        ParkTerrain preferred = ParkTerrain::Land;
        if (currentHabitat == ParkEnvironmentId::WaterfallLake && (res.traversal & TraverseWater) &&
            !res.hasType(Pokemon::TYPE_FLYING))
            preferred = ParkTerrain::Water;
        else if (currentHabitat == ParkEnvironmentId::EmberGrove && res.hasType(Pokemon::TYPE_FIRE))
            preferred = ParkTerrain::Warm;
        else if (currentHabitat == ParkEnvironmentId::SkySummit && (res.traversal & TraverseAir))
            preferred = ParkTerrain::Air;
        const bool edges = res.personality == ParkPersonality::Shy;
        const bool centre = res.personality == ParkPersonality::Bold || res.personality == ParkPersonality::Proud;

        float fallbackX = -1, fallbackY = -1;
        for (int attempt = 0; attempt < tries; ++attempt)
        {
            float x = frange(PLAY_MIN_X + 10, PLAY_MAX_X - 10), y = frange(PLAY_MIN_Y + 10, PLAY_MAX_Y - 6);
            if (edges && frand() < 0.7f)
                x = frand() < 0.5f ? frange(PLAY_MIN_X + 10, PLAY_MIN_X + 200) : frange(PLAY_MAX_X - 200, PLAY_MAX_X - 10);
            if (centre && frand() < 0.6f)
                x = frange(SCENE_W / 2 - 220, SCENE_W / 2 + 220);
            if (!canStand(res, x, y))
                continue;
            if (fallbackX < 0)
            {
                fallbackX = x;
                fallbackY = y;
            }
            if (terrainAt(currentHabitat, x, y) == preferred || frand() < 0.25f)
            {
                outX = x;
                outY = y;
                return true;
            }
        }
        if (fallbackX >= 0)
        {
            outX = fallbackX;
            outY = fallbackY;
            return true;
        }
        return false;
    }

    // ---- Clock & weather -----------------------------------------------------------------------

    void PokeparkManager::refreshClock()
    {
        const std::time_t now = std::time(nullptr);
        const std::tm *tm = std::localtime(&now);
        hour = tm ? tm->tm_hour : 12;
        const int day = tm ? tm->tm_yday : 0;
        // Weather per habitat per hour, the same for everyone that hour (a hash, not a live roll),
        // weighted by what the place gets.
        uint32_t h = static_cast<uint32_t>(day * 24 + hour) * 2654435761u ^
                     (static_cast<uint32_t>(currentHabitat) + 1) * 40503u;
        h ^= h >> 15;
        h *= 2246822519u;
        h ^= h >> 13;
        const int roll = static_cast<int>(h % 100);
        switch (currentHabitat)
        {
        case ParkEnvironmentId::Commons:
            weather = roll < 55 ? ParkWeather::Clear : roll < 75 ? ParkWeather::Wind : roll < 90 ? ParkWeather::Rain
                                                                                                 : ParkWeather::Fog;
            break;
        case ParkEnvironmentId::WaterfallLake:
            weather = roll < 50 ? ParkWeather::Clear : roll < 80 ? ParkWeather::Rain : ParkWeather::Fog;
            break;
        case ParkEnvironmentId::EmberGrove:
            weather = roll < 55 ? ParkWeather::Clear : roll < 90 ? ParkWeather::Heat : ParkWeather::Wind;
            break;
        case ParkEnvironmentId::SkySummit:
            weather = roll < 45 ? ParkWeather::Clear : roll < 75 ? ParkWeather::Snow : roll < 95 ? ParkWeather::Wind
                                                                                                 : ParkWeather::Fog;
            break;
        }
    }

    // ---- Pace ----------------------------------------------------------------------------------
    // Fast species dart and heavy ones plod: the walking pace comes from base Speed and weight.

    float PokeparkManager::paceFor(const ParkResident &res, ParkActivity activity) const
    {
        float walk = 28.0f + res.baseSpeed * 0.42f;
        if (res.weightKg > 200.0f)
            walk *= 0.75f;
        else if (res.weightKg > 80.0f)
            walk *= 0.88f;
        if (res.energy < 25.0f)
            walk *= 0.8f;
        switch (activity)
        {
        case ParkActivity::Run:
        case ParkActivity::Fetch:
            return walk * 2.3f;
        case ParkActivity::Play:
            return walk * 1.8f;
        case ParkActivity::Flee:
            return walk * 1.9f;
        case ParkActivity::Called:
            return walk * (res.bond >= 60 ? 1.8f : res.bond >= 25 ? 1.1f : 0.6f);
        case ParkActivity::Approach:
        case ParkActivity::Mediate:
            return walk * 1.15f;
        case ParkActivity::Swim:
            return walk * 0.8f;
        case ParkActivity::Follow:
            return walk * 1.05f;
        default:
            return walk;
        }
    }

    // Moves toward the target, easing up to pace and slowing on arrival; slides along anything it
    // cannot stand on, and gives up (ending the activity) when it is properly stuck.
    void PokeparkManager::steer(ParkResident &res, float dt, float pace)
    {
        const float dx = res.targetX - res.x, dy = res.targetY - res.y;
        const float d = std::hypot(dx, dy);
        const float want = d < 4.0f ? 0.0f : pace * std::min(1.0f, d / 50.0f);
        res.speed += (want - res.speed) * std::min(1.0f, dt * 5.0f);
        if (d < 4.0f || res.speed < 0.5f)
        {
            res.isMoving = false;
            res.vx = res.vy = 0;
            return;
        }
        res.vx = dx / d * res.speed;
        res.vy = dy / d * res.speed * 0.8f; // depth reads slower than across
        const float nx = res.x + res.vx * dt, ny = res.y + res.vy * dt;
        if (canStand(res, nx, ny))
        {
            res.x = nx;
            res.y = ny;
        }
        else if (canStand(res, nx, res.y))
            res.x = nx;
        else if (canStand(res, res.x, ny))
            res.y = ny;
        else
        {
            res.isMoving = false;
            res.activityTimer = 0.0f; // stuck: rethink
            return;
        }
        res.isMoving = true;
        if (std::fabs(res.vx) > 3.0f)
            res.facingLeft = res.vx < 0;
    }

    // ---- Choosing what to do -------------------------------------------------------------------

    void PokeparkManager::startActivity(int index, ParkActivity activity, float duration, int partner)
    {
        ParkResident &res = residents[index];
        res.activity = activity;
        res.activityTimer = duration;
        res.partner = partner;
        res.currentActivity = getActivityName(activity);
        if (activity != ParkActivity::Follow)
            res.leader = -1;
    }

    void PokeparkManager::chooseActivity(int index)
    {
        ParkResident &res = residents[index];
        const ParkPersonality p = res.personality;
        using P = ParkPersonality;
        const bool night = isNight();
        const bool sleepyTime = night != res.nocturnal;
        const bool rain = weather == ParkWeather::Rain, snow = weather == ParkWeather::Snow,
                   heat = weather == ParkWeather::Heat, wind = weather == ParkWeather::Wind;
        const bool fire = res.hasType(Pokemon::TYPE_FIRE), water = res.hasType(Pokemon::TYPE_WATER),
                   ice = res.hasType(Pokemon::TYPE_ICE), flyer = (res.traversal & TraverseAir) != 0;
        const float tired = 100.0f - res.energy;
        const float lively = res.energy / 100.0f;

        // Who it would rather spend time with: its best friend here, else someone new (Curious),
        // else whoever it gets along with.
        int friendIdx = -1, strangerIdx = -1, rivalIdx = -1;
        int bestFriend = -1000, bestRival = 0;
        const auto visible = getVisibleResidentIndices(currentHabitat);
        for (int other : visible)
        {
            if (other == index)
                continue;
            const ParkRelationship rel = getRelationship(res.id, residents[other].id);
            const int score = rel.friendship + compatibility(res, residents[other]);
            if (rel.sharedMoments < 3 && strangerIdx < 0)
                strangerIdx = other;
            if (score > bestFriend && residents[other].activity != ParkActivity::Nap)
            {
                bestFriend = score;
                friendIdx = other;
            }
            if (rel.rivalry > bestRival)
            {
                bestRival = rel.rivalry;
                rivalIdx = other;
            }
        }

        struct Option
        {
            ParkActivity activity;
            float score;
            int partner;
        };
        Option options[16];
        int count = 0;
        auto add = [&](ParkActivity a, float s, int partner = -1) {
            if (s > 0.5f && count < 16)
                options[count++] = {a, s, partner};
        };
        auto weight = [&](std::initializer_list<P> likes, float up, std::initializer_list<P> dislikes, float down) {
            for (P q : likes)
                if (q == p)
                    return up;
            for (P q : dislikes)
                if (q == p)
                    return down;
            return 1.0f;
        };

        add(ParkActivity::Nap, (tired * 1.2f - 30.0f) * (sleepyTime ? 2.2f : 0.8f) * weight({P::Calm, P::Gentle}, 1.2f, {P::Energetic}, 0.7f));
        add(ParkActivity::Sit, (tired * 0.5f + 10.0f) * weight({P::Calm, P::Gentle}, 1.8f, {P::Energetic, P::Playful}, 0.4f) *
                                   (heat && !fire ? 1.6f : 1.0f));
        add(ParkActivity::Idle, 12.0f);
        add(ParkActivity::Wander, 22.0f + res.curiosity * 0.2f);
        add(ParkActivity::Run, res.play * 0.6f * lively * weight({P::Energetic, P::Playful}, 2.2f, {P::Calm, P::Shy}, 0.4f) *
                                   (rain && fire ? 0.2f : 1.0f) * (snow && ice ? 1.6f : 1.0f) * (sleepyTime ? 0.3f : 1.0f));
        add(ParkActivity::Sniff, res.curiosity * 0.9f * weight({P::Curious}, 2.0f, {P::Proud}, 0.6f));

        if (friendIdx >= 0)
        {
            const float shyness = (p == P::Shy && bestFriend < 30) ? 0.35f : 1.0f;
            add(ParkActivity::Approach, res.social * 0.9f * shyness * weight({P::Helpful, P::Gentle, P::Playful}, 1.3f, {}, 1.0f), friendIdx);
            add(ParkActivity::Play, res.play * 0.8f * lively * weight({P::Playful, P::Energetic}, 1.8f, {P::Calm}, 0.6f) *
                                        (sleepyTime ? 0.3f : 1.0f), friendIdx);
            if (bestFriend >= 40)
                add(ParkActivity::Follow, res.social * 0.7f * weight({P::Shy, P::Gentle}, 1.5f, {P::Proud, P::Bold}, 0.5f), friendIdx);
        }
        if (strangerIdx >= 0)
            add(ParkActivity::Approach, res.curiosity * 0.7f * weight({P::Curious, P::Playful}, 1.8f, {P::Shy}, 0.15f), strangerIdx);
        if (strangerIdx >= 0 && p == P::Shy)
            add(ParkActivity::Watch, 14.0f, strangerIdx);
        if (rivalIdx >= 0 && (p == P::Bold || p == P::Proud) && bestRival > 20)
            add(ParkActivity::Approach, 18.0f + bestRival * 0.4f, rivalIdx);

        // Habitat- and weather-specific options.
        if (water && currentHabitat == ParkEnvironmentId::WaterfallLake)
            add(ParkActivity::Swim, 45.0f * (rain ? 1.6f : 1.0f));
        if (flyer && (currentHabitat == ParkEnvironmentId::SkySummit || wind))
            add(ParkActivity::Perch, 30.0f * weight({P::Proud, P::Bold}, 1.6f, {}, 1.0f) * (wind ? 1.5f : 1.0f));
        if (fire && currentHabitat == ParkEnvironmentId::EmberGrove)
            add(ParkActivity::Sit, 40.0f * (heat ? 1.6f : 1.0f)); // warming up at the cinder ring (see below)

        // Weighted pick, sharpened so the obvious choice usually wins without always winning.
        float total = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            options[i].score = std::pow(options[i].score, 1.4f);
            total += options[i].score;
        }
        float roll = frand() * total;
        Option chosen = {ParkActivity::Idle, 0.0f, -1};
        for (int i = 0; i < count; ++i)
        {
            roll -= options[i].score;
            if (roll <= 0.0f)
            {
                chosen = options[i];
                break;
            }
        }

        // Rain sends Fire types (and the soft-furred, when Calm) to shelter: under the tree, by the cliff.
        if (rain && fire)
        {
            startActivity(index, ParkActivity::Watch, frange(6, 10));
            const auto &marks = getLandmarks(currentHabitat);
            res.targetX = currentHabitat == ParkEnvironmentId::Commons ? marks[0].x + frange(-40, 40) : PLAY_MIN_X + 120.0f;
            res.targetY = currentHabitat == ParkEnvironmentId::Commons ? marks[0].y + frange(0, 30) : res.y;
            if (!canStand(res, res.targetX, res.targetY))
                pickSpot(res, res.targetX, res.targetY);
            setResidentEmotion(index, ParkEmotion::Sweat, 2.5f);
            res.currentActivity = "Sheltering from the rain";
            return;
        }

        switch (chosen.activity)
        {
        case ParkActivity::Nap:
        {
            startActivity(index, ParkActivity::Nap, sleepyTime ? frange(18, 40) : frange(8, 16));
            // Sleep near a friend if one is close: clusters at night.
            if (friendIdx >= 0 && bestFriend >= 30 && dist(res.x, res.y, residents[friendIdx].x, residents[friendIdx].y) < 240)
            {
                res.targetX = residents[friendIdx].x + (res.x < residents[friendIdx].x ? -46.0f : 46.0f);
                res.targetY = residents[friendIdx].y;
            }
            else
            {
                res.targetX = res.x;
                res.targetY = res.y;
            }
            setResidentEmotion(index, ParkEmotion::Sleep, 4.0f);
            break;
        }
        case ParkActivity::Sit:
        {
            startActivity(index, ParkActivity::Sit, frange(4, 10));
            if (fire && currentHabitat == ParkEnvironmentId::EmberGrove)
            {
                const auto &ring = getLandmarks(currentHabitat)[0];
                res.targetX = ring.x + frange(-ring.radius, ring.radius);
                res.targetY = ring.y + frange(-20, 30);
                res.currentActivity = "Warming up by the cinder ring";
            }
            else if (heat && !fire)
            {
                pickSpot(res, res.targetX, res.targetY); // looking for shade
                res.currentActivity = "Looking for shade";
            }
            else
            {
                res.targetX = res.x;
                res.targetY = res.y;
            }
            if (!canStand(res, res.targetX, res.targetY))
            {
                res.targetX = res.x;
                res.targetY = res.y;
            }
            break;
        }
        case ParkActivity::Wander:
            startActivity(index, ParkActivity::Wander, 12.0f);
            pickSpot(res, res.targetX, res.targetY);
            break;
        case ParkActivity::Run:
            startActivity(index, ParkActivity::Run, frange(4, 7));
            pickSpot(res, res.targetX, res.targetY);
            break;
        case ParkActivity::Sniff:
        {
            startActivity(index, ParkActivity::Sniff, frange(8, 12));
            const auto &marks = getLandmarks(currentHabitat);
            const ParkLandmark &mark = marks[0];
            res.targetX = mark.x + frange(-mark.radius, mark.radius) * 0.6f;
            res.targetY = mark.y + frange(-mark.radius, mark.radius) * 0.3f;
            if (!canStand(res, res.targetX, res.targetY))
                pickSpot(res, res.targetX, res.targetY);
            break;
        }
        case ParkActivity::Approach:
        case ParkActivity::Play:
        case ParkActivity::Follow:
        case ParkActivity::Watch:
        {
            const int other = chosen.partner;
            if (other < 0)
            {
                startActivity(index, ParkActivity::Idle, 2.0f);
                break;
            }
            if (chosen.activity == ParkActivity::Follow)
            {
                startActivity(index, ParkActivity::Follow, frange(8, 15), other);
                res.leader = other;
            }
            else if (chosen.activity == ParkActivity::Watch)
            {
                startActivity(index, ParkActivity::Watch, frange(3, 6), other);
                res.targetX = res.x;
                res.targetY = res.y;
                setResidentEmotion(index, ParkEmotion::Question, 2.0f);
            }
            else
                startActivity(index, ParkActivity::Approach, 10.0f, other);
            break;
        }
        case ParkActivity::Swim:
            startActivity(index, ParkActivity::Swim, frange(8, 14));
            pickSpot(res, res.targetX, res.targetY);
            break;
        case ParkActivity::Perch:
        {
            startActivity(index, ParkActivity::Perch, frange(6, 12));
            if (currentHabitat == ParkEnvironmentId::SkySummit && frand() < 0.6f)
            {
                const auto &look = getLandmarks(currentHabitat)[0];
                res.targetX = look.x + frange(-20, 20);
                res.targetY = look.y - frange(0, 40);
            }
            else
            {
                res.targetX = frange(PLAY_MIN_X + 40, PLAY_MAX_X - 40);
                res.targetY = frange(PLAY_MIN_Y + 6, PLAY_MIN_Y + 120);
            }
            if (!canStand(res, res.targetX, res.targetY))
                pickSpot(res, res.targetX, res.targetY);
            break;
        }
        default:
            startActivity(index, ParkActivity::Idle, frange(1.5f, 4.0f));
            res.targetX = res.x;
            res.targetY = res.y;
            break;
        }
    }

    // ---- One resident, one frame ---------------------------------------------------------------

    void PokeparkManager::stepResident(int index, float dt)
    {
        ParkResident &res = residents[index];
        res.animClock += dt;
        res.emotionAge += dt;
        if (res.emotionTimer > 0.0f && (res.emotionTimer -= dt) <= 0.0f)
            res.emotion = ParkEmotion::None;
        if (res.interactionCooldown > 0.0f)
            res.interactionCooldown -= dt;

        // Needs drift.
        const bool resting = res.activity == ParkActivity::Nap || res.activity == ParkActivity::Sit;
        float drain = res.activity == ParkActivity::Run || res.activity == ParkActivity::Play ? 0.6f
                      : resting                                                             ? 0.0f
                                                                                            : 0.12f;
        if (!resting && isNight() != res.nocturnal)
            drain += 0.08f;
        res.energy = std::clamp(res.energy - drain * dt + (res.activity == ParkActivity::Nap ? 0.9f * dt
                                                           : res.activity == ParkActivity::Sit ? 0.3f * dt
                                                                                               : 0.0f),
                                0.0f, 100.0f);
        const float socialRate = res.personality == ParkPersonality::Shy ? 0.06f
                                 : (res.personality == ParkPersonality::Helpful || res.personality == ParkPersonality::Gentle)
                                     ? 0.15f
                                     : 0.11f;
        res.social = std::min(100.0f, res.social + socialRate * dt);
        const bool playful = res.personality == ParkPersonality::Playful || res.personality == ParkPersonality::Energetic;
        res.play = std::min(100.0f, res.play + (playful ? 0.18f : 0.09f) * dt);
        res.curiosity = std::min(100.0f, res.curiosity + (res.personality == ParkPersonality::Curious ? 0.2f : 0.09f) * dt);
        if (res.activity == ParkActivity::Run || res.activity == ParkActivity::Play || res.activity == ParkActivity::Fetch)
            res.play = std::max(0.0f, res.play - 6.0f * dt);
        if (res.activity == ParkActivity::Sniff && !res.isMoving)
            res.curiosity = std::max(0.0f, res.curiosity - 9.0f * dt);
        if (res.activity == ParkActivity::Follow || res.activity == ParkActivity::Play)
            res.social = std::max(0.0f, res.social - 4.0f * dt);

        // Mood follows the needs (and the weather), rather than a coin flip.
        const bool likesWeather = (weather == ParkWeather::Rain && res.hasType(Pokemon::TYPE_WATER)) ||
                                  (weather == ParkWeather::Snow && res.hasType(Pokemon::TYPE_ICE)) ||
                                  (weather == ParkWeather::Heat && res.hasType(Pokemon::TYPE_FIRE)) ||
                                  (weather == ParkWeather::Wind && res.hasType(Pokemon::TYPE_FLYING));
        if (res.activity == ParkActivity::Sulk)
            res.mood = ParkMood::Grumpy;
        else if (res.energy < 25.0f)
            res.mood = ParkMood::Sleepy;
        else if (likesWeather)
            res.mood = ParkMood::Excited;
        else if (res.social > 80.0f)
            res.mood = ParkMood::Lonely;
        else if (res.play > 70.0f && res.energy > 50.0f)
            res.mood = ParkMood::Playful;
        else if (res.curiosity > 75.0f)
            res.mood = ParkMood::Curious;
        else
            res.mood = ParkMood::Content;

        // The activity itself.
        res.activityTimer -= dt;
        ParkResident *partner = (res.partner >= 0 && res.partner < static_cast<int>(residents.size())) ? &residents[res.partner] : nullptr;
        switch (res.activity)
        {
        case ParkActivity::Run:
            // Zoomies: a new lap target every time it arrives.
            if (dist(res.x, res.y, res.targetX, res.targetY) < 20.0f)
                pickSpot(res, res.targetX, res.targetY);
            res.hop = std::fabs(std::sin(res.animClock * 11.0f));
            break;
        case ParkActivity::Approach:
        case ParkActivity::Mediate:
            if (!partner)
            {
                res.activityTimer = 0.0f;
                break;
            }
            res.targetX = partner->x + (res.x < partner->x ? -52.0f : 52.0f);
            res.targetY = partner->y;
            if (dist(res.x, res.y, res.targetX, res.targetY) < 14.0f && partner->activity == ParkActivity::Nap &&
                res.activity == ParkActivity::Approach)
            {
                // It is asleep: sit beside it instead of waking it.
                startActivity(index, ParkActivity::Sit, frange(5, 9), res.partner);
                setResidentEmotion(index, ParkEmotion::Heart, 2.0f);
                updateRelationship(res.id, partner->id, 1, 2, 0);
            }
            else if (dist(res.x, res.y, res.targetX, res.targetY) < 14.0f)
            {
                if (res.activity == ParkActivity::Mediate)
                {
                    // Arrived between the two: make the peace.
                    // Only if the squabble is still on: the sulker may have moved on by now.
                    const int other = partner->activity == ParkActivity::Sulk ? partner->partner : -1;
                    if (other >= 0 && other < static_cast<int>(residents.size()) && other != index)
                    {
                        ParkRelationship &rel = relationships[ParkRelationship::makeKey(partner->id, residents[other].id)];
                        rel.firstId = partner->id;
                        rel.secondId = residents[other].id;
                        updateRelationship(partner->id, residents[other].id, 6, 2, -8);
                        const bool story = rel.lastConflictAt != 0 &&
                                           static_cast<uint64_t>(std::time(nullptr)) - rel.lastConflictAt > 20 * 3600;
                        addJournal(story ? ParkJournalEventKind::Story : ParkJournalEventKind::Reconciliation,
                                   story ? partner->name + " and " + residents[other].name +
                                               " finally made up, with a little help from " + res.name + "."
                                         : res.name + " calmed " + partner->name + " and " + residents[other].name +
                                               " down. They are friends again.",
                                   {res.partner, other});
                        rel.lastConflictAt = 0;
                        setResidentEmotion(res.partner, ParkEmotion::Heart, 3.0f);
                        setResidentEmotion(other, ParkEmotion::Heart, 3.0f);
                        startActivity(res.partner, ParkActivity::Celebrate, 1.5f);
                        startActivity(other, ParkActivity::Celebrate, 1.5f);
                    }
                    setResidentEmotion(index, ParkEmotion::Music, 3.0f);
                    res.bond = std::min(100, res.bond + 1);
                    startActivity(index, ParkActivity::Idle, 2.0f);
                }
                else
                    resolveMeeting(index, res.partner);
            }
            break;
        case ParkActivity::Follow:
            if (res.leader < 0 || res.leader >= static_cast<int>(residents.size()) ||
                residents[res.leader].activity == ParkActivity::Nap)
            {
                res.activityTimer = 0.0f;
                break;
            }
            {
                // Trail behind the leader, a little to one side, so a group walks as a group.
                const ParkResident &lead = residents[res.leader];
                const float side = static_cast<float>((index % 3) - 1) * 26.0f;
                res.targetX = lead.x + (lead.facingLeft ? 58.0f : -58.0f);
                res.targetY = lead.y + side;
                if (!canStand(res, res.targetX, res.targetY))
                {
                    res.targetX = lead.x;
                    res.targetY = lead.y + 30.0f;
                }
            }
            break;
        case ParkActivity::Play:
            // A chase: the pair take turns; the chaser runs at its partner, who runs off.
            if (!partner)
            {
                res.activityTimer = 0.0f;
                break;
            }
            if (static_cast<int>(res.animClock / 2.5f) % 2 == static_cast<int>(index < res.partner))
            {
                res.targetX = partner->x;
                res.targetY = partner->y;
            }
            else if (dist(res.x, res.y, res.targetX, res.targetY) < 20.0f)
                pickSpot(res, res.targetX, res.targetY);
            res.hop = std::fabs(std::sin(res.animClock * 9.0f));
            break;
        case ParkActivity::Spar:
            // Face each other and hop in place, in turn.
            if (partner)
                res.facingLeft = partner->x < res.x;
            res.hop = std::max(0.0f, std::sin(res.animClock * 8.0f + (index % 2) * 3.14159f));
            res.targetX = res.x;
            res.targetY = res.y;
            break;
        case ParkActivity::Celebrate:
            res.hop = std::fabs(std::sin(res.animClock * 10.0f));
            res.targetX = res.x;
            res.targetY = res.y;
            break;
        case ParkActivity::Flee:
            if (partner && dist(res.x, res.y, partner->x, partner->y) < 200.0f)
            {
                const float ax = res.x - partner->x, ay = res.y - partner->y, d = std::max(1.0f, std::hypot(ax, ay));
                res.targetX = std::clamp(res.x + ax / d * 120.0f, PLAY_MIN_X, PLAY_MAX_X);
                res.targetY = std::clamp(res.y + ay / d * 60.0f, PLAY_MIN_Y, PLAY_MAX_Y);
            }
            break;
        case ParkActivity::Watch:
            if (partner)
                res.facingLeft = partner->x < res.x;
            break;
        case ParkActivity::Called:
            // Coming over to you; a resident you are not close to stops partway, unsure.
            if (res.bond < 20 && dist(res.x, res.y, res.targetX, res.targetY) < 160.0f)
            {
                startActivity(index, ParkActivity::Watch, 3.0f);
                res.targetX = res.x;
                res.targetY = res.y;
                setResidentEmotion(index, ParkEmotion::Question, 2.5f);
                break;
            }
            if (dist(res.x, res.y, res.targetX, res.targetY) < 10.0f)
            {
                res.bond = std::min(100, res.bond + 1);
                setResidentEmotion(index, res.bond >= 50 ? ParkEmotion::Heart : ParkEmotion::Music, 3.0f);
                startActivity(index, res.bond >= 50 ? ParkActivity::Celebrate : ParkActivity::Sit, res.bond >= 50 ? 1.5f : 4.0f);
                res.facingLeft = false;
            }
            break;
        case ParkActivity::Fetch:
        {
            if (res.toy < 0 || res.toy >= static_cast<int>(toys.size()) || toys[res.toy].gone)
            {
                res.activityTimer = 0.0f;
                break;
            }
            ParkToy &toy = toys[res.toy];
            if (toy.holder >= 0 && toy.holder != index)
            {
                // Someone got there first.
                setResidentEmotion(index, ParkEmotion::Sweat, 1.5f);
                startActivity(index, ParkActivity::Watch, 2.0f, toy.holder);
                res.toy = -1;
                break;
            }
            res.targetX = toy.x;
            res.targetY = toy.y;
            if (toy.flight >= 1.0f && dist(res.x, res.y, toy.x, toy.y) < 18.0f)
            {
                toy.holder = index;
                if (toy.kind == ParkToyKind::Berry)
                {
                    startActivity(index, ParkActivity::Eat, 3.0f);
                    res.toy = static_cast<int>(&toy - &toys[0]);
                    // Share with a friend nearby.
                    for (int other : getVisibleResidentIndices(currentHabitat))
                    {
                        if (other == index || absorbed(residents[other].activity))
                            continue;
                        if (dist(res.x, res.y, residents[other].x, residents[other].y) < 140.0f &&
                            getRelationship(res.id, residents[other].id).friendship >= 30)
                        {
                            startActivity(other, ParkActivity::Eat, 3.0f, index);
                            residents[other].facingLeft = res.x < residents[other].x;
                            setResidentEmotion(index, ParkEmotion::Heart, 3.0f);
                            setResidentEmotion(other, ParkEmotion::Heart, 3.0f);
                            updateRelationship(res.id, residents[other].id, 4, 4, 0);
                            if (frand() < 0.4f)
                                addJournal(ParkJournalEventKind::Affection,
                                           res.name + " shared a berry with " + residents[other].name + ".",
                                           {index, other});
                            break;
                        }
                    }
                }
                else
                {
                    startActivity(index, ParkActivity::Play, 5.0f);
                    res.toy = static_cast<int>(&toy - &toys[0]);
                    setResidentEmotion(index, ParkEmotion::Music, 2.5f);
                }
            }
            break;
        }
        case ParkActivity::Eat:
            res.targetX = res.x;
            res.targetY = res.y;
            if (res.activityTimer <= 0.0f && res.toy >= 0 && res.toy < static_cast<int>(toys.size()))
            {
                ParkToy &toy = toys[res.toy];
                if (toy.holder == index)
                {
                    const int raised = (res.nature % 25) / 5, lowered = (res.nature % 25) % 5;
                    static constexpr ParkFlavor flavorOf[5] = {ParkFlavor::Spicy, ParkFlavor::Sour, ParkFlavor::Sweet,
                                                               ParkFlavor::Dry, ParkFlavor::Bitter};
                    const bool loved = raised != lowered && flavorOf[raised] == toy.flavor;
                    res.energy = std::min(100.0f, res.energy + (loved ? 14.0f : 7.0f));
                    setResidentEmotion(index, loved ? ParkEmotion::Heart : ParkEmotion::Music, 2.5f);
                    toy.gone = true;
                }
                res.toy = -1;
            }
            break;
        case ParkActivity::Swim:
            if (dist(res.x, res.y, res.targetX, res.targetY) < 16.0f)
                pickSpot(res, res.targetX, res.targetY);
            break;
        default:
            break;
        }

        // Ball play: batting the ball about in front of it.
        if (res.activity == ParkActivity::Play && res.toy >= 0 && res.toy < static_cast<int>(toys.size()))
        {
            ParkToy &ball = toys[res.toy];
            if (ball.holder == index && ball.kind == ParkToyKind::Ball && !ball.gone)
            {
                if (dist(res.x, res.y, ball.x, ball.y) < 22.0f)
                {
                    float nx = ball.x + (res.facingLeft ? -1.0f : 1.0f) * frange(50, 110), ny = ball.y + frange(-30, 30);
                    nx = std::clamp(nx, PLAY_MIN_X + 10, PLAY_MAX_X - 10);
                    ny = std::clamp(ny, PLAY_MIN_Y + 10, PLAY_MAX_Y - 10);
                    if (canStand(res, nx, ny))
                    {
                        ball.fromX = ball.x;
                        ball.fromY = ball.y;
                        ball.x = nx;
                        ball.y = ny;
                        ball.flight = 0.55f; // a short bounce
                    }
                }
                res.targetX = ball.x;
                res.targetY = ball.y;
                if (res.activityTimer <= 0.0f)
                {
                    ball.holder = -1;
                    res.toy = -1;
                }
            }
        }

        if (!moves(res.activity) && res.activity != ParkActivity::Sit)
        {
            res.targetX = res.x;
            res.targetY = res.y;
        }
        if (res.activity != ParkActivity::Run && res.activity != ParkActivity::Play && res.activity != ParkActivity::Spar &&
            res.activity != ParkActivity::Celebrate)
            res.hop = res.small && res.isMoving ? std::fabs(std::sin(res.animClock * 12.0f)) * 0.6f : 0.0f;

        steer(res, dt, paceFor(res, res.activity));

        // Footfall effects: dust when running on land, splashes in water, embers on the warm stones,
        // snow puffs on the summit.
        if (res.isMoving && res.speed > 30.0f)
        {
            res.stepDust += dt * (res.speed / 40.0f);
            if (res.stepDust > 0.35f)
            {
                res.stepDust = 0.0f;
                switch (terrainAt(currentHabitat, res.x, res.y))
                {
                case ParkTerrain::Water:
                    emit(ParkParticleKind::Splash, res.x, res.y, 3);
                    break;
                case ParkTerrain::Warm:
                    emit(ParkParticleKind::Ember, res.x, res.y, 2);
                    break;
                case ParkTerrain::Land:
                    emit(currentHabitat == ParkEnvironmentId::SkySummit ? ParkParticleKind::Snow : ParkParticleKind::Dust,
                         res.x, res.y, res.speed > 70.0f ? 3 : 1);
                    break;
                default:
                    break;
                }
            }
        }

        // Its favourite spot drifts toward wherever it chooses to rest.
        if (res.activity == ParkActivity::Sit || res.activity == ParkActivity::Nap)
        {
            if (res.favouriteX < 0)
            {
                res.favouriteX = res.x;
                res.favouriteY = res.y;
            }
            res.favouriteX += (res.x - res.favouriteX) * 0.002f;
            res.favouriteY += (res.y - res.favouriteY) * 0.002f;
        }

        if (res.activityTimer <= 0.0f)
        {
            // How some activities end.
            if (res.activity == ParkActivity::Spar)
            {
                // One of the pair sulks off; a peacemaker may come over.
                if (partner && (index % 2 == 0))
                {
                    startActivity(index, ParkActivity::Sulk, frange(4, 6), res.partner);
                    setResidentEmotion(index, ParkEmotion::Sweat, 3.0f);
                    const int mediator = findMediator(index, res.partner);
                    if (mediator >= 0 && !absorbed(residents[mediator].activity))
                        startActivity(mediator, ParkActivity::Mediate, 14.0f, index);
                    return;
                }
            }
            if (res.activity == ParkActivity::Sniff)
                res.curiosity = std::max(0.0f, res.curiosity - 20.0f);
            chooseActivity(index);
        }
    }

    int PokeparkManager::findMediator(int a, int b) const
    {
        int best = -1, bestScore = 0;
        for (int i = 0; i < static_cast<int>(residents.size()); ++i)
        {
            if (i == a || i == b || !residents[i].enjoysHabitat(currentHabitat))
                continue;
            int score = 12;
            switch (residents[i].personality)
            {
            case ParkPersonality::Helpful: score = 35; break;
            case ParkPersonality::Gentle: score = 30; break;
            case ParkPersonality::Calm: score = 25; break;
            case ParkPersonality::Shy: score = 8; break;
            case ParkPersonality::Proud: score = 0; break;
            default: break;
            }
            if (score > bestScore)
            {
                bestScore = score;
                best = i;
            }
        }
        return bestScore >= 20 ? best : -1;
    }

    // ---- Meetings ------------------------------------------------------------------------------

    void PokeparkManager::noteBonds(int a, int b)
    {
        ParkRelationship &rel = relationships[ParkRelationship::makeKey(residents[a].id, residents[b].id)];
        if (rel.friendship >= 60 && rel.sharedMoments >= 12 && !(rel.announced & 1))
        {
            rel.announced |= 1;
            addJournal(ParkJournalEventKind::Bond, residents[a].name + " and " + residents[b].name +
                                                       " have become best friends!", {a, b});
        }
        if (rel.rivalry >= 50 && !(rel.announced & 2))
        {
            rel.announced |= 2;
            addJournal(ParkJournalEventKind::Bond, residents[a].name + " and " + residents[b].name +
                                                       " are rivals now. Neither will admit the other is good.", {a, b});
        }
    }

    void PokeparkManager::resolveMeeting(int a, int b)
    {
        if (a < 0 || b < 0 || a == b)
            return;
        ParkResident &ra = residents[a];
        ParkResident &rb = residents[b];
        // Long enough that a pair does not bump into each other every few seconds: friendships
        // should take a few visits, not a few minutes.
        ra.interactionCooldown = rb.interactionCooldown = frange(18.0f, 26.0f);
        ra.facingLeft = rb.x < ra.x;
        rb.facingLeft = ra.x < rb.x;
        const ParkRelationship rel = getRelationship(ra.id, rb.id);
        const int comp = compatibility(ra, rb);
        const bool strangers = rel.sharedMoments < 3;
        ra.social = std::max(0.0f, ra.social - 35.0f);
        rb.social = std::max(0.0f, rb.social - 35.0f);

        // A shy resident meeting a stranger who barrels in, or a bold one, backs off.
        auto shyScare = [&](ParkResident &shy, int shyIdx, const ParkResident &other, int otherIdx) {
            if (shy.personality != ParkPersonality::Shy || !strangers)
                return false;
            if (other.activity != ParkActivity::Run && other.personality != ParkPersonality::Bold &&
                other.personality != ParkPersonality::Energetic && shy.bond >= 40)
                return false;
            startActivity(shyIdx, ParkActivity::Flee, 2.5f, otherIdx);
            setResidentEmotion(shyIdx, ParkEmotion::Sweat, 2.5f);
            setResidentEmotion(otherIdx, ParkEmotion::Question, 2.0f);
            startActivity(otherIdx, ParkActivity::Idle, 2.0f);
            updateRelationship(ra.id, rb.id, 1, 0, 0);
            return true;
        };
        if (shyScare(ra, a, rb, b) || shyScare(rb, b, ra, a))
            return;

        int conflictChance = std::clamp(8 + rel.rivalry / 3 - comp / 3 - rel.friendship / 10, 2, 45);
        if (ra.mood == ParkMood::Grumpy || rb.mood == ParkMood::Grumpy)
            conflictChance += 15;
        if (rand() % 100 < conflictChance)
        {
            // Posture, then a play-spar.
            startActivity(a, ParkActivity::Spar, frange(3, 4), b);
            startActivity(b, ParkActivity::Spar, frange(3, 4), a);
            setResidentEmotion(a, ParkEmotion::Angry, 3.0f);
            setResidentEmotion(b, ParkEmotion::Angry, 3.0f);
            updateRelationship(ra.id, rb.id, -3, 0, 6);
            relationships[ParkRelationship::makeKey(ra.id, rb.id)].lastConflictAt = static_cast<uint64_t>(std::time(nullptr));
            if (rel.rivalry + 6 >= 30 && frand() < 0.35f)
                addJournal(ParkJournalEventKind::Conflict, ra.name + " and " + rb.name + " squared off near " +
                                                                getLandmarks(currentHabitat)[0].name + " again.", {a, b});
        }
        else if (rel.friendship >= 40)
        {
            // Friends: hearts, then off together -- a game, a walk, or a rest side by side.
            setResidentEmotion(a, ParkEmotion::Heart, 3.0f);
            setResidentEmotion(b, ParkEmotion::Heart, 3.0f);
            updateRelationship(ra.id, rb.id, 2, 3, 0);
            if ((ra.play > 40.0f || rb.play > 40.0f) && ra.energy > 35.0f && rb.energy > 35.0f)
            {
                startActivity(a, ParkActivity::Play, frange(5, 8), b);
                startActivity(b, ParkActivity::Play, frange(5, 8), a);
            }
            else if (ra.energy < 40.0f || rb.energy < 40.0f)
            {
                startActivity(a, ParkActivity::Sit, frange(6, 10), b);
                startActivity(b, ParkActivity::Sit, frange(6, 10), a);
            }
            else
            {
                // The livelier one leads and the other tags along: a little group forms.
                const int leader = ra.energy >= rb.energy ? a : b, follower = leader == a ? b : a;
                startActivity(leader, ParkActivity::Wander, 12.0f);
                pickSpot(residents[leader], residents[leader].targetX, residents[leader].targetY);
                startActivity(follower, ParkActivity::Follow, frange(10, 15), leader);
                residents[follower].leader = leader;
                // Others close to the leader may join in.
                for (int other : getVisibleResidentIndices(currentHabitat))
                {
                    if (other == a || other == b || absorbed(residents[other].activity))
                        continue;
                    if (getRelationship(residents[other].id, residents[leader].id).friendship >= 40 &&
                        dist(residents[other].x, residents[other].y, residents[leader].x, residents[leader].y) < 260.0f)
                    {
                        startActivity(other, ParkActivity::Follow, frange(8, 12), leader);
                        residents[other].leader = leader;
                    }
                }
            }
        }
        else if (strangers)
        {
            // Strangers size each other up.
            setResidentEmotion(a, ParkEmotion::Question, 2.5f);
            setResidentEmotion(b, ParkEmotion::Exclamation, 2.5f);
            startActivity(a, ParkActivity::Sniff, 2.5f, b);
            startActivity(b, ParkActivity::Watch, 2.5f, a);
            ra.targetX = ra.x;
            ra.targetY = ra.y;
            updateRelationship(ra.id, rb.id, 3 + std::max(0, comp / 10), 1, 0);
            if (rel.sharedMoments == 0 && frand() < 0.15f)
                addJournal(ParkJournalEventKind::Friendship, ra.name + " met " + rb.name + " for the first time.", {a, b});
        }
        else
        {
            setResidentEmotion(a, ParkEmotion::Music, 3.0f);
            setResidentEmotion(b, ParkEmotion::Music, 3.0f);
            updateRelationship(ra.id, rb.id, 4 + std::max(0, comp / 15), 2, 0);
            if (ra.play > 30.0f && ra.energy > 30.0f)
            {
                startActivity(a, ParkActivity::Play, frange(4, 6), b);
                startActivity(b, ParkActivity::Play, frange(4, 6), a);
            }
            else
            {
                startActivity(a, ParkActivity::Idle, 2.5f, b);
                startActivity(b, ParkActivity::Idle, 2.5f, a);
            }
        }
        noteBonds(a, b);
    }

    void PokeparkManager::checkMeetings()
    {
        const auto visible = getVisibleResidentIndices(currentHabitat);
        for (size_t i = 0; i < visible.size(); ++i)
        {
            ParkResident &ra = residents[visible[i]];
            if (ra.interactionCooldown > 0.0f || absorbed(ra.activity) || ra.activity == ParkActivity::Follow)
                continue;
            for (size_t j = i + 1; j < visible.size(); ++j)
            {
                ParkResident &rb = residents[visible[j]];
                if (rb.interactionCooldown > 0.0f || absorbed(rb.activity) || rb.activity == ParkActivity::Follow)
                    continue;
                if (dist(ra.x, ra.y, rb.x, rb.y) < 64.0f && frand() < 0.6f)
                {
                    resolveMeeting(visible[i], visible[j]);
                    return; // one meeting per frame keeps the park legible
                }
            }
        }
    }

    // ---- Toys ----------------------------------------------------------------------------------

    void PokeparkManager::throwToy(ParkToyKind kind, float x, float y, ParkFlavor flavor)
    {
        ParkToy toy;
        toy.kind = kind;
        toy.flavor = flavor;
        toy.fromX = PLAYER_X;
        toy.fromY = PLAYER_Y + 40.0f;
        toy.x = std::clamp(x, PLAY_MIN_X + 10, PLAY_MAX_X - 10);
        toy.y = std::clamp(y, PLAY_MIN_Y + 10, PLAY_MAX_Y - 10);
        toy.flight = 0.0f;
        placeToy(toy);
    }

    // Toys live in fixed slots (residents hold a slot index): a finished toy's slot is reused, and
    // when all six are in play the oldest is taken back from whoever had it.
    void PokeparkManager::placeToy(const ParkToy &toy)
    {
        int slot = -1;
        for (size_t i = 0; i < toys.size(); ++i)
            if (toys[i].gone)
            {
                slot = static_cast<int>(i);
                break;
            }
        if (slot < 0 && toys.size() < 6)
        {
            toys.push_back(toy);
            return;
        }
        if (slot < 0)
        {
            slot = 0;
            for (size_t i = 1; i < toys.size(); ++i)
                if (toys[i].life > toys[slot].life)
                    slot = static_cast<int>(i);
        }
        for (auto &r : residents)
            if (r.toy == slot)
                r.toy = -1;
        toys[slot] = toy;
    }

    void PokeparkManager::updateToys(float dt)
    {
        for (size_t t = 0; t < toys.size(); ++t)
        {
            ParkToy &toy = toys[t];
            if (toy.gone)
                continue;
            if (toy.flight < 1.0f)
            {
                toy.flight = std::min(1.0f, toy.flight + dt / 0.6f);
                if (toy.flight >= 1.0f)
                {
                    emit(terrainAt(currentHabitat, toy.x, toy.y) == ParkTerrain::Water ? ParkParticleKind::Splash
                                                                                         : ParkParticleKind::Dust,
                         toy.x, toy.y, 4);
                    // Everyone nearby decides what to make of it, by personality.
                    for (int i : getVisibleResidentIndices(currentHabitat))
                    {
                        ParkResident &res = residents[i];
                        if (absorbed(res.activity) || dist(res.x, res.y, toy.x, toy.y) > 560.0f)
                            continue;
                        const bool berry = toy.kind == ParkToyKind::Berry;
                        float eagerness = berry ? (100.0f - res.energy) * 0.6f + 20.0f : res.play * 0.8f;
                        switch (res.personality)
                        {
                        case ParkPersonality::Playful:
                        case ParkPersonality::Energetic: eagerness += 45.0f; break;
                        case ParkPersonality::Curious: eagerness += 25.0f; break;
                        case ParkPersonality::Calm: eagerness -= 15.0f; break;
                        case ParkPersonality::Proud: eagerness -= 30.0f; break; // pretends not to care
                        case ParkPersonality::Shy: eagerness -= 10.0f; break;
                        default: break;
                        }
                        if (eagerness < 25.0f + frand() * 30.0f)
                            continue;
                        if (res.personality == ParkPersonality::Shy)
                        {
                            // Waits until nobody else is near it.
                            startActivity(i, ParkActivity::Watch, frange(2, 4));
                            res.toy = static_cast<int>(t);
                            setResidentEmotion(i, ParkEmotion::Question, 2.0f);
                            continue;
                        }
                        startActivity(i, ParkActivity::Fetch, 10.0f);
                        res.toy = static_cast<int>(t);
                        setResidentEmotion(i, ParkEmotion::Exclamation, 1.5f);
                    }
                }
                continue;
            }
            toy.life += dt;
            if (toy.life > 25.0f && toy.holder < 0)
                toy.gone = true;
        }
        // A shy resident watching a toy goes for it once nobody else is close.
        for (int i : getVisibleResidentIndices(currentHabitat))
        {
            ParkResident &res = residents[i];
            if (res.activity != ParkActivity::Watch || res.toy < 0 || res.toy >= static_cast<int>(toys.size()))
                continue;
            const ParkToy &toy = toys[res.toy];
            if (toy.gone || toy.holder >= 0)
            {
                res.toy = -1;
                continue;
            }
            bool crowded = false;
            for (int j : getVisibleResidentIndices(currentHabitat))
                if (j != i && dist(residents[j].x, residents[j].y, toy.x, toy.y) < 120.0f)
                    crowded = true;
            if (!crowded && res.activityTimer < 0.5f)
                startActivity(i, ParkActivity::Fetch, 10.0f);
            else if (crowded)
                res.activityTimer = std::max(res.activityTimer, 1.0f);
        }
        // Proud residents pretend not to care, then sneak over once nobody is going for it.
        for (int i : getVisibleResidentIndices(currentHabitat))
        {
            ParkResident &res = residents[i];
            if (res.personality != ParkPersonality::Proud || absorbed(res.activity))
                continue;
            for (size_t t = 0; t < toys.size(); ++t)
            {
                const ParkToy &toy = toys[t];
                if (toy.gone || toy.holder >= 0 || toy.flight < 1.0f || toy.life < 4.0f)
                    continue;
                const bool sought = std::any_of(residents.begin(), residents.end(), [&](const ParkResident &r) {
                    return r.toy == static_cast<int>(t) && r.activity == ParkActivity::Fetch;
                });
                if (!sought && frand() < dt * 0.6f)
                {
                    startActivity(i, ParkActivity::Fetch, 10.0f);
                    res.toy = static_cast<int>(t);
                    break;
                }
            }
        }
    }

    // ---- Wild visitors -------------------------------------------------------------------------

    void PokeparkManager::updateVisitor(float dt)
    {
        const int env = static_cast<int>(currentHabitat);
        if (!visitor.active)
        {
            visitorTimer -= dt;
            if (visitorTimer > 0.0f)
                return;
            visitorTimer = frange(70, 150);
            const int pick = rand() % 6;
            visitor = ParkVisitor{};
            visitor.active = true;
            visitor.species = VISITORS[env][pick];
            visitor.name = VISITOR_NAMES[env][pick];
            visitor.shiny = rand() % 64 == 0;
            const bool fromLeft = frand() < 0.5f;
            visitor.x = fromLeft ? PLAY_MIN_X - 40.0f : PLAY_MAX_X + 40.0f;
            visitor.y = frange(PLAY_MAX_Y - 120, PLAY_MAX_Y - 20);
            if (currentHabitat == ParkEnvironmentId::WaterfallLake)
                visitor.y = frange(300, 390);
            visitor.targetX = fromLeft ? frange(260, 520) : frange(700, 960);
            visitor.targetY = visitor.y;
            visitor.timer = frange(20, 30);
            visitor.facingLeft = !fromLeft;

            // The residents react, each in its own way.
            std::string greeter, hider;
            for (int i : getVisibleResidentIndices(currentHabitat))
            {
                ParkResident &res = residents[i];
                if (absorbed(res.activity))
                    continue;
                switch (res.personality)
                {
                case ParkPersonality::Curious:
                case ParkPersonality::Playful:
                    startActivity(i, ParkActivity::Sniff, frange(6, 9));
                    res.targetX = visitor.targetX + (res.x < visitor.targetX ? -60.0f : 60.0f);
                    res.targetY = visitor.targetY;
                    setResidentEmotion(i, ParkEmotion::Exclamation, 2.0f);
                    if (greeter.empty())
                        greeter = res.name;
                    break;
                case ParkPersonality::Shy:
                    startActivity(i, ParkActivity::Watch, frange(5, 8));
                    pickSpot(res, res.targetX, res.targetY);
                    setResidentEmotion(i, ParkEmotion::Sweat, 2.5f);
                    if (hider.empty())
                        hider = res.name;
                    break;
                case ParkPersonality::Bold:
                case ParkPersonality::Proud:
                    setResidentEmotion(i, ParkEmotion::Angry, 2.0f);
                    res.facingLeft = visitor.targetX < res.x;
                    break;
                case ParkPersonality::Helpful:
                case ParkPersonality::Gentle:
                    startActivity(i, ParkActivity::Sniff, frange(5, 8));
                    res.targetX = visitor.targetX + (res.x < visitor.targetX ? -70.0f : 70.0f);
                    res.targetY = visitor.targetY;
                    setResidentEmotion(i, ParkEmotion::Heart, 2.5f);
                    if (greeter.empty())
                        greeter = res.name;
                    break;
                default:
                    setResidentEmotion(i, ParkEmotion::Question, 2.0f);
                    break;
                }
                if (!canStand(res, res.targetX, res.targetY))
                    pickSpot(res, res.targetX, res.targetY);
            }
            std::string text = std::string("A wild ") + (visitor.shiny ? "shiny " : "") + visitor.name +
                               " wandered into " + getEnvironmentInfo(currentHabitat).name + ".";
            if (!greeter.empty())
                text += " " + greeter + " went to say hello";
            if (!hider.empty())
                text += std::string(greeter.empty() ? " " : ", while ") + hider + " watched from a safe distance";
            if (!greeter.empty() || !hider.empty())
                text += ".";
            addJournal(ParkJournalEventKind::Visitor, text, {});
            return;
        }

        visitor.animClock += dt;
        const float dx = visitor.targetX - visitor.x, dy = visitor.targetY - visitor.y, d = std::hypot(dx, dy);
        if (d > 4.0f)
        {
            visitor.x += dx / d * 55.0f * dt;
            visitor.y += dy / d * 55.0f * dt;
            visitor.facingLeft = dx < 0;
        }
        else if (!visitor.leaving)
        {
            visitor.timer -= dt;
            if (visitor.timer <= 0.0f)
            {
                visitor.leaving = true;
                visitor.targetX = visitor.x < SCENE_W / 2 ? PLAY_MIN_X - 60.0f : PLAY_MAX_X + 60.0f;
            }
            else if (frand() < dt * 0.25f)
            {
                visitor.targetX = std::clamp(visitor.x + frange(-120, 120), PLAY_MIN_X + 40, PLAY_MAX_X - 40);
            }
        }
        else
            visitor.active = false;
    }

    // ---- Particles -----------------------------------------------------------------------------

    void PokeparkManager::emit(ParkParticleKind kind, float x, float y, int count)
    {
        for (int i = 0; i < count && particles.size() < 240; ++i)
        {
            ParkParticle p{kind, x + frange(-8, 8), y + frange(-3, 3), 0, 0, 0, 1.0f, 3.0f};
            switch (kind)
            {
            case ParkParticleKind::Dust:
                p.vx = frange(-30, 30);
                p.vy = frange(-25, -5);
                p.maxLife = frange(0.4f, 0.7f);
                p.size = frange(3, 6);
                break;
            case ParkParticleKind::Splash:
                p.vx = frange(-50, 50);
                p.vy = frange(-90, -40);
                p.maxLife = frange(0.4f, 0.6f);
                p.size = frange(2, 4);
                break;
            case ParkParticleKind::Ember:
                p.vx = frange(-12, 12);
                p.vy = frange(-60, -30);
                p.maxLife = frange(0.8f, 1.4f);
                p.size = frange(2, 3.5f);
                break;
            case ParkParticleKind::Snow:
                p.vx = frange(-25, 25);
                p.vy = frange(-30, -10);
                p.maxLife = frange(0.4f, 0.8f);
                p.size = frange(2, 4);
                break;
            case ParkParticleKind::Sparkle:
            case ParkParticleKind::Heart:
                p.x = x + frange(-24, 24);
                p.y = y + frange(-50, -10);
                p.vy = frange(-30, -10);
                p.maxLife = frange(0.8f, 1.3f);
                p.size = frange(3, 6);
                break;
            default:
                p.maxLife = 1.0f;
                break;
            }
            particles.push_back(p);
        }
    }

    void PokeparkManager::spawnAmbient(float dt)
    {
        auto chance = [&](float perSecond) { return frand() < perSecond * dt; };
        auto ambient = [&](ParkParticleKind kind, float x, float y, float vx, float vy, float life, float size) {
            if (particles.size() < 240)
                particles.push_back({kind, x, y, vx, vy, 0, life, size});
        };
        // Weather.
        if (weather == ParkWeather::Rain)
            for (int i = 0; i < 3; ++i)
                if (chance(30.0f))
                    ambient(ParkParticleKind::Rain, frange(0, SCENE_W), frange(40, 120), -60.0f, frange(520, 640), 1.0f, 2.0f);
        if (weather == ParkWeather::Snow && chance(14.0f))
            ambient(ParkParticleKind::Snow, frange(0, SCENE_W), 50.0f, frange(-20, 20), frange(40, 70), 9.0f, frange(2, 4));
        if (weather == ParkWeather::Wind && chance(3.0f))
            ambient(ParkParticleKind::Leaf, -10.0f, frange(140, 520), frange(140, 220), frange(-20, 20), 8.0f, frange(4, 7));
        if (weather == ParkWeather::Heat && chance(2.5f))
            ambient(ParkParticleKind::Ember, frange(PLAY_MIN_X, PLAY_MAX_X), PLAY_MAX_Y, frange(-8, 8), frange(-40, -20), 3.0f, 2.5f);
        if (weather == ParkWeather::Fog && chance(0.8f))
            ambient(ParkParticleKind::Mist, -80.0f, frange(220, 500), frange(14, 24), 0.0f, 60.0f, frange(60, 110));
        // Places.
        if (isNight() && (currentHabitat == ParkEnvironmentId::Commons || currentHabitat == ParkEnvironmentId::WaterfallLake) &&
            chance(0.9f))
            ambient(ParkParticleKind::Firefly, frange(PLAY_MIN_X, PLAY_MAX_X), frange(PLAY_MIN_Y, PLAY_MAX_Y), frange(-10, 10),
                    frange(-8, 8), frange(4, 7), 3.0f);
        if (currentHabitat == ParkEnvironmentId::WaterfallLake && chance(4.0f))
            ambient(ParkParticleKind::Mist, frange(50, 110), frange(470, 510), frange(-6, 18), frange(-24, -8), 2.5f, frange(10, 22));
        if (currentHabitat == ParkEnvironmentId::EmberGrove && chance(3.0f))
        {
            const bool left = frand() < 0.5f;
            ambient(ParkParticleKind::Ember, left ? 488.0f + frange(-20, 20) : 748.0f + frange(-24, 24), left ? 480.0f : 495.0f,
                    frange(-6, 6), frange(-50, -25), frange(1.2f, 2.2f), frange(2, 3.5f));
        }
    }

    void PokeparkManager::updateParticles(float dt)
    {
        for (auto &p : particles)
        {
            p.life += dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            switch (p.kind)
            {
            case ParkParticleKind::Splash:
            case ParkParticleKind::Dust:
                p.vy += 160.0f * dt; // falls back
                p.vx *= 1.0f - 2.0f * dt;
                break;
            case ParkParticleKind::Firefly:
                p.vx += frange(-30, 30) * dt;
                p.vy += frange(-30, 30) * dt;
                break;
            case ParkParticleKind::Leaf:
                p.vy = std::sin(p.life * 3.0f + p.x * 0.01f) * 40.0f;
                break;
            default:
                break;
            }
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
                                       [](const ParkParticle &p) { return p.life >= p.maxLife; }),
                        particles.end());
    }

    // ---- The frame -----------------------------------------------------------------------------

    void PokeparkManager::update(float dt)
    {
        dt = std::clamp(dt, 0.0f, 0.05f);
        realTimeUpdateClock += dt;
        worldClock += dt;
        weatherCheck -= dt;
        if (weatherCheck <= 0.0f)
        {
            const ParkWeather before = weather;
            refreshClock();
            weatherCheck = 30.0f;
            if (before != weather && worldClock > 1.0f && weather != ParkWeather::Clear)
            {
                // Say who it suits.
                std::string pleased;
                for (int i : getVisibleResidentIndices(currentHabitat))
                {
                    const ParkResident &r = residents[i];
                    if ((weather == ParkWeather::Rain && r.hasType(Pokemon::TYPE_WATER)) ||
                        (weather == ParkWeather::Snow && r.hasType(Pokemon::TYPE_ICE)) ||
                        (weather == ParkWeather::Heat && r.hasType(Pokemon::TYPE_FIRE)) ||
                        (weather == ParkWeather::Wind && r.hasType(Pokemon::TYPE_FLYING)))
                    {
                        pleased = r.name;
                        setResidentEmotion(i, ParkEmotion::Music, 3.0f);
                        break;
                    }
                }
                std::string text = std::string(getWeatherName(weather)) + " rolled over " +
                                   getEnvironmentInfo(currentHabitat).name + ".";
                if (!pleased.empty())
                    text += " " + pleased + " could not have been happier about it.";
                addJournal(ParkJournalEventKind::Weather, text, {});
            }
        }

        const auto visible = getVisibleResidentIndices(currentHabitat);
        for (int i : visible)
            stepResident(i, dt);
        checkMeetings();
        updateToys(dt);
        updateVisitor(dt);

        // The Commons berry tree drops a berry now and then; residents nearby may go for it.
        if (currentHabitat == ParkEnvironmentId::Commons)
        {
            berryTimer -= dt;
            if (berryTimer <= 0.0f)
            {
                berryTimer = frange(35, 70);
                const auto &tree = getLandmarks(currentHabitat)[0];
                ParkToy berry;
                berry.kind = ParkToyKind::Berry;
                berry.flavor = static_cast<ParkFlavor>(rand() % 5);
                berry.fromX = tree.x + frange(-30, 30);
                berry.fromY = tree.y - 90.0f;
                berry.x = tree.x + frange(-60, 60);
                berry.y = tree.y + frange(0, 40);
                berry.flight = 0.4f;
                placeToy(berry);
            }
        }

        // A shiny resident catches the light; the curious come to look.
        sparkleTimer -= dt;
        if (sparkleTimer <= 0.0f)
        {
            sparkleTimer = frange(14, 28);
            for (int i : visible)
            {
                if (!residents[i].shiny)
                    continue;
                emit(ParkParticleKind::Sparkle, residents[i].x, residents[i].y, 6);
                setResidentEmotion(i, ParkEmotion::Sparkle, 2.0f);
                for (int j : visible)
                {
                    ParkResident &other = residents[j];
                    if (j == i || absorbed(other.activity))
                        continue;
                    if ((other.personality == ParkPersonality::Curious || other.personality == ParkPersonality::Playful) &&
                        dist(other.x, other.y, residents[i].x, residents[i].y) < 420.0f)
                    {
                        startActivity(j, ParkActivity::Sniff, 4.0f, i);
                        other.targetX = residents[i].x + (other.x < residents[i].x ? -55.0f : 55.0f);
                        other.targetY = residents[i].y;
                        setResidentEmotion(j, ParkEmotion::Exclamation, 1.5f);
                    }
                }
                break;
            }
        }

        spawnAmbient(dt);
        updateParticles(dt);

        // Needs, bonds and favourite spots are worth keeping, but not every frame.
        saveTimer += dt;
        if (saveTimer > 45.0f)
        {
            saveTimer = 0.0f;
            lastSimulationTime = static_cast<uint64_t>(std::time(nullptr));
            saveState();
        }
    }
}
