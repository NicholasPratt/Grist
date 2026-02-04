/*
 * Grist — Granular Sample Synth (DPF)
 *
 * v1 placeholder DSP: sine synth to validate CLAP + MIDI + UI plumbing.
 */

#include "Grist.hpp"
#include "DistrhoPluginInfo.h"

#include <cmath>
#include <algorithm>

// DPF toolchains may not use C++17 by default; provide our own clamp.
static inline float fclampf(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

START_NAMESPACE_DISTRHO

static constexpr double kTwoPi = 6.283185307179586476925286766559;

Grist::Grist()
    : Plugin(kParamCount, 0, 0),
      fGain(0.8f),
      fGrainSizeMs(60.0f),
      fDensity(20.0f),
      fPosition(0.5f),
      fSpray(0.0f),
      fPitch(0.0f),
      fRandomPitch(0.0f),
      fSampleRate(48000.0),
      gateOn(false),
      currentNote(60),
      currentVelocity(0.8f),
      phase(0.0)
{
}

void Grist::activate()
{
    phase = 0.0;
    gateOn = false;
    currentNote = 60;
    currentVelocity = 0.8f;
}

void Grist::sampleRateChanged(double newSampleRate)
{
    fSampleRate = newSampleRate > 1.0 ? newSampleRate : 48000.0;
}

void Grist::initParameter(uint32_t index, Parameter& parameter)
{
    parameter.hints = kParameterIsAutomatable;

    switch (index)
    {
    case kParamGain:
        parameter.name = "Gain";
        parameter.symbol = "gain";
        parameter.unit = "";
        parameter.ranges.def = 0.8f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        break;

    case kParamGrainSizeMs:
        parameter.name = "Grain Size";
        parameter.symbol = "grain_size_ms";
        parameter.unit = "ms";
        parameter.ranges.def = 60.0f;
        parameter.ranges.min = 5.0f;
        parameter.ranges.max = 250.0f;
        break;

    case kParamDensity:
        parameter.name = "Density";
        parameter.symbol = "density";
        parameter.unit = "gr/s";
        parameter.ranges.def = 20.0f;
        parameter.ranges.min = 1.0f;
        parameter.ranges.max = 80.0f;
        break;

    case kParamPosition:
        parameter.name = "Position";
        parameter.symbol = "position";
        parameter.unit = "%";
        parameter.ranges.def = 50.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 100.0f;
        break;

    case kParamSpray:
        parameter.name = "Spray";
        parameter.symbol = "spray";
        parameter.unit = "%";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 100.0f;
        break;

    case kParamPitch:
        parameter.name = "Pitch";
        parameter.symbol = "pitch";
        parameter.unit = "st";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -24.0f;
        parameter.ranges.max = 24.0f;
        break;

    case kParamRandomPitch:
        parameter.name = "Rnd Pitch";
        parameter.symbol = "random_pitch";
        parameter.unit = "st";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 12.0f;
        break;
    }
}

float Grist::getParameterValue(uint32_t index) const
{
    switch (index)
    {
    case kParamGain: return fGain;
    case kParamGrainSizeMs: return fGrainSizeMs;
    case kParamDensity: return fDensity;
    case kParamPosition: return fPosition * 100.0f;
    case kParamSpray: return fSpray * 100.0f;
    case kParamPitch: return fPitch;
    case kParamRandomPitch: return fRandomPitch;
    default: return 0.0f;
    }
}

void Grist::setParameterValue(uint32_t index, float value)
{
    switch (index)
    {
    case kParamGain:
        fGain = fclampf(value, 0.0f, 1.0f);
        break;
    case kParamGrainSizeMs:
        fGrainSizeMs = fclampf(value, 5.0f, 250.0f);
        break;
    case kParamDensity:
        fDensity = fclampf(value, 1.0f, 80.0f);
        break;
    case kParamPosition:
        fPosition = fclampf(value / 100.0f, 0.0f, 1.0f);
        break;
    case kParamSpray:
        fSpray = fclampf(value / 100.0f, 0.0f, 1.0f);
        break;
    case kParamPitch:
        fPitch = fclampf(value, -24.0f, 24.0f);
        break;
    case kParamRandomPitch:
        fRandomPitch = fclampf(value, 0.0f, 12.0f);
        break;
    }
}

double Grist::midiNoteToHz(int note) const
{
    // A4 = 440 at MIDI 69
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

void Grist::run(const float** /*inputs*/, float** outputs, uint32_t frames,
                const MidiEvent* midiEvents, uint32_t midiEventCount)
{
    float* outL = outputs[0];
    float* outR = outputs[1];

    // Parse MIDI events for this block.
    for (uint32_t i = 0; i < midiEventCount; ++i)
    {
        const MidiEvent& ev = midiEvents[i];
        if (ev.size < 3) continue;

        const uint8_t st = ev.data[0] & 0xF0;
        const uint8_t note = ev.data[1] & 0x7F;
        const uint8_t vel  = ev.data[2] & 0x7F;

        const bool isNoteOn  = (st == 0x90) && (vel > 0);
        const bool isNoteOff = (st == 0x80) || ((st == 0x90) && (vel == 0));

        if (isNoteOn)
        {
            gateOn = true;
            currentNote = (int)note;
            currentVelocity = (float)vel / 127.0f;
            // reset phase for now
            phase = 0.0;
        }
        else if (isNoteOff)
        {
            if (gateOn && (int)note == currentNote)
                gateOn = false;
        }
    }

    // Generate audio
    if (!gateOn)
    {
        for (uint32_t i = 0; i < frames; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }
        return;
    }

    const double baseHz = midiNoteToHz(currentNote);
    const double pitchMul = std::pow(2.0, (double)fPitch / 12.0);
    const double hz = baseHz * pitchMul;
    const double phaseInc = kTwoPi * hz / fSampleRate;
    const float amp = fGain * currentVelocity;

    double ph = phase;
    for (uint32_t i = 0; i < frames; ++i)
    {
        const float s = (float)std::sin(ph) * amp;
        outL[i] = s;
        outR[i] = s;
        ph += phaseInc;
        if (ph >= kTwoPi) ph -= kTwoPi;
    }
    phase = ph;
}

Plugin* createPlugin() { return new Grist(); }

END_NAMESPACE_DISTRHO
