#!/usr/bin/env python3
"""
bundle_pixel_sprites.py
Consolidates authentic Gen 1-9 pixel sprites from PKForge/PKHeX into PKSE's romfs/sprites/pokemon_pixel/.
"""

import glob
import os
import shutil
import sys

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    pkforge_root = os.path.abspath(os.path.join(repo_root, "..", "PKForge"))
    
    src_big = os.path.join(pkforge_root, "external/PKHeX/PKHeX.Drawing.PokeSprite/Resources/img/Big Pokemon Sprites")
    src_big_s = os.path.join(pkforge_root, "external/PKHeX/PKHeX.Drawing.PokeSprite/Resources/img/Big Shiny Sprites")
    src_art = os.path.join(pkforge_root, "external/PKHeX/PKHeX.Drawing.PokeSprite/Resources/img/Artwork Pokemon Sprites")
    src_art_s = os.path.join(pkforge_root, "external/PKHeX/PKHeX.Drawing.PokeSprite/Resources/img/Artwork Shiny Sprites")
    
    dst_dir = os.path.join(repo_root, "romfs/sprites/pokemon_pixel")
    os.makedirs(dst_dir, exist_ok=True)
    
    dest_map = {}
    
    # 1. Big Pokemon Sprites (b_*) -> {*}
    for p in glob.glob(os.path.join(src_big, "b_*.png")):
        fn = os.path.basename(p)[2:] # strip b_
        dest_map[fn] = p
        
    # 2. Big Shiny Sprites (b_*s) -> {*s}
    for p in glob.glob(os.path.join(src_big_s, "b_*.png")):
        fn = os.path.basename(p)[2:] # strip b_
        dest_map[fn] = p
        
    # 3. Artwork Pokemon Sprites (a_*) -> {*} (adds Gen 9 species 906-1025)
    for p in glob.glob(os.path.join(src_art, "a_*.png")):
        fn = os.path.basename(p)[2:] # strip a_
        if fn not in dest_map:
            dest_map[fn] = p
            
    # 4. Artwork Shiny Sprites (a_*s) -> {*s} (adds Gen 9 shinies)
    for p in glob.glob(os.path.join(src_art_s, "a_*.png")):
        fn = os.path.basename(p)[2:] # strip a_
        if fn not in dest_map:
            dest_map[fn] = p

    # 5. Base aliases for special species
    if "869-0-0.png" in dest_map:
        dest_map["869.png"] = dest_map["869-0-0.png"]
        dest_map["869-0.png"] = dest_map["869-0-0.png"]
    if "869-0-0s.png" in dest_map:
        dest_map["869s.png"] = dest_map["869-0-0s.png"]
        dest_map["869-0s.png"] = dest_map["869-0-0s.png"]

    # Unknown placeholder (b_0.png -> 0.png)
    if "0.png" not in dest_map and os.path.exists(os.path.join(src_big, "b_0.png")):
        dest_map["0.png"] = os.path.join(src_big, "b_0.png")

    print(f"Copying {len(dest_map)} pixel sprites to {dst_dir}...")
    copied = 0
    for dst_name, src_path in dest_map.items():
        target = os.path.join(dst_dir, dst_name)
        shutil.copy2(src_path, target)
        copied += 1

    total_bytes = sum(os.path.getsize(os.path.join(dst_dir, f)) for f in os.listdir(dst_dir) if f.endswith(".png"))
    print(f"Successfully bundled {copied} sprites ({total_bytes / (1024*1024):.2f} MB).")

if __name__ == "__main__":
    main()
