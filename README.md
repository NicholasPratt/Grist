# Grist

**Grist** is a WIP **granular sample synth** (currently CLAP) built with the **DISTRHO Plugin Framework (DPF)**.

The goal is a playable, *lush*, polyphonic granular instrument with a big, friendly UI (Surge XT-style), fast iteration, and sensible modulation/envelope behaviour.

## Features (current)

- **CLAP synth** (stereo out)
- **WAV sample loader**
  - Load via host file dialog: **Load sample…**
  - Reload a default sample: **Reload default**
  - Failure-proofing: if a load fails, the previous sample keeps playing and the UI shows an error.
- **Granular engine (WIP)**
  - Grain size (ms)
  - Density (grains/sec)
  - Position + spray
  - Pitch + random pitch
  - **Per-note pitch envelope** (amount + decay)
- **Polyphony**
  - 16 voices with quietest-voice stealing
  - Optional “New Voice” retrigger mode (layering)
  - Note-off behaviour via **Attack/Release envelope** (simple linear AR currently)

## Build (Linux)

From the repo root:

```bash
make
```

Build outputs land in:

- `bin/Grist.clap`

## Install / Use in REAPER

1. Add the `bin/` folder to REAPER’s CLAP scan paths (Preferences → Plug-ins → CLAP), **or** copy `bin/Grist.clap` into one of your existing CLAP folders.
2. Re-scan.
3. Insert **Grist (CLAP)** as an instrument.
4. Open the UI and click **Load sample…** to pick a WAV.

## Repo layout

- `plugins/Grist/` — the plugin DSP + UI
- `dpf/` — DPF as a git submodule

## License

- Grist code: **ISC** (see `LICENSE`)
- DPF: separate license (see the `dpf/` submodule)

## Status / Roadmap (short)

- Better envelope shapes (ADSR, curves)
- Smoother parameter modulation (de-zippering)
- More grain controls (window choice, stereo spread, scan modes)
- Presets + better state UX
