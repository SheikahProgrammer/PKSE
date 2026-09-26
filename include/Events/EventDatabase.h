#ifndef EVENTS_EVENT_DATABASE_H
#define EVENTS_EVENT_DATABASE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <span>

#include "Enums/GameVersion.h"
#include "Pokemon/Pokemon.h"
#include "Trainer/Trainer.h"

namespace Events {

#pragma pack(push, 1)
struct EventRecord {
    uint16_t id;             // Unique sequential ID
    uint8_t  generation;     // 3, 6, 7, 8, 9
    uint8_t  gameVersion;    // Target game version enum (or 0 for any)
    uint16_t species;        // National species ID (1 .. 1025)
    uint8_t  form;           // Form index
    uint8_t  isShiny;        // 0 = Normal, 1 = Shiny
    uint8_t  level;          // Event level (1 .. 100)
    uint8_t  gender;         // 0 = Male, 1 = Female, 2 = Genderless
    uint8_t  ball;           // Ball ID (e.g. 16 = Cherish Ball, 4 = Poke Ball)
    uint8_t  nature;         // Nature ID (0 .. 24), or 255 = Random
    uint8_t  abilityNum;     // Ability index (0, 1, 2 = Hidden)
    uint8_t  padding1;       // 0
    uint16_t heldItem;       // Held item ID
    uint16_t moves[4];       // Move IDs (0 = None)
    uint16_t relearnMoves[4];// Relearn move IDs
    uint16_t metLocation;    // Met location ID
    uint8_t  metLevel;       // Level met at
    uint8_t  metYear;        // Years since 2000
    uint8_t  metMonth;       // Month (1 .. 12)
    uint8_t  metDay;         // Day (1 .. 31)
    uint16_t tid16;          // Visible 16-bit TID
    uint16_t sid16;          // Secret 16-bit SID
    uint32_t id32;           // 32-bit trainer ID
    char     otName[24];     // Original Trainer name (UTF-8)
    char     title[48];      // Event display title (UTF-8)
    uint16_t cardId;         // Wonder card ID
    uint8_t  fateful;        // 1 if fateful encounter
    uint8_t  ribbonClassic;  // 1 if classic ribbon
    uint8_t  ribbonEvent;    // 1 if event ribbon
    uint8_t  ribbonPremier;  // 1 if premier ribbon
    uint8_t  ribbonWishing;  // 1 if wishing ribbon
    uint8_t  ribbonSouvenir; // 1 if souvenir ribbon
    uint8_t  padding2[18];   // Padding to 144 bytes
};
#pragma pack(pop)

static_assert(sizeof(EventRecord) == 144, "EventRecord struct size must be 144 bytes");

class EventDatabase {
public:
    static EventDatabase& getInstance();

    bool load();
    bool isLoaded() const noexcept { return loaded; }
    size_t getEventCount() const noexcept { return events.size(); }

    const EventRecord* getEvent(size_t index) const noexcept;
    std::vector<size_t> filter(Enums::GameVersion group, const std::string& query = "", bool matchCurrentGameOnly = false) const;

    /// Materializes the event as a live Pokemon entity for the given trainer
    std::unique_ptr<Pokemon::Pokemon> instantiate(const EventRecord& rec, const Trainer::Trainer& trainer) const;

    /// Materializes the event in its native generation format (for depositing directly into Bank)
    std::unique_ptr<Pokemon::Pokemon> instantiateNative(const EventRecord& rec) const;

    /// Helper to test if an event is directly compatible with a given game group
    static bool isCompatibleWithGame(const EventRecord& rec, Enums::GameVersion group) noexcept;

private:
    EventDatabase() = default;
    bool loaded = false;
    std::vector<EventRecord> events;
};

} // namespace Events

#endif // EVENTS_EVENT_DATABASE_H