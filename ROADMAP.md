# PKSE roadmap

Collected from the double-blind review of the `pkforge-overhaul` work (two independent reviewers, neither
seeing the other's report). Tick items off as they land.

## Known limitations (reviewed, not fixed yet)

- [ ] **Gen 3/4 wild catches do not pick an encounter slot.** The generator rolls a valid
      PID / IV chain but the slot value in that chain is arbitrary, so a strict PKHeX check may name a different
      slot. Needs per-route slot tables (the encounter data records rows, not slot numbers).
- [ ] **Seed-correlated Gen 8+ encounters** (Legends: Arceus spawns, Sword/Shield overworld and symbol
      statics): EC, PID, IVs, size and Tera type are independent random values, not one seed's output.
      Also unhandled: the Alpha flag, Wild Area level boost, fixed-hidden-ability statics.
- [ ] **Encounter-fixed abilities** are not modelled (the encounter rows carry no ability field), so a
      static locked to one ability can come out with the other.
- [ ] **Hidden abilities** are never rolled for catches.
- [ ] **Hatched friendship** is a fixed 120; unverified for Gen 8+.
- [ ] Screenshot compression and the PNG write run on the main thread (a one-frame hitch).
- [ ] Typing a dialog's message swallows any tap, including nav-bar badges.

## Theme / aesthetic

1. [ ] One shared colour-token table for the picker, hub, park and editor; drop or fully theme the unused
       dark / light modes.
2. [ ] Dual-screen look: a 3DS-style info bar over a bottom "touch" panel with a bezel divider.
3. [ ] Per-generation animated tiled backgrounds (PKSM-style): GBA green, DS blue, Switch red.
4. [ ] Pixel (bitmap) font for dialogs with classic two-tone text-window borders.
5. [ ] Bouncing hand / Poké Ball selection cursor with a soft shadow.
6. [ ] Legality badge colours and icon (Legal green, Check amber, Illegal red) used the same way
       everywhere, including the post-catch status line.
7. [ ] Type-coloured gradient headers in summary views; shiny sparkle on shiny Pokémon.
8. [ ] The real box wallpapers of each game; box name in a tab.
9. [ ] Menu sound effects (blip, error, save jingle) with a mute toggle in Settings.
10. [ ] One shared animation timing curve and duration set for pop-in, slide and fade.
11. [ ] A uniform button-legend strip on every screen (the catch chooser's hidden controls included).
12. [ ] Game cartridge art / colour accents in the editor header, matching the picker shelf.

## Features

1. [ ] Catch chooser: choose nature (honouring Synchronize in Gen 3/4), ball (from the encounter's legal
       set) and gender, constrained to legal values. *(Level now and met level are done.)*
2. [ ] Legality preview before committing, with a "fix common problems" action.
3. [ ] Diff view before saving: every changed Pokémon and field with its legality status.
4. [ ] Batch legality scan of all boxes with one-tap fixes (ability bit, met data, checksum).
5. [ ] Living-dex assistant: fill missing entries with legal catches into empty box slots, respecting
       version exclusives.
6. [ ] Search and filter boxes by species, shiny, IVs, ball, ability, OT or legality; multi-select move and
       release.
7. [ ] Showdown set text and `.pk*` file import / export; QR / text sharing of one Pokémon.
8. [ ] Undo / redo of edits within a session.
9. [ ] Automatic timestamped backup on every inject, with one-tap restore and rotation limits.
10. [ ] Mystery Gift / Wonder Card injection with event-legality validation (the event database exists).
11. [ ] Cross-game bank transfer with a per-transfer legality preview.
12. [ ] Poképark: photo gallery viewer; a bond / friendship readout per resident.

## Poképark ideas (behaviour and fun)

Quality-of-life items (quick menu, follow camera, activity highlight, journal filter, away summary,
speed / effects settings, send-all-shinies) are done. Still to come:

**Behaviour**
- [ ] Time-of-day routines: a morning, midday and evening spot per resident, learned from where it rests.
- [ ] Moves in play: Surf across the lake, Flamethrower at the cinder ring, Dig and pop up elsewhere,
      Psychic floating objects.
- [ ] Abilities in play: Static sparking near Water types, Pickup finding a berry, Intimidate making
      others back off.
- [ ] Evolution families: babies follow their evolved relatives; protective behaviour toward pre-evolutions.
- [ ] Rivalries with consequences: races for the same toy, contests for the best nap spot, a rematch
      event days later.
- [ ] Moods that persist between visits (a sulk or a good day carries over).
- [ ] Memory of the player: remembers yesterday's visit, the last berry, how long you were away.
- [ ] Nature quirks: Naughty steals berries, Careful walks around toys, Hasty queues first.

**Quality of life still open**
- [ ] Roster presets saved by name ("my team", "starters"), beyond party / shinies.

**Fun additions**
- [ ] Mini-games: ball-toss accuracy, hide-and-seek, berry catch, a race around the lake.
- [ ] Collectibles (shiny stones, feathers, berries lying around) with a collection page.
- [ ] Events: firefly festival, berry fair, a rainbow after storms, a meteor shower with wishes.
- [ ] Befriend wild visitors until they become regulars; invite them as park-only residents.
- [ ] Photo upgrades: frames, filters, stickers, an in-park gallery, the favourite photo as the banner.
- [ ] Seasons with their own visuals and moods.
- [ ] A small habit per resident (collects stones, dances to music, always sits under the tree).
- [ ] Park builder: place benches, lanterns and a pond that some residents prefer.

## Done from the review

- Encounter rows only merge when their level ranges overlap, so no impossible met level is offered.
- A catch that cannot be rolled is refused instead of written as PID 0 / zero IVs.
- Evolved catches start at least one level past the met level; trade-evolution chains are cut in Gen 6+.
- Gen 1/2 offer Custom only (DVs, not a PID).
- Gen 3 Unown's letter is built into its PID; single-ability species use slot 1 in Gen 5.
- Per-generation gender ratio and base friendship; an exact-game picker for loose emulator saves.
- Saving leaves already-corrupt Pokémon as found instead of laundering their checksum.
- Park and resume files are written atomically; park fields are sanitised.
- Poképark residents are keyed on the Pokémon (species, PID, trainer id), not its slot.
- A touch that begins during a pop-in keeps one mapping until release.
- Encounter lookups are cached instead of rebuilt every frame.
- Poképark quality of life: a quick menu (Down) to call everyone, tuck everyone in, throw berries for
  all, highlight who is sleeping / playing / lonely / quarrelling, follow camera, park speed and fewer
  effects; a journal filter (X); a "While you were away" summary; Y twice sends every shiny.
