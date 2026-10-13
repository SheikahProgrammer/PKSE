#ifndef UI_POKEPARK_H
#define UI_POKEPARK_H

#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Trainer
{
    class Trainer;
    class Bank;
}
namespace Pokemon
{
    class Pokemon;
}

namespace UI
{
    enum class ParkEnvironmentId : uint8_t
    {
        Commons = 0,
        WaterfallLake = 1,
        EmberGrove = 2,
        SkySummit = 3
    };

    enum class ParkPersonality : uint8_t
    {
        Balanced = 0,
        Playful,
        Gentle,
        Bold,
        Shy,
        Curious,
        Energetic,
        Calm,
        Helpful,
        Proud
    };

    enum class ParkMood : uint8_t
    {
        Content = 0,
        Playful,
        Sleepy,
        Excited,
        Curious,
        Grumpy,
        Lonely
    };

    enum class ParkEmotion : uint8_t
    {
        None = 0,
        Heart,
        Music,
        Exclamation,
        Sleep,
        Angry,
        Question,
        Sweat,
        Sparkle
    };

    enum class ParkJournalEventKind : uint8_t
    {
        Activity = 0,
        Discovery,
        Friendship,
        Conflict,
        Reconciliation,
        Affection,
        Bond,    // two residents became best friends, or rivals
        Visitor, // a wild Pokemon wandered through
        Weather,
        Story,   // the end of something that played out over days
        Photo
    };

    /// What a resident is doing right now. Each has its own pace, pose and way of ending.
    enum class ParkActivity : uint8_t
    {
        Idle = 0,  // standing, looking around
        Wander,    // a stroll to a chosen spot
        Run,       // zoomies: fast laps
        Sit,       // resting in place
        Nap,       // asleep
        Sniff,     // investigating a landmark, a toy, a newcomer
        Approach,  // going over to another resident
        Follow,    // walking with a friend or the group leader
        Flee,      // backing off from someone (shy, scared)
        Play,      // chasing / hopping about with a partner
        Spar,      // posturing and play-fighting with a rival
        Sulk,      // after a squabble
        Celebrate, // a little victory hop
        Swim,      // in the water (Water types)
        Perch,     // high up (Flying types, the lookout)
        Mediate,   // walking over to calm a squabble
        Fetch,     // going for a thrown ball or berry
        Eat,       // eating a berry
        Called,    // coming to the player
        Watch      // watching something from a distance (shy residents)
    };

    /// Ground under a point of a habitat. Each resident may walk a subset (see ParkTraversal).
    enum class ParkTerrain : uint8_t
    {
        Blocked = 0,
        Land,
        Water,
        Warm, // the ember stones: walkable, but only Fire/Ground/Rock types like to stand on them
        Air   // above the plateau: Flying types only
    };

    enum ParkTraversal : uint8_t
    {
        TraverseLand = 1,
        TraverseWater = 2,
        TraverseWarm = 4,
        TraverseAir = 8
    };

    enum class ParkWeather : uint8_t
    {
        Clear = 0,
        Rain,
        Snow,
        Heat,
        Wind,
        Fog
    };

    enum class ParkLandmarkKind : uint8_t
    {
        BerryTree = 0,
        Waterfall,
        CinderRing,
        Lookout
    };

    struct ParkLandmark
    {
        ParkLandmarkKind kind;
        float x, y;   // where residents gather (scene coordinates)
        float radius; // how close counts as "at" it
        const char *name;
    };

    /// Berry flavours, in the order the natures' liked/disliked flavours are numbered.
    enum class ParkFlavor : uint8_t
    {
        Spicy = 0,
        Dry,
        Sweet,
        Bitter,
        Sour
    };

    struct ParkRelationship
    {
        std::string firstId;
        std::string secondId;
        int friendship = 0; // -100 to 100
        int affection = 0;  // 0 to 100
        int rivalry = 0;    // 0 to 100
        int sharedMoments = 0;
        uint64_t lastEventAt = 0;
        uint64_t lastConflictAt = 0; // an unresolved squabble (0 = none open)
        uint8_t announced = 0;       // bit 0: best friends written up, bit 1: rivals written up

        std::string key() const;
        static std::string makeKey(const std::string &a, const std::string &b);
    };

    struct ParkJournalEvent
    {
        uint64_t timestamp = 0;
        std::string timeStr;
        ParkJournalEventKind kind = ParkJournalEventKind::Activity;
        std::string text;
        std::string habitatName;
        std::vector<std::string> residentIds;
        uint16_t species1 = 0;
        uint16_t species2 = 0;
    };

    struct ParkResident
    {
        std::string id; // e.g. "box:0:4" or "party:1"
        uint16_t species = 0;
        uint8_t form = 0;
        bool shiny = false;
        std::string name;
        std::string sourceInfo;
        uint8_t type1 = 0;
        uint8_t type2 = 255;
        uint8_t nature = 0;
        ParkEnvironmentId homeHabitat = ParkEnvironmentId::Commons;
        ParkPersonality personality = ParkPersonality::Balanced;
        ParkMood mood = ParkMood::Content;
        std::string currentActivity = "Wandering"; // shown on the profile card

