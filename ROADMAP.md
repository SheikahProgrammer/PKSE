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
