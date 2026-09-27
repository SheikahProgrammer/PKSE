#!/usr/bin/env python3
"""Bundle item sprites from PKHeX into PKSE romfs.

Sources (in priority order):
  1. Big Items/bitem_{id}.png   -- 20×20 pixel icons
  2. Artwork Items/aitem_{id}.png -- 25×25 artwork icons (fallback for items missing from Big Items)

Destination: romfs/sprites/items/{id}.png
"""

import shutil
import re
from pathlib import Path

PKHEX_IMG = Path("/home/aero/Documents/antigravity/optimistic-mendeleev/PKForge/external/PKHeX/PKHeX.Drawing.PokeSprite/Resources/img")
BIG_ITEMS = PKHEX_IMG / "Big Items"
ART_ITEMS = PKHEX_IMG / "Artwork Items"
DEST = Path("/home/aero/Documents/antigravity/optimistic-mendeleev/PKSE/romfs/sprites/items")

DEST.mkdir(parents=True, exist_ok=True)

# Collect all item IDs from both sources
big_items = {}  # id -> path
art_items = {}  # id -> path

for f in sorted(BIG_ITEMS.glob("bitem_*.png")):
    m = re.match(r"bitem_(\d+)\.png", f.name)
    if m:
        big_items[int(m.group(1))] = f

for f in sorted(ART_ITEMS.glob("aitem_*.png")):
    m = re.match(r"aitem_(\d+)\.png", f.name)
    if m:
        art_items[int(m.group(1))] = f

# Merge: prefer big items, fill gaps from artwork items
all_ids = sorted(set(big_items.keys()) | set(art_items.keys()))

copied = 0
for item_id in all_ids:
    src = big_items.get(item_id) or art_items.get(item_id)
    if src:
        shutil.copy2(src, DEST / f"{item_id}.png")
        copied += 1

print(f"Bundled {copied} item sprites from {len(big_items)} Big Items + {len(art_items)} Artwork Items")
print(f"ID range: {min(all_ids)} - {max(all_ids)}")