        // Derived from the species when the park loads (never persisted).
        uint8_t baseSpeed = 50;
        float weightKg = 20.0f;
        uint16_t familyRoot = 0; // first stage of its evolution family
        uint8_t traversal = TraverseLand;
        bool nocturnal = false;
        bool small = false; // hops in short bursts

        // Needs, 0..100. Energy is a reserve (low = tired); the others are urges (high = wants it).
        float energy = 80.0f;
        float social = 40.0f;
        float play = 40.0f;
        float curiosity = 40.0f;
        int bond = 20; // how close it is to the player, 0..100

        // Behaviour.
        ParkActivity activity = ParkActivity::Idle;
        float activityTimer = 0.0f;
        int partner = -1;    // the resident an activity is with or about
        int leader = -1;     // the group this resident walks with
        int toy = -1;        // the toy it is going for / holding
        float favouriteX = -1.0f, favouriteY = -1.0f; // its spot, learnt over time

        // Motion (scene coordinates; feet at (x, y)).
        float x = 200.0f, y = 350.0f;
        float targetX = 200.0f, targetY = 350.0f;
        float speed = 0.0f; // current, eased toward the activity's pace
        float vx = 0.0f, vy = 0.0f;
        bool facingLeft = false;
        bool isMoving = false;
        float animClock = 0.0f;
        float hop = 0.0f;     // 0..1 height of the current hop
        float stepDust = 0.0f;

        // Feedback.
        ParkEmotion emotion = ParkEmotion::None;
        float emotionTimer = 0.0f;
        float emotionAge = 0.0f; // seconds the bubble has been up (pop-in)
        float interactionCooldown = 0.0f;

        bool enjoysHabitat(ParkEnvironmentId env) const;
        bool hasType(uint8_t type) const { return type1 == type || type2 == type; }
    };

    enum class ParkToyKind : uint8_t
    {
        Ball = 0,
        Berry
    };

    struct ParkToy
    {
        ParkToyKind kind = ParkToyKind::Ball;
        ParkFlavor flavor = ParkFlavor::Sweet;
        float fromX = 0, fromY = 0, x = 0, y = 0;
        float flight = 0.0f; // 0..1 along the throw arc; 1 = landed
        float life = 0.0f;   // seconds since it landed
        int holder = -1;     // resident batting it about / eating it
        bool gone = false;
    };

    enum class ParkParticleKind : uint8_t
    {
        Dust = 0,
        Splash,
        Ember,
        Snow,
        Leaf,
        Rain,
        Firefly,
        Mist,
        Sparkle,
        Heart
    };

    struct ParkParticle
    {
        ParkParticleKind kind;
        float x, y, vx, vy;
        float life, maxLife;
        float size;
    };

    /// A wild Pokemon passing through the habitat for a little while.
    struct ParkVisitor
    {
        bool active = false;
        uint16_t species = 0;
        bool shiny = false;
        std::string name;
        float x = 0, y = 0, targetX = 0, targetY = 0;
        float timer = 0.0f; // seconds left before it heads off
        bool leaving = false;
        bool facingLeft = false;
        float animClock = 0.0f;
    };

    struct ParkEnvironmentInfo
    {
        ParkEnvironmentId id;
        const char *name;
        const char *subtitle;
        const char *landmark;
        std::vector<uint8_t> favouredTypes;
        bool favoursShy;
        std::vector<std::string> ambientLines;
    };

    class PokeparkManager
    {
    public:
        static constexpr size_t MAX_RESIDENTS = 12;
        static constexpr size_t DEFAULT_RESIDENTS = 8;
        static constexpr size_t MAX_JOURNAL_ENTRIES = 120;

        // The scene, in the panel's own coordinates: where residents' feet may go.
        static constexpr float SCENE_W = 1216.0f, SCENE_H = 588.0f;
        static constexpr float PLAY_MIN_X = 60.0f, PLAY_MAX_X = 1156.0f;
        static constexpr float PLAY_MIN_Y = 196.0f, PLAY_MAX_Y = 524.0f;
        // Where "you" stand when a resident is called over: the front of the scene.
        static constexpr float PLAYER_X = SCENE_W / 2.0f, PLAYER_Y = 500.0f;

        static PokeparkManager &instance();

        void initialize(Trainer::Trainer *trainer, Trainer::Bank *bank = nullptr);
        void update(float dt);

        ParkEnvironmentId getCurrentHabitat() const noexcept { return currentHabitat; }
        void setHabitat(ParkEnvironmentId habitat) noexcept { currentHabitat = habitat; }
        void cycleHabitat(int direction) noexcept;

        const std::vector<ParkResident> &getResidents() const noexcept { return residents; }
        std::vector<int> getVisibleResidentIndices(ParkEnvironmentId env) const;

        const std::vector<ParkJournalEvent> &getJournal() const noexcept { return journal; }
        void clearJournal();

