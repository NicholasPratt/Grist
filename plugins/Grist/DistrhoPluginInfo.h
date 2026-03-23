/*
 * Grist — Granular Sample Synth (DPF)
 * Copyright (C) 2026
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "ArchieAudio"
#define DISTRHO_PLUGIN_NAME    "Grist"
#define DISTRHO_PLUGIN_URI     "https://example.com/grist" // TODO: replace
#define DISTRHO_PLUGIN_CLAP_ID "com.archieaudio.grist"

// Plugin type
#define DISTRHO_PLUGIN_HAS_UI          1
#define DISTRHO_PLUGIN_IS_RT_SAFE      1
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 0
#define DISTRHO_PLUGIN_WANT_STATE 1

// Synth: no audio inputs, stereo out
#define DISTRHO_PLUGIN_NUM_INPUTS      0
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2

#define DISTRHO_UI_USE_NANOVG          1
#define DISTRHO_UI_USER_RESIZABLE      0
#define DISTRHO_UI_DEFAULT_WIDTH       1180
#define DISTRHO_UI_DEFAULT_HEIGHT      560

// Enable DPF file browser support (requestStateFile / openFileBrowser)
#define DISTRHO_UI_FILE_BROWSER        1

enum Parameters {
    // --- synth controls ---
    kParamGain = 0,
    kParamGrainSizeMs,
    kParamDensity,
    kParamPosition,
    kParamSpray,
    kParamPitch,
    kParamRandomPitch,
    kParamPitchEnvAmt,
    kParamPitchEnvDecayMs,
    kParamAttackMs,
    kParamReleaseMs,
    kParamKillOnRetrig,
    kParamNewVoiceOnRetrig,

    // --- modulation sources (automatable) ---
    kParamLfo1RateHz,
    kParamLfo1Shape,
    kParamLfo2RateHz,
    kParamLfo2Shape,

    // XY performance pad (can be automated / MIDI-mapped)
    kParamX,
    kParamY,

    // Macros (recommended for MIDI learn)
    kParamMacro1,
    kParamMacro2,
    kParamMacro3,
    kParamMacro4,
    kParamMacro5,
    kParamMacro6,
    kParamMacro7,
    kParamMacro8,

    // Sample selection range (normalized 0..1, start <= end)
    kParamSampleStart,
    kParamSampleEnd,

    kParamLatch,

    kParamCount
};

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED

