#!/usr/bin/env python3
"""gen_events.py - Packs Pokemon event distributions into romfs/events/events.bin for PKSE.

Supports:
- Gen 3 pk3 events (curated iconic distributions)
- Gen 4 PCD/PGT wondercards from PKForge events-g4.bin.gz
- Gen 5 PGF wondercards from PKForge events-g5.bin.gz
- Gen 6-9 wondercards from PKForge events-g6..9.bin.gz

Output:
- romfs/events/events.bin (144-byte EventRecord structs)
"""

import gzip
import os
import re
import struct
import sys

EVENT_RECORD_FORMAT = "<HBBHBBBBBBBBHHHHHHHHHHBBBBHHI24s48sHBBBBBB18s"
RECORD_SIZE = 144
assert struct.calcsize(EVENT_RECORD_FORMAT) == RECORD_SIZE, f"Record size is {struct.calcsize(EVENT_RECORD_FORMAT)}, expected {RECORD_SIZE}"

def clean_title(filename):
    base = filename.split("/")[-1]
    base = re.sub(r"\.(wc\d|wc\dfull|wb\d|wb\dfull|wa\d|pk\d|pcd|pgt|pgf)$", "", base, flags=re.I)
    base = re.sub(r"^([0-9\-]+)\s+([A-Z0-9]+)\s*-\s*", "", base)
    return base.strip()

GEN9_FIRST_UNALIGNED_SPECIES = 917
GEN9_INTERNAL_TO_NATIONAL = [
    65,  -1,  -1,  -1,  -1,  31,  31,  47,  47,  29,  29,  53,  31,
    31,  46,  44,  30,  30,  -7,  -7,  -7,  13,  13,  -2,  -2,  23,
    23,  24, -21, -21,  27,  27,  47,  47,  47,  26,  14, -33, -33,
   -33, -17, -17,   3, -29,  12, -12, -31, -31, -31,   3,   3, -24,
   -24, -44, -44, -30, -30, -28, -28,  23,  23,   6,   7,  29,   8,
     3,   4,   4,  20,   4,  23,   6,   3,   3,   4,  -1,  13,   9,
     7,   5,   7,   9,   9, -43, -43, -43, -68, -68, -68, -58, -58,
   -25, -29, -31,   6,  -1,   6,   0,   0,   0,   3,   3,   4,  -1,
    -1,   0,  -1,  -1,  -1
]

def get_national9(internal):
    if internal < GEN9_FIRST_UNALIGNED_SPECIES:
        return internal
    idx = internal - GEN9_FIRST_UNALIGNED_SPECIES
    if idx < len(GEN9_INTERNAL_TO_NATIONAL):
        return internal + GEN9_INTERNAL_TO_NATIONAL[idx]
    return internal

# Gen 4 PK4 decryption tables
BLOCK_POS = [
    0, 1, 2, 3,  0, 1, 3, 2,  0, 2, 1, 3,  0, 3, 1, 2,
    0, 2, 3, 1,  0, 3, 2, 1,  1, 0, 2, 3,  1, 0, 3, 2,
    2, 0, 1, 3,  3, 0, 1, 2,  2, 0, 3, 1,  3, 0, 2, 1,
    1, 2, 0, 3,  1, 3, 0, 2,  2, 1, 0, 3,  3, 1, 0, 2,
    2, 3, 0, 1,  3, 2, 0, 1,  1, 2, 3, 0,  1, 3, 2, 0,
    2, 1, 3, 0,  3, 1, 2, 0,  2, 3, 1, 0,  3, 2, 1, 0,
    0, 1, 2, 3,  0, 1, 3, 2,  0, 2, 1, 3,  0, 3, 1, 2,
    0, 2, 3, 1,  0, 3, 2, 1,  1, 0, 2, 3,  1, 0, 3, 2,
]

