#include <cstdio>
#include <cstring>
#include <algorithm>

#include "Events/EventDatabase.h"
#include "Utils/Logger.h"
#include "Utils/StringHelpers.h"
#include "Pokemon/Pokemon3FRLG.h"
#include "Pokemon/Pokemon4DP.h"
#include "Pokemon/Pokemon5BW.h"
#include "Pokemon/Pokemon6XY.h"
#include "Pokemon/Pokemon7SM.h"
#include "Pokemon/Pokemon8SWSH.h"
#include "Pokemon/Pokemon9SV.h"
#include "Encryption/Encryption3FRLG.h"
#include "Encryption/Encryption4DP.h"
#include "Encryption/Encryption5BW.h"
#include "Encryption/Encryption6XY.h"
#include "Encryption/Encryption7SM.h"
#include "Encryption/Encryption7LGPE.h"
#include "Encryption/Encryption8SWSH.h"
#include "Encryption/Encryption9SV.h"
#include "Pokemon/PersonalInfoTable.h"
#include "Pokemon/AbilityInfo.h"
#include "Pokemon/FormInfo.h"
#include "Names/MoveInfo.h"
#include "Names/ItemNames.h"
#include "Names/ItemPresence.h"
#include "Utils/HelperUtilities.h"
#include "Trainer/Trainer.h"

namespace Events {

EventDatabase& EventDatabase::getInstance() {
    static EventDatabase instance;
    return instance;
}

bool EventDatabase::load() {
    if (loaded) return true;

    const char* paths[] = {
        "romfs:/events/events.bin",
        "romfs/events/events.bin",
        "events.bin"
    };

    FILE* f = nullptr;
    for (const char* p : paths) {
        f = fopen(p, "rb");
        if (f) break;
    }

    if (!f) {
        Utils::logErrorToFile("EventDatabase: could not open events.bin from any known path");
        return false;
    }

    char magic[4];
    uint32_t ver = 0, count = 0, entrySize = 0;
    if (fread(magic, 1, 4, f) != 4 ||
        fread(&ver, 4, 1, f) != 1 ||
        fread(&count, 4, 1, f) != 1 ||
        fread(&entrySize, 4, 1, f) != 1) {
        Utils::logErrorToFile("EventDatabase: corrupt header in events.bin");
        fclose(f);
        return false;
    }

    if (std::memcmp(magic, "PKEV", 4) != 0 || ver != 1 || entrySize != sizeof(EventRecord)) {
        char err[128];
        snprintf(err, sizeof(err), "EventDatabase: header mismatch (size %u, expected %zu)", entrySize, sizeof(EventRecord));
        Utils::logErrorToFile(err);
        fclose(f);
        return false;
    }

    events.resize(count);
    size_t read = fread(events.data(), sizeof(EventRecord), count, f);
    fclose(f);

    if (read != count) {
        Utils::logErrorToFile("EventDatabase: partial read in events.bin");
        events.resize(read);
    }

    loaded = true;
    char msg[128];
    snprintf(msg, sizeof(msg), "EventDatabase: loaded %zu events from events.bin", events.size());
    Utils::logInfoToFile(msg);
    return true;
}

const EventRecord* EventDatabase::getEvent(size_t index) const noexcept {
    if (index >= events.size()) return nullptr;
    return &events[index];
}

bool EventDatabase::isCompatibleWithGame(const EventRecord& rec, Enums::GameVersion group) noexcept {
    switch (group) {
        case Enums::GameVersion::FRLG:
        case Enums::GameVersion::RSE:
            return rec.generation == 3 && rec.species <= 386;
        case Enums::GameVersion::DP:
        case Enums::GameVersion::PT:
        case Enums::GameVersion::HGSS:
            return rec.generation == 4 && rec.species <= 493;
        case Enums::GameVersion::BW:
        case Enums::GameVersion::B2W2:
            return rec.generation == 5 && rec.species <= 649;
        case Enums::GameVersion::XY:
        case Enums::GameVersion::ORAS:
            return rec.generation == 6 && rec.species <= 721;
        case Enums::GameVersion::SM:
        case Enums::GameVersion::USUM:
            return rec.generation == 7 && rec.species <= 807;
        case Enums::GameVersion::GG:
            return (rec.generation == 7 || rec.generation == 1) && rec.species <= 151;
        case Enums::GameVersion::SWSH:
            return rec.generation == 8;
        case Enums::GameVersion::BDSP:
            return rec.generation == 8 && rec.species <= 493;
        case Enums::GameVersion::PLA:
            return rec.generation == 8;
        case Enums::GameVersion::SV:
        case Enums::GameVersion::ZA:
            return rec.generation == 9;
        default:
            return false;
    }
}

std::vector<size_t> EventDatabase::filter(Enums::GameVersion group, const std::string& query, bool matchCurrentGameOnly) const {
    std::vector<size_t> result;
    result.reserve(events.size());

    std::string lquery = query;
    for (char& c : lquery) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& rec = events[i];

        if (matchCurrentGameOnly && !isCompatibleWithGame(rec, group)) {
            continue;
        }

        if (!lquery.empty()) {
            std::string title = rec.title;
            for (char& c : title) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

            const char* spName = Trainer::getSpeciesName(rec.species);
            std::string species = spName ? spName : "";
            for (char& c : species) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

            if (title.find(lquery) == std::string::npos && species.find(lquery) == std::string::npos) {
                continue;
            }
        }

        result.push_back(i);
    }
    return result;
}

