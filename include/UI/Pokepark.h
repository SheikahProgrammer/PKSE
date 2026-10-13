#ifndef UI_POKEPARK_H
#define UI_POKEPARK_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>

namespace Trainer
{
    class Trainer;
    class Bank;
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
        Grumpy
    };

    enum class ParkEmotion : uint8_t
    {
        None = 0,
        Heart,
        Music,
        Exclamation,
        Sleep,
        Angry,
        Question
    };

    enum class ParkJournalEventKind : uint8_t
    {
        Activity = 0,
        Discovery,
        Friendship,
        Conflict,
        Reconciliation,
        Affection
    };

    struct ParkRelationship
    {
        std::string firstId;
        std::string secondId;
        int friendship = 0;   // -100 to 100
        int affection = 0;    // 0 to 100
        int rivalry = 0;      // 0 to 100
        int sharedMoments = 0;
        uint64_t lastEventAt = 0;

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
        std::string id;       // e.g. "save:0:4" or "party:1" or "bank:0:2"
        uint16_t species = 0;
        uint8_t form = 0;
        bool shiny = false;
        std::string name;
        std::string sourceInfo;
        uint8_t type1 = 0;
        uint8_t type2 = 255;
        ParkEnvironmentId homeHabitat = ParkEnvironmentId::Commons;
        ParkPersonality personality = ParkPersonality::Balanced;
        ParkMood mood = ParkMood::Content;
        std::string currentActivity = "wandering";

        // Real-time animation & wander state in the habitat scene
        float x = 200.0f;
        float y = 350.0f;
        float targetX = 200.0f;
        float targetY = 350.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        float wanderTimer = 0.0f;
        bool facingLeft = false;
        bool isMoving = false;
        float animClock = 0.0f;

        // Social / interaction state
        ParkEmotion emotion = ParkEmotion::None;
        float emotionTimer = 0.0f;
        int interactingWith = -1;
        float interactionCooldown = 0.0f;

        bool enjoysHabitat(ParkEnvironmentId env) const;
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
        static constexpr size_t MAX_JOURNAL_ENTRIES = 80;

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
        void updateRelationship(const std::string &idA, const std::string &idB, int dFriendship, int dAffection, int dRivalry);

        // Resident interactions
        std::string interact(int residentIndex, const std::string &action);
        void setResidentEmotion(int residentIndex, ParkEmotion emotion, float duration = 3.5f);

        // Auto-fill and Roster management
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

        void runOfflineSimulation(uint64_t previous, uint64_t now);
        void checkRandomInteractions(float dt);
        void stepResidentMotion(ParkResident &res, float dt, float minX, float maxX, float minY, float maxY);
    };

    namespace ParkDialogue
    {
        std::string generateLine(const ParkResident &resident, const std::string &action,
                                 const std::string &nearbyName, ParkEnvironmentId env);
    }
}

#endif // UI_POKEPARK_H
