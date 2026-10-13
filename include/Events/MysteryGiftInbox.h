#ifndef EVENTS_MYSTERY_GIFT_INBOX_H
#define EVENTS_MYSTERY_GIFT_INBOX_H

#include <cstdint>
#include <string>
#include <vector>
#include "Trainer/Trainer.h"

namespace Events {

struct MysteryGiftCardRecord {
    int slotIndex = 0;
    uint16_t cardId = 0;
    std::string title;
    uint8_t type = 0; // 0 = Pokemon, 1 = Item, 2 = Other
    uint16_t species = 0;
    uint8_t level = 0;
    bool used = false;
    bool empty = true;
    bool receivedRecord = false; // a received-gift history entry (BDSP) rather than a stored card
};

// Read-only view of the Mystery Gift cards physically stored in a save, as PKForge's
// MysteryGiftInboxEditor: receiving, deleting and received-flag mutation are intentionally not
// exposed. Empty slots are skipped.

class MysteryGiftInbox {
public:
    static bool isInboxSupported(const Trainer::Trainer &trainer);
    static std::vector<MysteryGiftCardRecord> getInboxCards(Trainer::Trainer &trainer);
};

} // namespace Events

#endif // EVENTS_MYSTERY_GIFT_INBOX_H