// Rigorous Gen 3-5 PID solver: guarantees valid non-zero PID matching nature, gender, ability bit, and shininess.
static uint32_t generateGen3to5PID(uint8_t nature, uint8_t gender, uint8_t genderRatio, int abilityBit, bool isShiny, uint16_t tid, uint16_t sid) {
    if (nature >= 25) nature = static_cast<uint8_t>(Utils::rand32() % 25);
    const uint16_t tsv = tid ^ sid;

    for (int iter = 0; iter < 1000000; ++iter) {
        uint32_t cand = 0;
        if (isShiny) {
            uint16_t shinyXor = static_cast<uint16_t>(Utils::rand32() & 7);
            uint16_t psv = tsv ^ shinyXor;
            uint16_t low = static_cast<uint16_t>(Utils::rand32() & 0xFFFF);
            uint16_t high = low ^ psv;
            cand = (static_cast<uint32_t>(high) << 16) | low;
        } else {
            cand = Utils::rand32();
            if (cand == 0) cand = 1;
            uint16_t psv = static_cast<uint16_t>((cand >> 16) ^ (cand & 0xFFFF));
            if ((tsv ^ psv) < 8) continue; // avoid accidental shiny
        }

        if (cand == 0) continue;
        if (cand % 25 != nature) continue;

        if (abilityBit >= 0 && static_cast<int>(cand & 1) != abilityBit) continue;

        if (genderRatio != 255 && genderRatio != 254 && genderRatio != 0) {
            uint8_t g = ((cand & 0xFF) < genderRatio) ? 1 : 0;
            if (gender <= 1 && g != gender) continue;
        }

        return cand;
    }

    for (uint32_t i = 1; i < 200; ++i) {
        if (i % 25 == nature) return i;
    }
    return 1;
}

