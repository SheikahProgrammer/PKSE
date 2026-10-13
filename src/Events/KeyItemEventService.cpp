#include "Events/KeyItemEventService.h"

#include <algorithm>
#include <functional>

#include "Trainer/Trainer2GSC.h"
#include "Trainer/Trainer3RSE.h"
#include "Trainer/Trainer3FRLG.h"
#include "Trainer/Trainer4DP.h"
#include "Trainer/Trainer4PT.h"
#include "Trainer/Trainer4HGSS.h"
#include "Trainer/Trainer5BW.h"
#include "Trainer/Trainer6ORAS.h"
#include "Trainer/Inventory3RSE.h"
#include "Trainer/Inventory3FRLG.h"
#include "Trainer/Inventory4DP.h"
#include "Trainer/Inventory4PT.h"
#include "Trainer/Inventory4HGSS.h"
#include "Trainer/Inventory5BW.h"
#include "Trainer/Inventory6ORAS.h"
#include "Names/ItemNames.h"

// Port of PKForge.Engine/KeyItemEventService.cs. Every flag, var and item id below is copied from
// there; the comments name the same sources (PKHeX event labels, pret disassemblies) so either file
// can be checked against the other.

namespace Events {

namespace {

inline uint16_t readU16LE(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline void writeU16LE(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}
inline uint32_t readU32LE(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline void writeU32LE(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// ── Save access ────────────────────────────────────────────────────────────────────────────────
// One flag/var view per supported save, so the definitions below stay data-only like PKForge's.

struct SaveView
{
    std::function<bool(int)> getFlag;
    std::function<void(int, bool)> setFlag;
    std::function<uint16_t(int)> getWork;    // empty when the game has no 16-bit vars here
    std::function<void(int, uint16_t)> setWork;
    size_t keyPouch = 1;
    size_t keyPouchCapacity = 0;
};

bool flagIn(const uint8_t *base, size_t size, int flag)
{
    const size_t byte = static_cast<size_t>(flag) >> 3;
    return byte < size && (base[byte] & (1 << (flag & 7))) != 0;
}

void setFlagIn(uint8_t *base, size_t size, int flag, bool on)
{
    const size_t byte = static_cast<size_t>(flag) >> 3;
    if (byte >= size)
        return;
    if (on)
        base[byte] = static_cast<uint8_t>(base[byte] | (1 << (flag & 7)));
    else
        base[byte] = static_cast<uint8_t>(base[byte] & ~(1 << (flag & 7)));
}

// Gen 3: flags and vars live in the rotated SaveBlock1 ("large" block).
template <typename T> SaveView gen3View(T &t, size_t flagBase, size_t workBase, size_t capacity)
{
    SaveView v;
    v.getFlag = [&t, flagBase](int flag)
    {
        uint8_t b = 0;
        t.readBlock(T::LARGE_ID, flagBase + (static_cast<size_t>(flag) >> 3), &b, 1);
        return (b & (1 << (flag & 7))) != 0;
    };
    v.setFlag = [&t, flagBase](int flag, bool on)
    {
        uint8_t b = 0;
        const size_t ofs = flagBase + (static_cast<size_t>(flag) >> 3);
        t.readBlock(T::LARGE_ID, ofs, &b, 1);
        b = on ? static_cast<uint8_t>(b | (1 << (flag & 7))) : static_cast<uint8_t>(b & ~(1 << (flag & 7)));
        t.writeBlock(T::LARGE_ID, ofs, &b, 1);
    };
    v.getWork = [&t, workBase](int index)
    {
        uint8_t buf[2] = {};
        t.readBlock(T::LARGE_ID, workBase + static_cast<size_t>(index) * 2, buf, 2);
        return readU16LE(buf);
    };
    v.setWork = [&t, workBase](int index, uint16_t value)
    {
        uint8_t buf[2];
        writeU16LE(buf, value);
        t.writeBlock(T::LARGE_ID, workBase + static_cast<size_t>(index) * 2, buf, 2);
    };
    v.keyPouchCapacity = capacity;
    return v;
}

// Gen 4: flags/vars in the General block (PKHeX SAV4DP/Pt/HGSS EventFlag / EventWork).
template <typename T> SaveView gen4View(T &t, size_t flagBase, size_t workBase, size_t capacity)
{
    SaveView v;
    v.getFlag = [&t, flagBase](int flag) { return flagIn(t.getGeneralBlock() + flagBase, 0x10000, flag); };
    v.setFlag = [&t, flagBase](int flag, bool on) { setFlagIn(t.getGeneralBlock() + flagBase, 0x10000, flag, on); };
    v.getWork = [&t, workBase](int index) { return readU16LE(t.getGeneralBlock() + workBase + index * 2); };
    v.setWork = [&t, workBase](int index, uint16_t value)
    { writeU16LE(t.getGeneralBlock() + workBase + index * 2, value); };
    v.keyPouchCapacity = capacity;
    return v;
}

// Gen 5/6: an EventWork block of u16 vars followed by the flag bitfield (PKHeX EventWork5BW / EventWork6).
template <typename T> SaveView eventWorkView(T &t, size_t blockOfs, size_t workCount, size_t flagBytes, size_t capacity)
{
    SaveView v;
    const size_t flagOfs = blockOfs + workCount * 2;
    v.getFlag = [&t, flagOfs, flagBytes](int flag)
    {
        const auto &d = t.getSaveData();
        return d.size() >= flagOfs + flagBytes && flagIn(d.data() + flagOfs, flagBytes, flag);
    };
    v.setFlag = [&t, flagOfs, flagBytes](int flag, bool on)
    {
        auto &d = t.getSaveDataMutable();
        if (d.size() >= flagOfs + flagBytes)
            setFlagIn(d.data() + flagOfs, flagBytes, flag, on);
    };
    v.getWork = [&t, blockOfs, workCount](int index)
    {
        const auto &d = t.getSaveData();
        if (static_cast<size_t>(index) >= workCount || d.size() < blockOfs + workCount * 2)
            return uint16_t(0);
        return readU16LE(d.data() + blockOfs + index * 2);
    };
    v.setWork = [&t, blockOfs, workCount](int index, uint16_t value)
    {
        auto &d = t.getSaveDataMutable();
        if (static_cast<size_t>(index) < workCount && d.size() >= blockOfs + workCount * 2)
            writeU16LE(d.data() + blockOfs + index * 2, value);
    };
    v.keyPouchCapacity = capacity;
    return v;
}

// ── Definitions ────────────────────────────────────────────────────────────────────────────────

// A Gen 4 var the distribution sets to a value; onlyIfZero mirrors pokeplatinum's
// "if (state == 0) set(1)" guard so a later story state is never rewound.
struct WorkValue
{
    int index;
    uint16_t value;
    bool onlyIfZero = false;
};

// Distribution state that is not a plain flag/var (a checked magic value, a raw SRAM byte).
struct ExtraState
{
    std::function<bool(Trainer::Trainer &)> get;
    std::function<void(Trainer::Trainer &, bool)> set;
};

struct Definition
{
    std::string id, title, destination;
    uint16_t item = 0; // 0 = no item (Crystal GS Ball)
    std::vector<int> enableFlags;
    std::vector<WorkValue> enableWork;
    int usedFlag = -1;
    std::vector<int> completionFlags;
    std::function<std::string(const SaveView &)> prerequisite;
    ExtraState extra;
    std::function<bool(const SaveView &)> completedWhen;
};

std::function<std::string(const SaveView &)> gameClear(int flag)
{
    return [flag](const SaveView &v)
    { return v.getFlag(flag) ? std::string() : std::string("Needs the Hall of Fame cleared before the ferry sails."); };
}

// VermilionCity/scripts.inc only offers the Seagallop when VAR_MAP_SCENE_VERMILION_CITY (0x407E) == 3.
std::string vermilionSeagallop(const SaveView &v)
{
    return v.getWork(0x7E) >= 3 ? std::string() : std::string("The Seagallop only runs after the S.S. Anne has left Vermilion.");
}

// pokeruby: FLAG_SYS_GAME_CLEAR = SYSTEM_FLAGS(0x800) + 0x04; FLAG_SYS_HAS_EON_TICKET = SYSTEM_FLAGS + 0x53.
// PKHeX flags_rs: 0x804 "Entered Hall of Fame", 0x853 "Received Eon Ticket", 0xCE "Captured/Defeated Lati@s".
std::vector<Definition> rubySapphire()
{
    return {{"rs-eon", "Eon Ticket", "Southern Island", 275, {0x853}, {}, -1, {0xCE}, gameClear(0x804)}};
}

// pokeemerald flags.h (SYSTEM_FLAGS = 0x860): FLAG_SYS_GAME_CLEAR 0x864, FLAG_ENABLE_SHIP_SOUTHERN_ISLAND 0x8B3,
// _BIRTH_ISLAND 0x8D5, _FARAWAY_ISLAND 0x8D6, _NAVEL_ROCK 0x8E0, FLAG_RECEIVED_AURORA_TICKET 0x13A,
// _MYSTIC_TICKET 0x13B, _OLD_SEA_MAP 0x13C, FLAG_SHOWN_EON_TICKET 0x1AE, _AURORA 0x1AF, _OLD_SEA_MAP 0x1B0,
// _MYSTIC 0x1DB. The gift_*.inc scripts do "giveitem X; setflag FLAG_ENABLE_SHIP_*; setflag FLAG_RECEIVED_*".
// The Eon Ticket has no Emerald gift script: record_mixing.c sets only FLAG_ENABLE_SHIP_SOUTHERN_ISLAND.
// Gen 3 item ids: Eon Ticket 0x113, Mystic Ticket 0x172, Aurora Ticket 0x173, Old Sea Map 0x178.
std::vector<Definition> emerald()
{
    return {
        {"e-eon", "Eon Ticket", "Southern Island", 275, {0x8B3}, {}, 0x1AE, {0x1C8, 0x1C9}, gameClear(0x864)},
        {"e-aurora", "Aurora Ticket", "Birth Island", 371, {0x8D5, 0x13A}, {}, 0x1AF, {0x1AC, 0x1AD}, gameClear(0x864)},
        {"e-oldseamap", "Old Sea Map", "Faraway Island", 376, {0x8D6, 0x13C}, {}, 0x1B0, {0x1C7, 0x1CA}, gameClear(0x864)},
        {"e-mystic", "Mystic Ticket", "Navel Rock", 370, {0x8E0, 0x13B}, {}, 0x1DB, {0x91, 0x92, 0x1DC, 0x1DD},
         gameClear(0x864)},
    };
}

// pokefirered flags.h: FLAG_RECEIVED_AURORA_TICKET 0x2A7, _MYSTIC_TICKET 0x2A8, FLAG_ENABLE_SHIP_NAVEL_ROCK 0x84A,
// _BIRTH_ISLAND 0x84B, FLAG_FOUGHT_DEOXYS 0x2E4, FLAG_SHOWN_MYSTIC_TICKET 0x2F0, FLAG_SHOWN_AURORA_TICKET 0x2F1.
// PKHeX flags_frlg: 754/755 "Captured Lugia/Ho-Oh", 757/758/759 "Defeated Lugia/Ho-Oh/Deoxys".
std::vector<Definition> fireRedLeafGreen()
{
    return {
        {"frlg-aurora", "Aurora Ticket", "Birth Island", 371, {0x84B, 0x2A7}, {}, 0x2F1, {0x2E4, 759}, vermilionSeagallop},
        {"frlg-mystic", "Mystic Ticket", "Navel Rock", 370, {0x84A, 0x2A8}, {}, 0x2F0, {754, 755, 757, 758},
         vermilionSeagallop},
    };
}

// pokeplatinum system_vars.c sDistributionEventMagicNumbers: DARKRAI 0x1209, SHAYMIN 0x1112, ARCEUS 0x1123,
// ROTOM 0x1103, at work 67/68/69/70 (PKHeX const_pt/const_dp). InitShayminEvent also sets
// VAR_SHAYMIN_EVENT_STATE (work 87) to 1 when it is 0. Completion (PKHeX flags_dp/flags_pt): 344 Darkrai
// captured, 291 Shaymin captured, 286 Arceus captured, 710 Rotom's Room unlocked (Pt).
std::vector<Definition> platinum()
{
    return {
        {"pt-member", "Member Card", "Newmoon Island (Darkrai)", 454, {}, {{67, 0x1209}}, -1, {344}},
        {"pt-oak", "Oak's Letter", "Flower Paradise (Shaymin)", 452, {}, {{68, 0x1112}, {87, 1, true}}, -1, {291}},
        {"pt-azure", "Azure Flute", "Hall of Origin (Arceus)", 455, {}, {{69, 0x1123}}, -1, {286}},
        {"pt-secret", "Secret Key", "Rotom's Room", 467, {}, {{70, 0x1103}}, -1, {710}},
    };
}

// Diamond/Pearl share the magic vars 67-69 (PKHeX const_dp). The DP Shaymin state var is not mapped
// by PKHeX or a disassembly, so only the magic var is written there.
std::vector<Definition> diamondPearl()
{
    return {
        {"dp-member", "Member Card", "Newmoon Island (Darkrai)", 454, {}, {{67, 0x1209}}, -1, {344}},
        {"dp-oak", "Oak's Letter", "Flower Paradise (Shaymin)", 452, {}, {{68, 0x1112}}, -1, {291}},
        {"dp-azure", "Azure Flute", "Hall of Origin (Arceus)", 455, {}, {{69, 0x1123}}, -1, {286}},
    };
}

// HGSS: PKHeX const_hgss "0067 Enigma Stone 0:Not Activated,1778:Activated". Completion: flags_hgss
// 0781 "Lati@s (Pewter City) Disappeared".
std::vector<Definition> heartGoldSoulSilver()
{
    return {{"hgss-enigma", "Enigma Stone", "Pewter City (Latias/Latios)", 536, {}, {{67, 1778}}, -1, {781}}};
}

// BW: PKHeX Misc5BW.LibertyTicketState (Misc block 0x21200 + 0xBC) must equal 2010_04_06 XOR ID32.
// Completion: PKHeX const_bw work 145 "Victini" 3:Defeated, 4:Captured, 5:Event Completed, 6:Disappeared.
constexpr size_t BW_LIBERTY_OFS = 0x21200 + 0xBC;
constexpr uint32_t LIBERTY_MAGIC = 20100406;

std::vector<Definition> blackWhite()
{
    Definition d{"bw-liberty", "Liberty Pass", "Liberty Garden (Victini)", 574, {}, {}, -1, {}};
    d.extra.get = [](Trainer::Trainer &t)
    {
        const auto &data = static_cast<Trainer::Trainer5BW &>(t).getSaveData();
        return data.size() >= BW_LIBERTY_OFS + 4 && readU32LE(data.data() + BW_LIBERTY_OFS) == (LIBERTY_MAGIC ^ t.ID32);
    };
    d.extra.set = [](Trainer::Trainer &t, bool on)
    {
        auto &data = static_cast<Trainer::Trainer5BW &>(t).getSaveDataMutable();
        if (data.size() >= BW_LIBERTY_OFS + 4)
            writeU32LE(data.data() + BW_LIBERTY_OFS, on ? (LIBERTY_MAGIC ^ t.ID32) : 0u);
    };
    d.completedWhen = [](const SaveView &v) { return v.getWork(145) >= 3; };
    return {d};
}

// ORAS: PKHeX flags_oras 3010 "Received Eon Ticket", 3011 "Eon Ticket Event Completed".
std::vector<Definition> omegaRubyAlphaSapphire()
{
    return {{"oras-eon", "Eon Ticket", "Southern Island", 726, {3010}, {}, -1, {3011}}};
}

// Crystal: PKHeX SAV2.EnableGSBallMobileEvent writes GS_BALL_AVAILABLE (0x0B) to the Mobile event byte
// (0x3E3C + backup 0x3E44 international, 0xA000 + 0xA083 Japanese). No item is written: the Goldenrod
// Pokémon Center hands the GS Ball over in game, which sets flags_c 0832 "Received GS Ball" (shown).
constexpr uint8_t GS_BALL_AVAILABLE = 0x0B;

std::vector<Definition> crystal()
{
    Definition d{"c-gsball", "GS Ball", "Ilex Forest shrine (Celebi)", 0, {}, {}, 832, {}};
    d.extra.get = [](Trainer::Trainer &t)
    {
        auto &gsc = static_cast<Trainer::Trainer2GSC &>(t);
        const size_t primary = gsc.layout() == Trainer::Gen2Layout::C_JP ? 0xA000 : 0x3E3C;
        const auto &data = gsc.getSaveData();
        return data.size() > primary && data[primary] == GS_BALL_AVAILABLE;
    };
    d.extra.set = [](Trainer::Trainer &t, bool on)
    {
        auto &gsc = static_cast<Trainer::Trainer2GSC &>(t);
        const bool jp = gsc.layout() == Trainer::Gen2Layout::C_JP;
        auto &data = gsc.getSaveDataMutable();
        for (size_t ofs : {jp ? size_t(0xA000) : size_t(0x3E3C), jp ? size_t(0xA083) : size_t(0x3E44)})
        {
            if (ofs >= data.size())
                continue;
            if (on)
                data[ofs] = GS_BALL_AVAILABLE;
            // Reverse only while the byte still holds what we wrote: past that it is the game's progress.
            else if (data[ofs] == GS_BALL_AVAILABLE)
                data[ofs] = 0;
        }
    };
    return {d};
}

bool isCrystal(const Trainer::Trainer &trainer)
{
    if (trainer.getGameGroup() != GameVersion::GSC)
        return false;
    const auto layout = static_cast<const Trainer::Trainer2GSC &>(trainer).layout();
    return layout == Trainer::Gen2Layout::C_INTL || layout == Trainer::Gen2Layout::C_JP;
}

struct Session
{
    std::vector<Definition> definitions;
    SaveView view;
};

Session sessionFor(Trainer::Trainer &trainer)
{
    Session s;
    switch (trainer.getGameGroup())
    {
    case GameVersion::RSE:
    {
        auto &rse = static_cast<Trainer::Trainer3RSE &>(trainer);
        const bool em = rse.layout() == Trainer::Gen3HoennLayout::Emerald;
        const size_t cap = Trainer::getPouchInfo3RSE(Trainer::PouchType3RSE::KeyItems, rse.layout()).maxSlots;
        // PKHeX SAV3E EventFlag 0x1270 / EventWork 0x139C; SAV3RS 0x1220 / 0x1340.
        s.view = em ? gen3View(rse, 0x1270, 0x139C, cap) : gen3View(rse, 0x1220, 0x1340, cap);
        s.definitions = em ? emerald() : rubySapphire();
        break;
    }
    case GameVersion::FRLG:
    {
        auto &frlg = static_cast<Trainer::Trainer3FRLG &>(trainer);
        // PKHeX SAV3FRLG EventFlag 0xEE0 / EventWork 0x1000.
        s.view = gen3View(frlg, 0xEE0, 0x1000,
                          Trainer::getPouchInfo3FRLG(Trainer::PouchType3FRLG::KeyItems).maxSlots);
        s.definitions = fireRedLeafGreen();
        break;
    }
    case GameVersion::DP:
        s.view = gen4View(static_cast<Trainer::Trainer4DP &>(trainer), 0xFDC, 0xD9C, Trainer::POUCHES4_DP[1].slots);
        s.definitions = diamondPearl();
        break;
    case GameVersion::PT:
        s.view = gen4View(static_cast<Trainer::Trainer4PT &>(trainer), 0xFEC, 0xDAC, Trainer::POUCHES4_PT[1].slots);
        s.definitions = platinum();
        break;
    case GameVersion::HGSS:
        s.view = gen4View(static_cast<Trainer::Trainer4HGSS &>(trainer), 0x10C4, 0xDE4,
                          Trainer::POUCHES4_HGSS[1].slots);
        s.definitions = heartGoldSoulSilver();
        break;
    case GameVersion::BW:
        // SaveBlockAccessor5BW block 45 EventWork at 0x20100: 0x13E vars, then 0xB60 flags.
        s.view = eventWorkView(static_cast<Trainer::Trainer5BW &>(trainer), 0x20100, 0x13E, 0xB60 / 8,
                               Trainer::POUCHES5_BW[1].slots);
        s.definitions = blackWhite();
        break;
    case GameVersion::ORAS:
        // SaveBlockAccessor6AO block 19 EventWork at 0x14A00: 0x178 vars (0x2F0 bytes), then 0xD00 flags.
        s.view = eventWorkView(static_cast<Trainer::Trainer6ORAS &>(trainer), 0x14A00, 0x178, 0xD00 / 8,
                               Trainer::POUCHES6_ORAS[1].slots);
        s.definitions = omegaRubyAlphaSapphire();
        break;
    case GameVersion::GSC:
    {
        if (!isCrystal(trainer))
            break;
        auto &gsc = static_cast<Trainer::Trainer2GSC &>(trainer);
        // PKHeX SAV2Offsets: Crystal EventFlag = CurrentBoxIndex - 0x100 (2000 flags).
        const size_t flagBase = gsc.getOffsets()->currentBoxIndex - 0x100;
        s.view.getFlag = [&gsc, flagBase](int flag)
        {
            const auto &d = gsc.getSaveData();
            return d.size() >= flagBase + 250 && flagIn(d.data() + flagBase, 250, flag);
        };
        s.view.setFlag = [&gsc, flagBase](int flag, bool on)
        {
            auto &d = gsc.getSaveDataMutable();
            if (d.size() >= flagBase + 250)
                setFlagIn(d.data() + flagBase, 250, flag, on);
        };
        s.view.keyPouch = 2;
        s.definitions = crystal();
        break;
    }
    default:
        break;
    }
    return s;
}

bool hasItem(const Trainer::Trainer &trainer, uint16_t itemId)
{
    for (const auto &pouch : trainer.items)
        for (const auto &it : pouch)
            if (it.itemId == itemId && it.count > 0)
                return true;
    return false;
}

std::string itemNameFor(const Trainer::Trainer &trainer, const Definition &d)
{
    if (d.item == 0)
        return d.title;
    const char *name = Names::getItemNameFor(trainer.getGameGroup(), d.item);
    return (name && *name) ? std::string(name) : d.title;
}

const Definition *find(const Session &s, const std::string &id)
{
    for (const auto &d : s.definitions)
        if (d.id == id)
            return &d;
    return nullptr;
}

KeyItemEventStatus read(Trainer::Trainer &trainer, const Session &s, const Definition &d)
{
    const SaveView &v = s.view;
    KeyItemEventStatus st;
    st.id = d.id;
    st.title = d.title;
    st.destination = d.destination;
    st.itemId = d.item;
    st.itemName = itemNameFor(trainer, d);
    st.needsItem = d.item != 0;
    st.hasItem = st.needsItem && hasItem(trainer, d.item);

    const int extra = d.extra.get ? (d.extra.get(trainer) ? 1 : 0) : -1; // -1: no extra state
    // "onlyIfZero" state vars advance with the story, so they are not part of the armed check.
    bool armed = extra != 0, anyArmed = extra == 1;
    for (int f : d.enableFlags)
    {
        const bool on = v.getFlag(f);
        armed = armed && on;
        anyArmed = anyArmed || on;
    }
    for (const auto &w : d.enableWork)
    {
        if (w.onlyIfZero)
            continue;
        const bool on = v.getWork(w.index) == w.value;
        armed = armed && on;
        anyArmed = anyArmed || on;
    }
    st.eventArmed = armed;
    st.used = d.usedFlag >= 0 && v.getFlag(d.usedFlag);
    st.completed = std::any_of(d.completionFlags.begin(), d.completionFlags.end(), [&](int f) { return v.getFlag(f); }) ||
                   (d.completedWhen && d.completedWhen(v));

    st.state = st.completed                              ? KeyItemEventState::Completed
               : st.used                                 ? KeyItemEventState::Used
               : (st.hasItem || !st.needsItem) && armed ? KeyItemEventState::Enabled
               : st.hasItem || anyArmed                  ? KeyItemEventState::Partial
                                                         : KeyItemEventState::NotEnabled;
    if (d.prerequisite)
        st.prerequisite = d.prerequisite(v);
    return st;
}

} // namespace

bool KeyItemEventService::isSupported(const Trainer::Trainer &trainer)
{
    switch (trainer.getGameGroup())
    {
    case GameVersion::RSE:
    case GameVersion::FRLG:
    case GameVersion::DP:
    case GameVersion::PT:
    case GameVersion::HGSS:
    case GameVersion::BW:
    case GameVersion::ORAS:
        return true;
    case GameVersion::GSC:
        return isCrystal(trainer);
    default:
        return false;
    }
}

std::vector<KeyItemEventStatus> KeyItemEventService::getEvents(Trainer::Trainer &trainer)
{
    std::vector<KeyItemEventStatus> result;
    if (!isSupported(trainer))
        return result;
    const Session s = sessionFor(trainer);
    for (const auto &d : s.definitions)
        result.push_back(read(trainer, s, d));
    return result;
}

bool KeyItemEventService::enableEvent(Trainer::Trainer &trainer, const std::string &eventId, std::string &message)
{
    if (!isSupported(trainer))
    {
        message = "That event is not available for this game.";
        return false;
    }
    const Session s = sessionFor(trainer);
    const Definition *d = find(s, eventId);
    if (!d)
    {
        message = "That event is not available for this game.";
        return false;
    }

    if (d->item != 0 && !hasItem(trainer, d->item))
    {
        if (s.view.keyPouch >= trainer.items.size() || trainer.items[s.view.keyPouch].size() >= s.view.keyPouchCapacity)
        {
            message = "This save's Key Items pouch cannot hold the " + d->title + ".";
            return false;
        }
        trainer.items[s.view.keyPouch].push_back(Trainer::InventoryItem{d->item, 1, false, false});
        trainer.updateItemBlock();
    }

    for (int f : d->enableFlags)
        s.view.setFlag(f, true);
    for (const auto &w : d->enableWork)
        if (!w.onlyIfZero || s.view.getWork(w.index) == 0)
            s.view.setWork(w.index, w.value);
    if (d->extra.set)
        d->extra.set(trainer, true);

    message = d->title + " event enabled: " + d->destination + ".";
    return true;
}

bool KeyItemEventService::disableEvent(Trainer::Trainer &trainer, const std::string &eventId, std::string &message)
{
    if (!isSupported(trainer))
    {
        message = "That event is not available for this game.";
        return false;
    }
    const Session s = sessionFor(trainer);
    const Definition *d = find(s, eventId);
    if (!d)
    {
        message = "That event is not available for this game.";
        return false;
    }

    if (d->item != 0 && s.view.keyPouch < trainer.items.size())
    {
        auto &pouch = trainer.items[s.view.keyPouch];
        const auto before = pouch.size();
        pouch.erase(std::remove_if(pouch.begin(), pouch.end(),
                                   [d](const Trainer::InventoryItem &it) { return it.itemId == d->item; }),
                    pouch.end());
        if (pouch.size() != before)
            trainer.updateItemBlock();
    }

    for (int f : d->enableFlags)
        s.view.setFlag(f, false);
    for (const auto &w : d->enableWork)
        if (!w.onlyIfZero)
            s.view.setWork(w.index, 0);
    if (d->extra.set)
        d->extra.set(trainer, false);

    message = d->title + " event disabled.";
    return true;
}

} // namespace Events
