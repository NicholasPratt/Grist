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

// Synth: no audio inputs, stereo out
#define DISTRHO_PLUGIN_NUM_INPUTS      0
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2

#define DISTRHO_UI_USE_NANOVG          1
#define DISTRHO_UI_USER_RESIZABLE      0
#define DISTRHO_UI_DEFAULT_WIDTH       640
#define DISTRHO_UI_DEFAULT_HEIGHT      360

enum Parameters {
    kParamGain = 0,
    kParamGrainSizeMs,
    kParamDensity,
    kParamPosition,
    kParamSpray,
    kParamPitch,
    kParamRandomPitch,
    kParamCount
};

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED

