/*
 * Grist — Granular Sample Synth (DPF)
 *
 * v0.2: WAV sample load + simple sample playback (MIDI in → audio out)
 * Next: granular engine
 */

#include "Grist.hpp"
#include "DistrhoPluginInfo.h"

#define DR_WAV_IMPLEMENTATION
#include "DSP/dr_wav.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>

START_NAMESPACE_DISTRHO

static inline float fclampf(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

Grist::Grist()
    : Plugin(kParamCount, 0, 1), // params, programs, states
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
      sampleRateLoaded(0),
      playhead(0.0)
{
}

void Grist::activate()
{
    gateOn = false;
    currentNote = 60;
    currentVelocity = 0.8f;
    playhead = 0.0;

    // Try loading default sample location on activate (no dialogs needed).
    if (sampleL.empty())
        loadDefaultSample();
}

void Grist::sampleRateChanged(double newSampleRate)
{
    fSampleRate = newSampleRate > 1.0 ? newSampleRate : 48000.0;
}

void Grist::initState(uint32_t index, State& state)
{
    if (index == 0)
    {
        state.key = "sample";
        state.defaultValue = "";
        state.hints = kStateIsFilenamePath;
        state.label = "Sample";
    }
}

void Grist::setState(const char* key, const char* value)
{
    if (std::strcmp(key, "sample") != 0)
        return;

    if (value == nullptr || value[0] == '\0')
        return;

    if (std::strcmp(value, "__DEFAULT__") == 0)
    {
        loadDefaultSample();
        return;
    }

    loadWavFile(value);
}

void Grist::initParameter(uint32_t index, Parameter& parameter)
{
    parameter.hints = kParameterIsAutomatable;

    switch (index)
    {
    case kParamGain:
        parameter.name = "Gain";
        parameter.symbol = "gain";
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
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

bool Grist::loadDefaultSample()
{
    const char* home = std::getenv("HOME");
    if (!home) return false;
    std::string p(home);
    p += "/Documents/samples/grist.wav";
    return loadWavFile(p.c_str());
}

bool Grist::loadWavFile(const char* path)
{
    drwav wav;
    if (!drwav_init_file(&wav, path, nullptr))
        return false;

    // minimal loader expects PCM 16-bit in our trimmed dr_wav.
    const uint32_t ch = wav.channels;
    const uint32_t sr = wav.sampleRate;
    const uint64_t frames = wav.totalPCMFrameCount;
    if (ch < 1 || ch > 2 || frames == 0)
    {
        drwav_uninit(&wav);
        return false;
    }

    std::vector<float> interleaved;
    interleaved.resize((size_t)frames * ch);
    const uint64_t read = drwav_read_pcm_frames_f32(&wav, frames, interleaved.data());
    drwav_uninit(&wav);
    if (read == 0)
        return false;

    std::vector<float> L, R;
    L.resize((size_t)read);
    R.resize((size_t)read);

    if (ch == 1)
    {
        for (uint64_t i = 0; i < read; ++i)
        {
            const float s = interleaved[(size_t)i];
            L[(size_t)i] = s;
            R[(size_t)i] = s;
        }
    }
    else
    {
        for (uint64_t i = 0; i < read; ++i)
        {
            L[(size_t)i] = interleaved[(size_t)i * 2 + 0];
            R[(size_t)i] = interleaved[(size_t)i * 2 + 1];
        }
    }

    {
        const std::lock_guard<std::mutex> lock(sampleMutex);
        sampleL.swap(L);
        sampleR.swap(R);
        sampleRateLoaded = sr;
        samplePath = path;
    }

    return true;
}

void Grist::run(const float** /*inputs*/, float** outputs, uint32_t frames,
                const MidiEvent* midiEvents, uint32_t midiEventCount)
{
    float* outL = outputs[0];
    float* outR = outputs[1];

    // default clear
    for (uint32_t i = 0; i < frames; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }

    // MIDI parse
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

            // reset playback position on each note
            std::lock_guard<std::mutex> lock(sampleMutex);
            const size_t len = sampleL.size();
            playhead = (len > 0) ? (double)(fPosition * (double)(len - 1)) : 0.0;
        }
        else if (isNoteOff)
        {
            if (gateOn && (int)note == currentNote)
                gateOn = false;
        }
    }

    if (!gateOn)
        return;

    // Snapshot sample pointers/length without holding lock during render.
    std::vector<float> Lcopy;
    std::vector<float> Rcopy;
    uint32_t srLoaded = 0;
    {
        std::lock_guard<std::mutex> lock(sampleMutex);
        srLoaded = sampleRateLoaded;
        // Avoid copying if no sample loaded
        if (sampleL.empty() || sampleR.empty())
            return;
        Lcopy = sampleL;
        Rcopy = sampleR;
    }

    const size_t len = Lcopy.size();
    if (len < 2)
        return;

    // playback rate: note pitch relative to C4 (60)
    const double baseHz = midiNoteToHz(currentNote);
    const double refHz  = midiNoteToHz(60);
    const double noteMul = baseHz / refHz;
    const double pitchMul = std::pow(2.0, (double)fPitch / 12.0);
    const double rate = noteMul * pitchMul * ((srLoaded > 0) ? ((double)srLoaded / fSampleRate) : 1.0);

    const float amp = fGain * currentVelocity;

    double ph = playhead;
    for (uint32_t i = 0; i < frames; ++i)
    {
        const size_t idx = (size_t)ph;
        if (idx + 1 >= len)
            break;
        const float frac = (float)(ph - (double)idx);
        const float l = lerp(Lcopy[idx], Lcopy[idx + 1], frac) * amp;
        const float r = lerp(Rcopy[idx], Rcopy[idx + 1], frac) * amp;
        outL[i] = l;
        outR[i] = r;
        ph += rate;
        if (ph >= (double)(len - 1))
            ph = (double)(len - 1);
    }
    playhead = ph;
}

Plugin* createPlugin() { return new Grist(); }

END_NAMESPACE_DISTRHO
