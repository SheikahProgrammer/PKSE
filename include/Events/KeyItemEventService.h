#ifndef EVENTS_KEY_ITEM_EVENT_SERVICE_H
#define EVENTS_KEY_ITEM_EVENT_SERVICE_H

#include <cstdint>
#include <string>
#include <vector>
#include "Trainer/Trainer.h"

namespace Events {

// Where a key-item distribution event stands in a save. Mirrors PKForge.Engine.KeyItemEventState.
enum class KeyItemEventState {
    NotEnabled, // Neither the item nor the event state is present
    Partial,    // Item in the bag without the flags/vars (or vice versa): the game ignores it
    Enabled,    // Item and every flag/var the real distribution writes are present
    Used,       // The player already showed the ticket (Gen 3 "shown" flag / Crystal "received GS Ball")
    Completed   // The event legendary was caught or defeated
};

struct KeyItemEventStatus {
    std::string id;           // Stable key, e.g. "e-aurora"
    std::string title;        // "Aurora Ticket"
    std::string destination;  // "Birth Island"
    uint16_t itemId = 0;      // Save-native item id (Gen 3 internal ids on Gen 3); 0 when no item
    std::string itemName;     // Display name of the item, or the title when there is none
    bool hasItem = false;
    bool eventArmed = false;
    bool used = false;
    bool completed = false;
    bool needsItem = true;
    KeyItemEventState state = KeyItemEventState::NotEnabled;
    std::string prerequisite; // Empty when nothing else blocks the event in game
};

// Key-item Mystery Gift events for the retail Gen 2-6 games: the ticket/flute/card plus the exact
// flags and vars the official distribution writes. A straight port of PKForge.Engine's
// KeyItemEventService (PKHeX event labels + pret disassemblies), sharing its ids and numbers.
class KeyItemEventService {
public:
    static bool isSupported(const Trainer::Trainer &trainer);
    static std::vector<KeyItemEventStatus> getEvents(Trainer::Trainer &trainer);
    // Gives the item and writes every flag/var the official distribution writes. `message` gets a
    // one-line result for the status bar.
    static bool enableEvent(Trainer::Trainer &trainer, const std::string &eventId, std::string &message);
    // Takes the item and clears the distribution flags/vars. History (shown/caught flags, story
    // state vars) is left alone: it records what the player actually did.
    static bool disableEvent(Trainer::Trainer &trainer, const std::string &eventId, std::string &message);
};

// PKForge KeyItemEventsEditor wording.
inline const char *keyItemStateLabel(KeyItemEventState state)
{
    switch (state)
    {
    case KeyItemEventState::Enabled: return "Ready";
    case KeyItemEventState::Partial: return "Incomplete";
    case KeyItemEventState::Used: return "Used";
    case KeyItemEventState::Completed: return "Completed";
    default: return "Off";
    }
}

inline const char *keyItemStateDetail(KeyItemEventState state)
{
    switch (state)
    {
    case KeyItemEventState::Enabled: return "Ready: the event will trigger in game.";
    case KeyItemEventState::Partial: return "Incomplete: item and flags disagree, so the game ignores it. Enable to fix.";
    case KeyItemEventState::Used: return "Already used in game: the ticket was shown to the sailor, or the GS Ball was received.";
    case KeyItemEventState::Completed: return "The event Pok\u00e9mon was already caught or defeated.";
    default: return "Not enabled.";
    }
}

} // namespace Events

#endif // EVENTS_KEY_ITEM_EVENT_SERVICE_H
