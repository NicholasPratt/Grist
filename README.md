# Grist

A **granular sample synth** plugin (synth-only) inspired by devices like the Nanobox Lemondrop.

Built with **DPF (DISTRHO Plugin Framework)**.

## Status
MVP scaffolding in progress.

## MVP goals
- Load a single sample (WAV)
- MIDI note input → pitch + gate
- Grain engine controls:
  - Grain size (ms)
  - Density (grains/sec)
  - Position (0..1)
  - Spray (position jitter)
  - Pitch (semitones)
  - Random pitch
  - Gain

## Build (Linux)
From the repo root:

```bash
make
```

Outputs land in `bin/`.

## Next steps
- Implement sample loader + grain voice pool
- Basic UI for parameters
- Add preset/state save