        ParkRelationship getRelationship(const std::string &idA, const std::string &idB);
        void updateRelationship(const std::string &idA, const std::string &idB, int dFriendship, int dAffection,
                                int dRivalry);
        /// "Best friends with X" / "Rivals with Y" for the profile card, or "" when it has neither.
        std::string bondSummary(int residentIndex);

        // Player interactions. Each returns the line the resident's reaction is narrated with.
        std::string interact(int residentIndex, const std::string &action);
        std::string feedBerry(int residentIndex, ParkFlavor flavor);
        std::string callResident(int residentIndex);
        /// Throws a ball / berry to the scene point (x, y); nearby residents decide what to do about it.
        void throwToy(ParkToyKind kind, float x, float y, ParkFlavor flavor = ParkFlavor::Sweet);
        void setResidentEmotion(int residentIndex, ParkEmotion emotion, float duration = 3.5f);
        void recordPhoto(const std::string &fileName);

        // The living world, for the renderer.
        const std::vector<ParkToy> &getToys() const noexcept { return toys; }
        const std::vector<ParkParticle> &getParticles() const noexcept { return particles; }
        const ParkVisitor &getVisitor() const noexcept { return visitor; }
        ParkWeather getWeather() const noexcept { return weather; }
        int getHour() const noexcept { return hour; }
        static const char *getWeatherName(ParkWeather w);
        static ParkTerrain terrainAt(ParkEnvironmentId env, float x, float y);
        static const std::vector<ParkLandmark> &getLandmarks(ParkEnvironmentId env);
        /// Seconds until the berry tree next drops a berry (Commons), for the renderer's ripening berries.
        float berryRipeness() const noexcept { return berryTimer; }

        // Roster management
        void autoFillRoster(Trainer::Trainer *trainer, Trainer::Bank *bank);
        void removeResident(int residentIndex);
        void addResidentFromSave(int box, int slot, const Trainer::Trainer &trainer);

        // Persistence
        void loadState();
        void saveState();

        static const ParkEnvironmentInfo &getEnvironmentInfo(ParkEnvironmentId id);
        static ParkPersonality derivePersonality(uint8_t nature, uint32_t pid);
        static const char *getPersonalityName(ParkPersonality p);
        static const char *getPersonalityDescription(ParkPersonality p);
        static const char *getMoodName(ParkMood m);
        static const char *getActivityName(ParkActivity a);
        static const char *getFlavorName(ParkFlavor f);
        static int compatibility(const ParkResident &a, const ParkResident &b);

    private:
        PokeparkManager() = default;

        ParkEnvironmentId currentHabitat = ParkEnvironmentId::Commons;
        std::vector<ParkResident> residents;
        std::vector<ParkJournalEvent> journal;
        std::map<std::string, ParkRelationship> relationships;
        uint64_t lastSimulationTime = 0;
        float realTimeUpdateClock = 0.0f;
        Trainer::Trainer *activeTrainer = nullptr;
        Trainer::Bank *activeBank = nullptr;

        // The living world.
        std::vector<ParkToy> toys;
        std::vector<ParkParticle> particles;
        ParkVisitor visitor;
        ParkWeather weather = ParkWeather::Clear;
        int hour = 12;
        float worldClock = 0.0f;   // seconds the park has been on screen this visit
        float visitorTimer = 90.0f; // until the next wild visitor may come
        float berryTimer = 25.0f;   // until the berry tree next drops a berry
        float sparkleTimer = 12.0f; // until a shiny resident next sparkles
        float weatherCheck = 0.0f;
        float saveTimer = 0.0f;

        void runOfflineSimulation(uint64_t previous, uint64_t now);
        static void refreshDerived(ParkResident &res);
        static ParkResident makeResident(const std::string &id, const std::string &source,
                                         const Pokemon::Pokemon &mon, const Trainer::Trainer &trainer);
        void refreshClock();

        // Behaviour (PokeparkWorld.cpp).
        void stepResident(int index, float dt);
        void chooseActivity(int index);
        void startActivity(int index, ParkActivity activity, float duration, int partner = -1);
        void steer(ParkResident &res, float dt, float pace);
        bool pickSpot(const ParkResident &res, float &outX, float &outY, int tries = 24) const;
        bool canStand(const ParkResident &res, float x, float y) const;
        void resolveMeeting(int a, int b);
        void checkMeetings();
        void updateToys(float dt);
        void placeToy(const ParkToy &toy);
        void updateVisitor(float dt);
        void updateParticles(float dt);
        void spawnAmbient(float dt);
        void emit(ParkParticleKind kind, float x, float y, int count);
        void addJournal(ParkJournalEventKind kind, const std::string &text, const std::vector<int> &who,
                        uint64_t when = 0);
        void noteBonds(int a, int b);
        int findMediator(int a, int b) const;
        float paceFor(const ParkResident &res, ParkActivity activity) const;
        bool isNight() const { return hour >= 21 || hour < 6; }
    };

    namespace ParkDialogue
    {
        std::string generateLine(const ParkResident &resident, const std::string &action,
                                 const std::string &nearbyName, ParkEnvironmentId env);
    }
}

#endif // UI_POKEPARK_H