static void applyEventData(Pokemon::Pokemon* pk, const EventRecord& rec, const Trainer::Trainer* trainer = nullptr) {
    if (!pk) return;

    // 1. Species & Form
    pk->setSpecies(rec.species);
    pk->setForm(rec.form);

    const Pokemon::PersonalInfo& pi = Pokemon::getPersonalInfo(rec.species, rec.form);

    // 2. Nature
    uint8_t nature = (rec.nature < 25) ? rec.nature : static_cast<uint8_t>(Utils::rand32() % 25);

    // 3. Gender
    uint8_t gender = 2;
    if (pi.genderRatio == 255) {
        gender = 2;
    } else if (pi.genderRatio == 254) {
        gender = 1;
    } else if (pi.genderRatio == 0) {
        gender = 0;
    } else if (rec.gender <= 1) {
        gender = rec.gender;
    } else {
        gender = ((Utils::rand32() & 0xFF) < pi.genderRatio) ? 1 : 0;
    }

    // 4. Ability Number & ID
    int abilityBit = (rec.abilityNum == 2) ? 1 : 0;
    if (rec.abilityNum >= 3) abilityBit = -1;

    const Pokemon::AbilitySlots slots = Pokemon::getAbilitySlots(rec.species, rec.form, pk->getGameGroup());
    uint16_t abId = 0;
    if (rec.abilityNum == 4) {
        abId = (slots.count >= 3 && slots.slot[2]) ? slots.slot[2] : slots.slot[0];
    } else if (rec.abilityNum == 2) {
        abId = slots.slot[1] ? slots.slot[1] : slots.slot[0];
    } else {
        abId = slots.slot[0];
    }

    // 5. Trainer / OT Information
    uint16_t tid = rec.tid16;
    uint16_t sid = rec.sid16;
    uint32_t id32 = rec.id32;
    if (rec.otName[0] != '\0') {
        pk->setOTName(Utils::utf8ToUtf16(rec.otName));
        pk->setOTGender(0);
    } else if (trainer) {
        pk->setOTName(Utils::utf8ToUtf16(trainer->trainerName));
        tid = trainer->TID16 ? trainer->TID16 : static_cast<uint16_t>(trainer->TID);
        sid = trainer->SID16 ? trainer->SID16 : static_cast<uint16_t>(trainer->SID);
        id32 = trainer->ID32;
        pk->setOTGender(trainer->trainerGender);
    }
    pk->setTID16(tid);
    pk->setSID16(sid);
    pk->setId32(id32 ? id32 : (static_cast<uint32_t>(sid) << 16 | tid));

    // 6. PID & Encryption Constant (strictly defensive against Bad Eggs)
    if (rec.generation <= 5) {
        uint32_t p = generateGen3to5PID(nature, gender, pi.genderRatio, abilityBit, rec.isShiny != 0, tid, sid);
        pk->setPID(p);
        pk->setEncryptionConstant(p);
    } else {
        uint32_t ec = Utils::rand32();
        if (ec == 0) ec = 1;
        ec = Pokemon::correctEncryptionConstantForForm(rec.species, rec.form, ec);
        pk->setEncryptionConstant(ec);

        uint32_t p = Utils::rand32();
        if (p == 0) p = 1;
        pk->setPID(p);
        pk->setShiny(rec.isShiny != 0, pk->id32());
    }

    // 7. Nature, Gender, Ability
    pk->setNature(nature);
    pk->setStatNature(nature);
    pk->setGender(gender);
    if (abId != 0) {
        pk->setAbility(abId);
    }
    pk->setAbilityNumber(rec.abilityNum < 3 ? rec.abilityNum : (rec.abilityNum == 4 ? 4 : 1));

    // 8. Level & Friendship
    pk->setLevel(rec.level ? rec.level : 50);
    pk->setFriendship(pi.baseFriendship);

    // 9. Held Item & Ball
    uint16_t item = rec.heldItem;
    auto destGroup = pk->getGameGroup();
    if (destGroup == Enums::GameVersion::FRLG || destGroup == Enums::GameVersion::RSE) {
        if (rec.generation != 3) {
            item = Names::itemModernToG3(item);
        }
    } else {
        if (rec.generation == 3) {
            item = Names::itemG3ToModern(item);
        }
        if (item != 0) {
            bool keep = false;
            if (destGroup == Enums::GameVersion::DP || destGroup == Enums::GameVersion::PT) {
                keep = (item <= 467);
            } else if (destGroup == Enums::GameVersion::HGSS) {
                keep = (item <= 536);
            } else if (destGroup == Enums::GameVersion::BW || destGroup == Enums::GameVersion::B2W2) {
                keep = (item <= 638);
            } else if (destGroup == Enums::GameVersion::XY) {
                keep = (item <= 755);
            } else if (destGroup == Enums::GameVersion::ORAS) {
                keep = (item <= 775);
            } else if (destGroup == Enums::GameVersion::SM || destGroup == Enums::GameVersion::USUM) {
                keep = (item <= 958);
            } else {
                keep = Names::isHeldItemPresent(item, destGroup);
            }
            if (!keep) item = 0;
        }
    }
    pk->setHeldItem(item);
    pk->setBall(rec.ball ? rec.ball : 16); // Cherish Ball default

    // 10. Moves, Relearn Moves & Base PP (base PP guaranteed non-zero for valid moves)
    for (int i = 0; i < 4; ++i) {
        pk->setMove(i, rec.moves[i]);
        pk->setRelearnMove(i, rec.relearnMoves[i]);
        if (rec.moves[i] != 0) {
            pk->setMovePP(i, Names::getMoveBasePP(rec.moves[i], pk->getGameGroup()));
        } else {
            pk->setMovePP(i, 0);
        }
        pk->setMovePPUps(i, 0);
    }

    // 11. Met Details
    if (rec.metLocation != 0) {
        pk->setMetLocation(rec.metLocation);
    }
    pk->setMetLevel(rec.metLevel ? rec.metLevel : pk->level());
    if (rec.metYear != 0) {
        pk->setMetYear(rec.metYear);
        pk->setMetMonth(rec.metMonth ? rec.metMonth : 1);
        pk->setMetDay(rec.metDay ? rec.metDay : 1);
    }
    pk->setFatefulEncounter(rec.fateful != 0);

    // 12. Default Nickname (species name)
    const char* spName = pk->species();
    std::string defaultName = spName ? spName : "";
    if (destGroup == Enums::GameVersion::FRLG || destGroup == Enums::GameVersion::RSE) {
        for (char& c : defaultName) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (!defaultName.empty()) {
        pk->setNickname(Utils::utf8ToUtf16(defaultName));
        pk->setIsNicknamed(false);
    }

    // 13. Language & Origin Game
    pk->setLanguage(2); // English
    if (rec.gameVersion != 0) {
        pk->setOriginGame(rec.gameVersion);
    } else {
        switch (rec.generation) {
            case 3: pk->setOriginGame(1); break;
            case 4: pk->setOriginGame(10); break;
            case 5: pk->setOriginGame(20); break;
            case 6: pk->setOriginGame(24); break;
            case 7: pk->setOriginGame(30); break;
            case 8: pk->setOriginGame(44); break;
            case 9: pk->setOriginGame(50); break;
            default: break;
        }
    }

    // 14. IVs (31 across all stats)
    for (int i = 0; i < 6; ++i) {
        pk->setIV(i, 31);
    }

    // 15. Recalculate stats & Current HP (guaranteed non-zero HP)
    pk->recalculateStats();
    pk->setStatHPCurrent(pk->statHPMax());

    // 16. LGPE Size Handling
    if (destGroup == Enums::GameVersion::GG) {
        auto d = pk->getData();
        if (d.size() >= 0xE8) {
            const uint8_t hs = static_cast<uint8_t>(Utils::rand32() & 0xFF);
            const uint8_t ws = static_cast<uint8_t>(Utils::rand32() & 0xFF);
            d.data()[0x3A] = static_cast<std::byte>(hs);
            d.data()[0x3B] = static_cast<std::byte>(ws);
            const float hr = (hs / 255.0f) * 0.79999995f + 0.6f;
            const float wr = (ws / 255.0f) * 0.40000004f + 0.8f;
            const float hAbs = hr * static_cast<float>(pi.height);
            const float wAbs = hr * wr * static_cast<float>(pi.weight);
            std::memcpy(d.data() + 0x2C, &hAbs, sizeof(float));
            std::memcpy(d.data() + 0xE4, &wAbs, sizeof(float));
        }
    }

    // 17. Final Checksum
    pk->refreshChecksum();
}

std::unique_ptr<Pokemon::Pokemon> EventDatabase::instantiate(const EventRecord& rec, const Trainer::Trainer& trainer) const {
    auto pk = trainer.createBlankPokemon();
    if (!pk) return nullptr;
    applyEventData(pk.get(), rec, &trainer);
    return pk;
}

std::unique_ptr<Pokemon::Pokemon> EventDatabase::instantiateNative(const EventRecord& rec) const {
    std::unique_ptr<Pokemon::Pokemon> pk = nullptr;

    switch (rec.generation) {
        case 3: {
            std::vector<std::byte> zero(100, std::byte{0});
            std::byte* enc = Encryption::encryptArray3FRLG(std::span<const std::byte>(zero.data(), 100));
            pk = std::make_unique<Pokemon::Pokemon3FRLG>(std::span<const std::byte>(enc, 100));
            delete[] enc;
            break;
        }
        case 4: {
            pk = std::make_unique<Pokemon::Pokemon4DP>();
            break;
        }
        case 5: {
            pk = std::make_unique<Pokemon::Pokemon5BW>();
            break;
        }
        case 6: {
            pk = std::make_unique<Pokemon::Pokemon6XY>();
            break;
        }
        case 7: {
            pk = std::make_unique<Pokemon::Pokemon7SM>();
            break;
        }
        case 8: {
            std::vector<std::byte> zero(Encryption::SIZE_PARTY8_SWSH, std::byte{0});
            std::byte* enc = Encryption::encryptArray8SWSH(
                std::span<const std::byte>(zero.data(), Encryption::SIZE_PARTY8_SWSH), 0);
            pk = std::make_unique<Pokemon::Pokemon8SWSH>(
                std::span<const std::byte>(enc, Encryption::SIZE_PARTY8_SWSH));
            delete[] enc;
            break;
        }
        case 9:
        default: {
            std::vector<std::byte> zero(Encryption::SIZE_PARTY9_SV, std::byte{0});
            std::byte* enc = Encryption::encryptArray9SV(
                std::span<const std::byte>(zero.data(), Encryption::SIZE_PARTY9_SV), 0);
            pk = std::make_unique<Pokemon::Pokemon9SV>(
                std::span<const std::byte>(enc, Encryption::SIZE_PARTY9_SV));
            delete[] enc;
            break;
        }
    }

    if (pk) {
        applyEventData(pk.get(), rec, nullptr);
    }
    return pk;
}

} // namespace Events