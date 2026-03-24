# Grist modulation system (draft)

Goal: Lemondrop-style “modulation slots” (a few assignable sources per parameter) without exploding the automatable parameter count.

## Design constraints

- **CLAP + DPF**: parameters must be declared up-front; changing parameter count breaks session recall.
- We want **host automation** for primary musical controls (macros, LFO rates, etc.).
- We can store the *routing graph* (which source modulates which target) in a **state string** (non-RT setState), then apply it RT-safe in `run()`.

## Terminology

- **Source**: signal that outputs a value in `[-1, +1]` (LFOs, envelopes, velocity, keytrack, random, XY, macros, sequencer).
- **Target**: a synth parameter we can modulate (Position, Grain Size, Density, Pitch, Spray, Filter Cutoff, etc.).
- **Slot**: one connection `source -> target` with an amount.

## MVP (phase 1)

### Sources (global unless noted)

- LFO1: rate (Hz), shape, phase
- LFO2: rate (Hz), shape, phase
- Env1 (per voice): amp env (0..1 mapped to -1..+1 as needed)
- Velocity (per voice)
- Keytrack (per voice)
- XY-X, XY-Y (global)
- Macro1..Macro8 (global)

All sources are converted to `[-1, +1]` at the modulation engine boundary.

### Targets (initial)

- Grain Position (0..1)
- Grain Size (ms)
- Density (grains/sec)
- Spray (0..1)
- Pitch (semitones)

### Amount scaling

- Each target defines a **mod range** in the target’s native units.
- Slot amount is `[-1, +1]` and scales that range.

Example:
- Target `Position` range contribution: `pos += source * amount * 0.50` (i.e. +/- 50% of the full position range).
- Target `Pitch` range contribution: `pitch += source * amount * 12.0` (i.e. +/- 12 semitones).

### Routing storage

State key: `mod_matrix`

Format (simple, human-editable):

```
<target>:<slotIndex>:<source>:<amount>;...

# example
pos:0:lfo1:0.40;pitch:0:lfo2:-0.15
```

- `target` is a short id (pos,size,dens,spray,pitch,cut,res,...)
- `slotIndex` is 0..2
- `source` is a short id (lfo1,lfo2,env1,vel,key,x,y,m1..m8)
- `amount` float -1..1

Parsing occurs in `setState()` (non-RT). The audio thread only reads the resulting fixed-size arrays.

## Current UI behaviour (as implemented)

On the **Perform** tab, modulated parameters show **3 boxes** (slots).

- **Left-click** a slot: cycle through sources (`None → LFO1 → LFO2 → Env1 → … → M8 → None`).
  - When cycling to `None`, the slot amount is reset to 0.
  - When cycling from `None` to a source and the amount is 0, it initializes to a small default (currently `0.10`).
- **Left-click + drag** up/down on a slot: adjust amount in `[-1, +1]`.
- **Right-click** a slot: clear the **source assignment only** (no confirmation prompt).

The UI persists routing via the `mod_matrix` state string.

## Future

- Mod sequencer source (tempo-synced)
- Filter 1/2 targets + serial/parallel routing
- Per-voice random (sample&hold) sources
- MPE (pressure, slide, pitch) sources
