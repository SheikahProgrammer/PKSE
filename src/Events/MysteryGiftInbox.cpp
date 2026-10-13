#include "Events/MysteryGiftInbox.h"

#include <cstring>
#include <algorithm>

#include "Trainer/Trainer4DP.h"
#include "Trainer/Trainer4PT.h"
#include "Trainer/Trainer4HGSS.h"
#include "Trainer/Trainer5BW.h"
#include "Trainer/Trainer5B2W2.h"
#include "Trainer/Trainer6XY.h"
#include "Trainer/Trainer6ORAS.h"
#include "Trainer/Trainer7SM.h"
#include "Trainer/Trainer7USUM.h"
#include "Trainer/Trainer8BDSP.h"
#include "Trainer/Blocks6XY.h"
#include "Trainer/Blocks6ORAS.h"
#include "Trainer/Blocks7SM.h"
#include "Trainer/Blocks7USUM.h"

namespace Events {

namespace {

inline uint16_t readU16LE(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t *p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

std::string utf16leToUtf8(const uint8_t *bytes, size_t maxBytes) {
    std::string s;
    for (size_t i = 0; i + 1 < maxBytes; i += 2) {
        uint16_t ch = bytes[i] | (bytes[i + 1] << 8);
        if (ch == 0 || ch == 0xFFFF) break;
        if (ch < 0x80) {
            s.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            s.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            s.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return s;
}

void cryptGen5(uint8_t *data, size_t length, uint32_t seed) {
    for (size_t i = 0; i + 1 < length; i += 2) {
        seed = (0x41C64E6D * seed) + 0x00006073;
        uint16_t xorVal = static_cast<uint16_t>(seed >> 16);
        uint16_t val = data[i] | (data[i + 1] << 8);
        val ^= xorVal;
        data[i] = val & 0xFF;
        data[i + 1] = (val >> 8) & 0xFF;
    }
}

} // namespace

bool MysteryGiftInbox::isInboxSupported(const Trainer::Trainer &trainer) {
    const auto g = trainer.getGameGroup();
    return g == GameVersion::DP   || g == GameVersion::PT   || g == GameVersion::HGSS ||
           g == GameVersion::BW   || g == GameVersion::B2W2 ||
           g == GameVersion::XY   || g == GameVersion::ORAS ||
           g == GameVersion::SM   || g == GameVersion::USUM ||
           g == GameVersion::BDSP;
}

std::vector<MysteryGiftCardRecord> MysteryGiftInbox::getInboxCards(Trainer::Trainer &trainer) {
    std::vector<MysteryGiftCardRecord> records;
    const auto g = trainer.getGameGroup();

    // =========================================================================
    // Gen 4 (DP, Pt, HGSS)
    // =========================================================================
    if (g == GameVersion::DP || g == GameVersion::PT || g == GameVersion::HGSS) {
        const uint8_t *gen = nullptr;
        size_t ofsMystery = 0;
        size_t cardStart = 0x100;

        if (g == GameVersion::DP) {
            gen = static_cast<const Trainer::Trainer4DP &>(trainer).getGeneralBlock();
            ofsMystery = 0xA6D0;
            cardStart = 0x100 + 44; // 11 sentinels
        } else if (g == GameVersion::PT) {
            gen = static_cast<const Trainer::Trainer4PT &>(trainer).getGeneralBlock();
            ofsMystery = 0xB4C0;
            cardStart = 0x100;
        } else {
            gen = static_cast<const Trainer::Trainer4HGSS &>(trainer).getGeneralBlock();
            ofsMystery = 0x9D3C;
            cardStart = 0x100;
        }

        const uint8_t *mystery = gen + ofsMystery;
        const uint8_t *pcdBase = mystery + cardStart + (8 * 0x104); // 3 PCD cards

        for (int i = 0; i < 3; ++i) {
            const uint8_t *pcd = pcdBase + (i * 0x358);
            uint16_t cardId = readU16LE(pcd + 0x150);
            std::string title = utf16leToUtf8(pcd + 0x104, 0x48);
            uint16_t cardType = readU16LE(pcd + 0x00);

            MysteryGiftCardRecord r;
            r.slotIndex = i;
            r.cardId = cardId;
            r.title = title;
            r.type = (cardType == 1) ? 0 : 1; // 0 = Pokemon, 1 = Item
            r.empty = (cardId == 0 && title.empty());
            r.used = false;

            if (!r.empty && r.title.empty()) {
                r.title = "Wonder Card #" + std::to_string(cardId);
            }
            if (!r.empty)
                records.push_back(r);
        }
        return records;
    }

    // =========================================================================
    // Gen 5 (BW, B2W2)
    // =========================================================================
    if (g == GameVersion::BW || g == GameVersion::B2W2) {
        const auto &saveData = (g == GameVersion::BW)
            ? static_cast<const Trainer::Trainer5BW &>(trainer).getSaveData()
            : static_cast<const Trainer::Trainer5B2W2 &>(trainer).getSaveData();

        constexpr size_t blockOfs = 0x1C800;
        constexpr size_t dataSize = 0xA90;
        if (saveData.size() >= blockOfs + 0xA94) {
            uint32_t seed = readU32LE(saveData.data() + blockOfs + dataSize);
            std::vector<uint8_t> buf(saveData.begin() + blockOfs, saveData.begin() + blockOfs + dataSize);
            cryptGen5(buf.data(), dataSize, seed);

            for (int i = 0; i < 12; ++i) {
                const uint8_t *pgf = buf.data() + 0x100 + (i * 0xCC);
                uint16_t cardId = readU16LE(pgf + 0xB0);
                std::string title = utf16leToUtf8(pgf + 0x60, 74);
                uint8_t cardType = pgf[0xB3];
                uint16_t species = readU16LE(pgf + 0x1A);
                uint8_t level = pgf[0x5B];
                bool used = (pgf[0xB4] & 2) != 0;

                MysteryGiftCardRecord r;
                r.slotIndex = i;
                r.cardId = cardId;
                r.title = title;
                r.type = (cardType == 1) ? 0 : 1;
                r.species = species;
                r.level = level;
                r.used = used;
                r.empty = (cardId == 0 && title.empty());

                if (!r.empty && r.title.empty()) {
                    r.title = "Wonder Card #" + std::to_string(cardId);
                }
                if (!r.empty)
                    records.push_back(r);
            }
        }
        return records;
    }

    // =========================================================================
    // Gen 6 (XY, ORAS)
    // =========================================================================
    if (g == GameVersion::XY || g == GameVersion::ORAS) {
        const auto &saveData = (g == GameVersion::XY)
            ? static_cast<const Trainer::Trainer6XY &>(trainer).getSaveData()
            : static_cast<const Trainer::Trainer6ORAS &>(trainer).getSaveData();

        const size_t blockOfs = (g == GameVersion::XY) ? 0x01BC00 : 0x01CC00;

        if (saveData.size() >= blockOfs + 0x100 + (24 * 0x108)) {
            for (int i = 0; i < 24; ++i) {
                const uint8_t *wc6 = saveData.data() + blockOfs + 0x100 + (i * 0x108);
                uint16_t cardId = readU16LE(wc6);
                std::string title = utf16leToUtf8(wc6 + 2, 0x4A);
                uint8_t cardType = wc6[0x51];
                bool used = (wc6[0x52] & 2) != 0;
                uint16_t species = readU16LE(wc6 + 0x82);
                uint8_t level = wc6[0xD0];

                MysteryGiftCardRecord r;
                r.slotIndex = i;
                r.cardId = cardId;
                r.title = title;
                r.type = (cardType == 0) ? 0 : 1;
                r.species = species;
                r.level = level;
                r.used = used;
                r.empty = (cardId == 0 && title.empty());

                if (!r.empty && r.title.empty()) {
                    r.title = "Wonder Card #" + std::to_string(cardId);
                }
                if (!r.empty)
                    records.push_back(r);
            }
        }
        return records;
    }

    // =========================================================================
    // Gen 7 (SM, USUM)
    // =========================================================================
    if (g == GameVersion::SM || g == GameVersion::USUM) {
        const auto &saveData = (g == GameVersion::SM)
            ? static_cast<const Trainer::Trainer7SM &>(trainer).getSaveData()
            : static_cast<const Trainer::Trainer7USUM &>(trainer).getSaveData();

        const size_t blockOfs = (g == GameVersion::SM) ? 0x065C00 : 0x066200;

        if (saveData.size() >= blockOfs + 0x100 + (48 * 0x108)) {
            for (int i = 0; i < 48; ++i) {
                const uint8_t *wc7 = saveData.data() + blockOfs + 0x100 + (i * 0x108);
                uint16_t cardId = readU16LE(wc7);
                std::string title = utf16leToUtf8(wc7 + 2, 0x4A);
                uint8_t cardType = wc7[0x51];
                bool used = (wc7[0x52] & 2) != 0;
                uint16_t species = readU16LE(wc7 + 0x82);
                uint8_t level = wc7[0xD0];

                MysteryGiftCardRecord r;
                r.slotIndex = i;
                r.cardId = cardId;
                r.title = title;
                r.type = (cardType == 0) ? 0 : 1;
                r.species = species;
                r.level = level;
                r.used = used;
                r.empty = (cardId == 0 && title.empty());

                if (!r.empty && r.title.empty()) {
                    r.title = "Wonder Card #" + std::to_string(cardId);
                }
                if (!r.empty)
                    records.push_back(r);
            }
        }
        return records;
    }

    // =========================================================================
    // Gen 8 (BDSP)
    // =========================================================================
    if (g == GameVersion::BDSP) {
        const auto &saveData = static_cast<const Trainer::Trainer8BDSP &>(trainer).getSaveData();
        constexpr size_t ofsMystery = 0xE9BE8;

        if (saveData.size() >= ofsMystery + (50 * 0xE0)) {
            for (int i = 0; i < 50; ++i) {
                const uint8_t *recv = saveData.data() + ofsMystery + (i * 0xE0);
                uint16_t deliveryId = readU16LE(recv + 0x08);
                uint8_t dataType = recv[0x0C];
                uint16_t species = readU16LE(recv + 0x10);
                std::string ot = utf16leToUtf8(recv + 0x1E, 0x1A);

                MysteryGiftCardRecord r;
                r.slotIndex = i;
                r.cardId = deliveryId;
                r.type = (dataType == 1) ? 0 : 1;
                r.species = species;
                r.empty = (deliveryId == 0);
                r.used = true; // BDSP stores received history log
                r.receivedRecord = true;
                r.title = r.empty ? "" : ("Gift #" + std::to_string(deliveryId) + (ot.empty() ? "" : (" (" + ot + ")")));
                if (!r.empty)
                    records.push_back(r);
            }
        }
        return records;
    }

    return records;
}

} // namespace Events