def crypt_array(data, seed):
    res = bytearray(data)
    for i in range(0, len(res), 2):
        seed = (seed * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
        val = res[i] | (res[i+1] << 8)
        val ^= (seed >> 16)
        res[i] = val & 0xFF
        res[i+1] = (val >> 8) & 0xFF
    return res

def unshuffle_45(shuffle_bytes, sv):
    blocks = [shuffle_bytes[i*32:(i+1)*32] for i in range(4)]
    un = [None]*4
    for i in range(4):
        pos = BLOCK_POS[sv*4 + i]
        un[pos] = blocks[i]
    return b''.join(un)

def decrypt_pk4(data):
    pv, = struct.unpack_from('<I', data, 0)
    chk, = struct.unpack_from('<H', data, 6)
    sv = (pv >> 13) & 31
    shuffle = data[8:136]
    decrypted_shuffle = crypt_array(shuffle, chk)
    unshuffled = unshuffle_45(decrypted_shuffle, sv)
    return data[:8] + unshuffled + (data[136:] if len(data) > 136 else b'')

def decode_gen4_text(raw):
    chars = []
    for i in range(0, len(raw), 2):
        val = raw[i] | (raw[i+1] << 8)
        if val == 0xFFFF:
            break
        if 0x00A2 <= val <= 0x00AB:
            chars.append(chr(ord('0') + val - 0x00A2))
        elif 0x00AC <= val <= 0x00C5:
            chars.append(chr(ord('A') + val - 0x00AC))
        elif 0x00C6 <= val <= 0x00DF:
            chars.append(chr(ord('a') + val - 0x00C6))
        elif val == 0x0001:
            chars.append(' ')
        elif val == 0x00E3:
            chars.append('-')
        elif val == 0x00E2:
            chars.append('/')
        elif val < 128:
            chars.append(chr(val))
    return ''.join(chars).strip()

def parse_card(name, data, gen):
    ext = os.path.splitext(name)[1].lower()

    if gen == 4:
        if len(data) < 260:
            return None
        pgt = data[:260]
        card_type, = struct.unpack_from('<H', pgt, 0)
        if card_type not in (1, 2, 7, 13):
            return None
        
        if card_type == 7: # Manaphy Egg
            return {
                'generation': 4,
                'gameVersion': 0,
                'species': 490,
                'form': 0,
                'isShiny': 0,
                'level': 1,
                'gender': 2,
                'ball': 4,
                'nature': 255,
                'abilityNum': 0,
                'heldItem': 0,
                'moves': [391, 352, 250, 151],
                'relearnMoves': [0, 0, 0, 0],
                'metLocation': 30001,
                'metLevel': 1,
                'metYear': 6,
                'metMonth': 1,
                'metDay': 1,
                'tid16': 0,
                'sid16': 0,
                'id32': 0,
                'otName': 'RANGER',
                'cardId': 0,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }

        pk4 = pgt[8:244]
        is_enc = struct.unpack_from('<I', pk4, 0x64)[0] != 0
        if is_enc:
            pk4 = decrypt_pk4(pk4)

        species, held_item, tid, sid = struct.unpack_from('<HHHH', pk4, 8)
        if species == 0 or species > 493:
            return None
        
        moves = list(struct.unpack_from('<HHHH', pk4, 40))
        pid, = struct.unpack_from('<I', pk4, 0)
        is_shiny = 1 if ((tid ^ sid ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8) else 0
        nature = pid % 25
        level = pk4[0x84] & 0x7F
        ball = pk4[0x83] or pk4[0x86] or 16
        loc = struct.unpack_from('<H', pk4, 0x80)[0]
        ot = decode_gen4_text(pk4[104:120])
        fateful = 1 if (pk4[0x40] & 1) else 0

        rib0, = struct.unpack_from('<I', pk4, 0x24)
        rib_classic = 1 if (rib0 & (1 << 16)) else 0
        rib_premier = 1 if (rib0 & (1 << 17)) else 0

        return {
            'generation': 4,
            'gameVersion': 0,
            'species': species,
            'form': (pk4[0x40] >> 3) & 0x1F,
            'isShiny': is_shiny,
            'level': level if level else 50,
            'gender': (pid & 0xFF) % 2,
            'ball': ball if ball else 16,
            'nature': nature if nature < 25 else 255,
            'abilityNum': (pid & 1),
            'heldItem': held_item,
            'moves': moves,
            'relearnMoves': [0, 0, 0, 0],
            'metLocation': loc if loc else 30001,
            'metLevel': level if level else 50,
            'metYear': 8,
            'metMonth': 1,
            'metDay': 1,
            'tid16': tid,
            'sid16': sid,
            'id32': (sid << 16) | tid,
            'otName': ot,
            'cardId': 0,
            'fateful': fateful,
            'ribbonClassic': rib_classic,
            'ribbonEvent': 0,
            'ribbonPremier': rib_premier,
            'ribbonWishing': 0,
            'ribbonSouvenir': 0,
        }

    elif gen == 5:
        if len(data) < 0xCC:
            return None
        species, = struct.unpack_from('<H', data, 0x1A)
        if species == 0 or species > 649:
            return None

        tid, sid = struct.unpack_from('<HH', data, 0x00)
        ball = data[0x0E]
        held_item, = struct.unpack_from('<H', data, 0x10)
        moves = list(struct.unpack_from('<4H', data, 0x12))
        form = data[0x1C]
        nature = data[0x34]
        gender = data[0x35]
        ability_num = data[0x36]
        pid_type = data[0x37]
        pid, = struct.unpack_from('<I', data, 0x08)
        is_shiny = 1 if pid_type == 2 or ((tid ^ sid ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8) else 0
        met_loc, = struct.unpack_from('<H', data, 0x3A)
        met_level = data[0x3C]
        ot = data[0x4A:0x5A].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
        level = data[0x5B]
        
        rib0 = data[0x0C]
        rib1 = data[0x0D]
        rib_classic = 1 if (rib0 & (1 << 4)) else 0
        rib_event = 1 if (rib0 & (1 << 6)) else 0
        rib_premier = 1 if (rib0 & (1 << 5)) else 0
        rib_wishing = 1 if (rib1 & (1 << 2)) else 0
        rib_souvenir = 1 if (rib1 & (1 << 1)) else 0

        return {
            'generation': 5,
            'gameVersion': data[0x04],
            'species': species,
            'form': form,
            'isShiny': is_shiny,
            'level': level if level else 50,
            'gender': gender if gender in (0, 1, 2) else 0,
            'ball': ball if ball else 16,
            'nature': nature if nature < 25 else 255,
            'abilityNum': ability_num if ability_num < 3 else 0,
            'heldItem': held_item,
            'moves': moves,
            'relearnMoves': [0, 0, 0, 0],
            'metLocation': met_loc if met_loc else 30001,
            'metLevel': met_level if met_level else level,
            'metYear': 11,
            'metMonth': 1,
            'metDay': 1,
            'tid16': tid,
            'sid16': sid,
            'id32': (sid << 16) | tid,
            'otName': ot,
            'cardId': 0,
            'fateful': 1,
            'ribbonClassic': rib_classic,
            'ribbonEvent': rib_event,
            'ribbonPremier': rib_premier,
            'ribbonWishing': rib_wishing,
            'ribbonSouvenir': rib_souvenir,
        }

    elif gen in (6, 7):
        if len(data) >= 0x310:
            data = data[0x208:0x208 + 0x108]
        if len(data) < 0x108 or data[0x51] != 0:
            return None
        card_id, = struct.unpack_from('<H', data, 0)
        tid, sid = struct.unpack_from('<HH', data, 0x68)
        origin_game = data[0x6C]
        ball = data[0x76]
        held_item, = struct.unpack_from('<H', data, 0x78)
        moves = list(struct.unpack_from('<4H', data, 0x7A))
        species, = struct.unpack_from('<H', data, 0x82)
        form = data[0x84]
        nature = data[0xA0]
        gender = data[0xA1]
        ability_num = data[0xA2]
        is_shiny = 1 if data[0xA3] in (1, 2) else 0
        egg_loc, met_loc = struct.unpack_from('<HH', data, 0xA4)
        ot = data[0xB6:0xD0].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
        level = data[0xD0]
        relearn = list(struct.unpack_from('<4H', data, 0xD8))

        rib0 = data[0x74]
        rib1 = data[0x75]
        rib_classic = 1 if (rib0 & (1 << 4)) else 0
        rib_event = 1 if (rib0 & (1 << 5)) else 0
        rib_premier = 1 if (rib0 & (1 << 2)) else 0
        rib_wishing = 1 if (rib1 & (1 << 0)) else 0
        rib_souvenir = 1 if (rib1 & (1 << 2)) else 0

        return {
            'generation': gen,
            'gameVersion': origin_game,
            'species': species,
            'form': form,
            'isShiny': is_shiny,
            'level': level,
            'gender': gender,
            'ball': ball if ball else 16,
            'nature': nature if nature < 25 else 255,
            'abilityNum': ability_num if ability_num < 3 else 0,
            'heldItem': held_item,
            'moves': moves,
            'relearnMoves': relearn,
            'metLocation': met_loc if met_loc else (40001 if gen == 6 else 30001),
            'metLevel': level,
            'metYear': 14 if gen == 6 else 17,
            'metMonth': 1,
            'metDay': 1,
            'tid16': tid,
            'sid16': sid,
            'id32': (sid << 16) | tid,
            'otName': ot,
            'cardId': card_id,
            'fateful': 1,
            'ribbonClassic': rib_classic,
            'ribbonEvent': rib_event,
            'ribbonPremier': rib_premier,
            'ribbonWishing': rib_wishing,
            'ribbonSouvenir': rib_souvenir,
        }

    elif gen == 8:
        if ext == '.wc8':
            if len(data) < 0x2D0 or data[0x11] != 1:
                return None
            card_id, = struct.unpack_from('<H', data, 0x08)
            tid, sid = struct.unpack_from('<HH', data, 0x20)
            id32, = struct.unpack_from('<I', data, 0x20)
            origin_game, = struct.unpack_from('<I', data, 0x24)
            egg_loc, loc, ball, held_item = struct.unpack_from('<4H', data, 0x228)
            moves = list(struct.unpack_from('<4H', data, 0x230))
            relearn = list(struct.unpack_from('<4H', data, 0x238))
            species, = struct.unpack_from('<H', data, 0x240)
            form = data[0x242]
            gender = data[0x243]
            level = data[0x244]
            nature = data[0x246]
            ability_num = data[0x247]
            is_shiny = 1 if data[0x248] in (1, 2, 3, 4) else 0
            met_level = data[0x249]
            ot = data[0x146:0x160].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            if not ot:
                ot = data[0x12C:0x146].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            return {
                'generation': 8,
                'gameVersion': origin_game,
                'species': species,
                'form': form,
                'isShiny': is_shiny,
                'level': level,
                'gender': gender,
                'ball': ball if ball else 16,
                'nature': nature if nature < 25 else 255,
                'abilityNum': ability_num if ability_num < 3 else 0,
                'heldItem': held_item,
                'moves': moves,
                'relearnMoves': relearn,
                'metLocation': loc if loc else 30001,
                'metLevel': met_level if met_level else level,
                'metYear': 20,
                'metMonth': 1,
                'metDay': 1,
                'tid16': tid,
                'sid16': sid,
                'id32': id32,
                'otName': ot,
                'cardId': card_id,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }
        elif ext == '.wb8':
            if len(data) < 0x2DC or data[0x11] != 1:
                return None
            card_id, = struct.unpack_from('<H', data, 0x08)
            tid, sid = struct.unpack_from('<HH', data, 0x20)
            id32, = struct.unpack_from('<I', data, 0x20)
            origin_game, = struct.unpack_from('<I', data, 0x24)
            egg_loc, loc, ball, held_item = struct.unpack_from('<4H', data, 0x270)
            moves = list(struct.unpack_from('<4H', data, 0x278))
            relearn = list(struct.unpack_from('<4H', data, 0x280))
            species, = struct.unpack_from('<H', data, 0x288)
            form = data[0x28A]
            gender = data[0x28B]
            level = data[0x28C]
            nature = data[0x28E]
            ability_num = data[0x28F]
            is_shiny = 1 if data[0x290] in (1, 2, 3, 4) else 0
            met_level = data[0x291]
            ot = data[0x146:0x160].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            return {
                'generation': 8,
                'gameVersion': origin_game,
                'species': species,
                'form': form,
                'isShiny': is_shiny,
                'level': level,
                'gender': gender,
                'ball': ball if ball else 16,
                'nature': nature if nature < 25 else 255,
                'abilityNum': ability_num if ability_num < 3 else 0,
                'heldItem': held_item,
                'moves': moves,
                'relearnMoves': relearn,
                'metLocation': loc if loc else 30001,
                'metLevel': met_level if met_level else level,
                'metYear': 21,
                'metMonth': 1,
                'metDay': 1,
                'tid16': tid,
                'sid16': sid,
                'id32': id32,
                'otName': ot,
                'cardId': card_id,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }
        elif ext == '.wa8':
            if len(data) < 0x2B0 or data[0x11] != 1:
                return None
            card_id, = struct.unpack_from('<H', data, 0x08)
            tid, sid = struct.unpack_from('<HH', data, 0x20)
            id32, = struct.unpack_from('<I', data, 0x20)
            origin_game, = struct.unpack_from('<I', data, 0x24)
            egg_loc, loc, ball, held_item = struct.unpack_from('<4H', data, 0x228)
            moves = list(struct.unpack_from('<4H', data, 0x230))
            species, = struct.unpack_from('<H', data, 0x238)
            form = data[0x23A]
            gender = data[0x23B]
            level = data[0x23C]
            nature = data[0x23E]
            ability_num = data[0x23F]
            is_shiny = 1 if data[0x240] in (1, 2, 3, 4) else 0
            met_level = data[0x241]
            ot = data[0x146:0x160].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            return {
                'generation': 8,
                'gameVersion': origin_game,
                'species': species,
                'form': form,
                'isShiny': is_shiny,
                'level': level,
                'gender': gender,
                'ball': ball if ball else 16,
                'nature': nature if nature < 25 else 255,
                'abilityNum': ability_num if ability_num < 3 else 0,
                'heldItem': held_item,
                'moves': moves,
                'relearnMoves': [0, 0, 0, 0],
                'metLocation': loc if loc else 30001,
                'metLevel': met_level if met_level else level,
                'metYear': 22,
                'metMonth': 1,
                'metDay': 1,
                'tid16': tid,
                'sid16': sid,
                'id32': id32,
                'otName': ot,
                'cardId': card_id,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }

    elif gen == 9:
        if ext == '.wc9':
            if len(data) < 0x280 or data[0x11] != 1:
                return None
            card_id, = struct.unpack_from('<H', data, 0x08)
            tid, sid = struct.unpack_from('<HH', data, 0x18)
            id32, = struct.unpack_from('<I', data, 0x18)
            origin_game, = struct.unpack_from('<I', data, 0x1C)
            egg_loc, loc, ball, held_item = struct.unpack_from('<4H', data, 0x220)
            moves = list(struct.unpack_from('<4H', data, 0x228))
            relearn = list(struct.unpack_from('<4H', data, 0x230))
            raw_species, = struct.unpack_from('<H', data, 0x238)
            species = get_national9(raw_species)
            form = data[0x23A]
            gender = data[0x23B]
            level = data[0x23C]
            nature = data[0x23E]
            ability_num = data[0x23F]
            is_shiny = 1 if data[0x240] in (1, 2, 3, 4) else 0
            met_level = data[0x241]
            ot = data[0x146:0x160].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            if not ot:
                ot = data[0x12C:0x146].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            return {
                'generation': 9,
                'gameVersion': origin_game,
                'species': species,
                'form': form,
                'isShiny': is_shiny,
                'level': level,
                'gender': gender,
                'ball': ball if ball else 16,
                'nature': nature if nature < 25 else 255,
                'abilityNum': ability_num if ability_num < 3 else 0,
                'heldItem': held_item,
                'moves': moves,
                'relearnMoves': relearn,
                'metLocation': loc if loc else 30001,
                'metLevel': met_level if met_level else level,
                'metYear': 22,
                'metMonth': 1,
                'metDay': 1,
                'tid16': tid,
                'sid16': sid,
                'id32': id32,
                'otName': ot,
                'cardId': card_id,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }
        elif ext == '.wa9':
            if len(data) < 0x2B0 or data[0x11] != 1:
                return None
            card_id, = struct.unpack_from('<H', data, 0x08)
            tid, sid = struct.unpack_from('<HH', data, 0x18)
            id32, = struct.unpack_from('<I', data, 0x18)
            origin_game, = struct.unpack_from('<I', data, 0x1C)
            egg_loc, loc, ball, held_item = struct.unpack_from('<4H', data, 0x258)
            moves = list(struct.unpack_from('<4H', data, 0x260))
            relearn = list(struct.unpack_from('<4H', data, 0x268))
            raw_species, = struct.unpack_from('<H', data, 0x270)
            species = get_national9(raw_species)
            form = data[0x272]
            gender = data[0x273]
            level = data[0x274]
            nature = data[0x276]
            ability_num = data[0x277]
            is_shiny = 1 if data[0x278] in (1, 2, 3, 4) else 0
            met_level = data[0x279]
            ot = data[0x146:0x160].decode('utf-16le', errors='ignore').split(chr(0))[0].strip()
            return {
                'generation': 9,
                'gameVersion': origin_game,
                'species': species,
                'form': form,
                'isShiny': is_shiny,
                'level': level,
                'gender': gender,
                'ball': ball if ball else 16,
                'nature': nature if nature < 25 else 255,
                'abilityNum': ability_num if ability_num < 3 else 0,
                'heldItem': held_item,
                'moves': moves,
                'relearnMoves': [0, 0, 0, 0],
                'metLocation': loc if loc else 30001,
                'metLevel': met_level if met_level else level,
                'metYear': 25,
                'metMonth': 1,
                'metDay': 1,
                'tid16': tid,
                'sid16': sid,
                'id32': id32,
                'otName': ot,
                'cardId': card_id,
                'fateful': 1,
                'ribbonClassic': 1,
                'ribbonEvent': 0,
                'ribbonPremier': 0,
                'ribbonWishing': 0,
                'ribbonSouvenir': 0,
            }

    return None

GEN3_ICONIC = [
    {
        'generation': 3,
        'gameVersion': 1,
        'species': 151,
        'form': 0,
        'isShiny': 0,
        'level': 10,
        'gender': 2,
        'ball': 4,
        'nature': 255,
        'abilityNum': 0,
        'heldItem': 0,
        'moves': [152, 0, 0, 0],
        'relearnMoves': [0, 0, 0, 0],
        'metLocation': 255,
        'metLevel': 10,
        'metYear': 5,
        'metMonth': 1,
        'metDay': 1,
        'tid16': 507,
        'sid16': 0,
        'id32': 507,
        'otName': 'Aura',
        'title': 'Aura Mew (Europe)',
        'cardId': 1,
        'fateful': 1,
        'ribbonClassic': 0,
        'ribbonEvent': 0,
        'ribbonPremier': 0,
        'ribbonWishing': 0,
        'ribbonSouvenir': 0,
    },
    {
        'generation': 3,
        'gameVersion': 1,
        'species': 251,
        'form': 0,
        'isShiny': 0,
        'level': 10,
        'gender': 2,
        'ball': 4,
        'nature': 255,
        'abilityNum': 0,
        'heldItem': 0,
        'moves': [140, 94, 237, 215],
        'relearnMoves': [0, 0, 0, 0],
        'metLocation': 255,
        'metLevel': 10,
        'metYear': 6,
        'metMonth': 1,
        'metDay': 1,
        'tid16': 70,
        'sid16': 0,
        'id32': 70,
        'otName': '10 ANIV',
        'title': '10th Anniversary Celebi (USA)',
        'cardId': 2,
        'fateful': 1,
        'ribbonClassic': 0,
        'ribbonEvent': 0,
        'ribbonPremier': 0,
        'ribbonWishing': 0,
        'ribbonSouvenir': 0,
    },
    {
        'generation': 3,
        'gameVersion': 1,
        'species': 385,
        'form': 0,
        'isShiny': 0,
        'level': 5,
        'gender': 2,
        'ball': 4,
        'nature': 255,
        'abilityNum': 0,
        'heldItem': 204,
        'moves': [322, 93, 273, 0],
        'relearnMoves': [0, 0, 0, 0],
        'metLocation': 255,
        'metLevel': 5,
        'metYear': 3,
        'metMonth': 1,
        'metDay': 1,
        'tid16': 40122,
        'sid16': 0,
        'id32': 40122,
        'otName': 'CHANNEL',
        'title': 'Pokemon Channel Jirachi (Europe)',
        'cardId': 3,
        'fateful': 1,
        'ribbonClassic': 0,
        'ribbonEvent': 0,
        'ribbonPremier': 0,
        'ribbonWishing': 0,
        'ribbonSouvenir': 0,
    },
    {
        'generation': 3,
        'gameVersion': 1,
        'species': 385,
        'form': 0,
        'isShiny': 0,
        'level': 5,
        'gender': 2,
        'ball': 4,
        'nature': 255,
        'abilityNum': 0,
        'heldItem': 203,
        'moves': [273, 93, 0, 0],
        'relearnMoves': [0, 0, 0, 0],
        'metLocation': 255,
        'metLevel': 5,
        'metYear': 3,
        'metMonth': 1,
        'metDay': 1,
        'tid16': 20043,
        'sid16': 0,
        'id32': 20043,
        'otName': 'WISHMKR',
        'title': 'Colosseum Bonus Disc Jirachi',
        'cardId': 4,
        'fateful': 1,
        'ribbonClassic': 0,
        'ribbonEvent': 0,
        'ribbonPremier': 0,
        'ribbonWishing': 0,
        'ribbonSouvenir': 0,
    },
    {
        'generation': 3,
        'gameVersion': 1,
        'species': 386,
        'form': 0,
        'isShiny': 0,
        'level': 70,
        'gender': 2,
        'ball': 4,
        'nature': 255,
        'abilityNum': 0,
        'heldItem': 0,
        'moves': [353, 354, 94, 285],
        'relearnMoves': [0, 0, 0, 0],
        'metLocation': 255,
        'metLevel': 70,
        'metYear': 6,
        'metMonth': 1,
        'metDay': 1,
        'tid16': 5,
        'sid16': 0,
        'id32': 5,
        'otName': 'SPACE C',
        'title': 'SPACE C Deoxys (Space Center)',
        'cardId': 5,
        'fateful': 1,
        'ribbonClassic': 0,
        'ribbonEvent': 0,
        'ribbonPremier': 0,
        'ribbonWishing': 0,
        'ribbonSouvenir': 0,
    },
]

def pack_records(records, out_path):
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    count = len(records)
    with open(out_path, 'wb') as f:
        header = struct.pack('<4sIII', b'PKEV', 1, count, RECORD_SIZE)
        f.write(header)

        for idx, r in enumerate(records):
            ot_bytes = r['otName'].encode('utf-8')[:23]
            ot_padded = ot_bytes + bytes([0]) * (24 - len(ot_bytes))
            title_bytes = r['title'].encode('utf-8')[:47]
            title_padded = title_bytes + bytes([0]) * (48 - len(title_bytes))

            moves = (r['moves'] + [0] * 4)[:4]
            relearn = (r['relearnMoves'] + [0] * 4)[:4]

            # clamp all uint8 fields:
            generation = max(0, min(255, int(r['generation'])))
            gameVersion = max(0, min(255, int(r['gameVersion'])))
            form = max(0, min(255, int(r['form'])))
            isShiny = max(0, min(255, int(r['isShiny'])))
            level = max(0, min(255, int(r['level'])))
            gender = max(0, min(255, int(r['gender'])))
            ball = max(0, min(255, int(r['ball'])))
            nature = max(0, min(255, int(r['nature'])))
            abilityNum = max(0, min(255, int(r['abilityNum'])))
            metLevel = max(0, min(255, int(r['metLevel'])))
            metYear = max(0, min(255, int(r['metYear'])))
            metMonth = max(0, min(255, int(r['metMonth'])))
            metDay = max(0, min(255, int(r['metDay'])))
            fateful = max(0, min(255, int(r['fateful'])))
            ribbonClassic = max(0, min(255, int(r['ribbonClassic'])))
            ribbonEvent = max(0, min(255, int(r['ribbonEvent'])))
            ribbonPremier = max(0, min(255, int(r['ribbonPremier'])))
            ribbonWishing = max(0, min(255, int(r['ribbonWishing'])))
            ribbonSouvenir = max(0, min(255, int(r['ribbonSouvenir'])))

            entry = struct.pack(
                EVENT_RECORD_FORMAT,
                idx + 1,
                generation,
                gameVersion,
                r['species'],
                form,
                isShiny,
                level,
                gender,
                ball,
                nature,
                abilityNum,
                0,
                r['heldItem'],
                moves[0], moves[1], moves[2], moves[3],
                relearn[0], relearn[1], relearn[2], relearn[3],
                r['metLocation'],
                metLevel,
                metYear,
                metMonth,
                metDay,
                r['tid16'],
                r['sid16'],
                r['id32'],
                ot_padded,
                title_padded,
                r['cardId'],
                fateful,
                ribbonClassic,
                ribbonEvent,
                ribbonPremier,
                ribbonWishing,
                ribbonSouvenir,
                bytes([0]) * 18
            )
            f.write(entry)
        print(f'Packed {count} event records to {out_path} ({os.path.getsize(out_path)} bytes)')

def find_bundle_dir():
    candidates = [
        'PKForge/src/PKForge.App/Resources/Events',
        '../PKForge/src/PKForge.App/Resources/Events',
        '/home/aero/Documents/antigravity/optimistic-mendeleev/PKForge/src/PKForge.App/Resources/Events',
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None

def collect_all_events():
    all_events = []
    all_events.extend(GEN3_ICONIC)

    bundle_dir = find_bundle_dir()
    if not bundle_dir:
        print('Warning: PKForge events bundle directory not found!')
        return all_events

    seen_keys = set()
    for gen in [4, 5, 6, 7, 8, 9]:
        bundle_path = os.path.join(bundle_dir, f'events-g{gen}.bin.gz')
        if not os.path.exists(bundle_path):
            print(f'Warning: bundle {bundle_path} not found')
            continue
        with gzip.open(bundle_path, 'rb') as gz:
            count, = struct.unpack('<I', gz.read(4))
            for _ in range(count):
                nl, = struct.unpack('<H', gz.read(2))
                filename = gz.read(nl).decode('utf-8')
                dl, = struct.unpack('<I', gz.read(4))
                raw_data = gz.read(dl)

                title = clean_title(filename)
                if any(x in title for x in ['Statue', 'Shirt', 'Clothing', 'Points', 'Item', 'Candy', 'Curry', 'Crystal', 'Berry', 'Power', 'Pass', 'Key', 'Ticket', 'App', 'Seal']):
                    continue

                info = parse_card(filename, raw_data, gen)
                if not info or info['species'] == 0 or info['species'] > 1025:
                    continue

                info['title'] = title[:47]
                key = (info['species'], info['form'], info['isShiny'], info['title'], info['level'], info['otName'], info['heldItem'])
                if key in seen_keys:
                    continue
                seen_keys.add(key)
                all_events.append(info)

    return all_events

if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, '..'))
    default_out = os.path.join(repo_root, 'romfs/events/events.bin')
    out_file = sys.argv[1] if len(sys.argv) > 1 else default_out
    records = collect_all_events()
    pack_records(records, out_file)
