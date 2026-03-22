/*
 * Grist — Granular Sample Synth (DPF)
 *
 * v0.2: WAV sample load + simple sample playback (MIDI in → audio out)
 * Next: granular engine
 */

#include "Grist.hpp"
#include "DistrhoPluginInfo.h"
#include "GristVizBus.hpp"

#define DR_WAV_IMPLEMENTATION
#include "DSP/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#include "DSP/dr_mp3.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>

#include <string>

START_NAMESPACE_DISTRHO

static inline float fclampf(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

static inline float catmullRom(const float y0, const float y1, const float y2, const float y3, const float t)
{
    // Catmull-Rom spline (cubic), reasonably good for sample playback
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f*y0 - 5.0f*y1 + 4.0f*y2 - y3) * t2 + (-y0 + 3.0f*y1 - 3.0f*y2 + y3) * t3);
}

Grist::Grist()
    : Plugin(kParamCount, 0, 6), // params, programs, states
      fGain(0.8f),
      fGrainSizeMs(60.0f),
      fDensity(20.0f),
      fPosition(0.5f),
      fSpray(0.0f),
      fPitch(0.0f),
      fRandomPitch(0.0f),
      fPitchEnvAmt(0.0f),
      fPitchEnvDecayMs(120.0f),
      fAttackMs(5.0f),
      fReleaseMs(120.0f),
      fKillOnRetrig(1.0f),
      fNewVoiceOnRetrig(0.0f),
      fLfo1RateHz(0.25f),
      fLfo1Shape(0.0f),
      fLfo2RateHz(0.10f),
      fLfo2Shape(0.0f),
      fX(0.0f),
      fY(0.0f),
      fSampleRate(48000.0),
      gateOn(false),
      currentNote(60),
      currentVelocity(0.8f)
{
    vizEventCount = 0;
    vizDecim = 0;

    // voices init
    for (uint32_t v = 0; v < kMaxVoices; ++v)
    {
        voices[v].active = false;
        voices[v].gate = false;
        voices[v].releasing = false;
        voices[v].note = 60;
        voices[v].velocity = 1.0f;
        voices[v].env = 0.0f;
        voices[v].pitchEnv = 0.0f;
        voices[v].samplesToNextGrain = 0.0;
        for (uint32_t g = 0; g < Voice::kMaxGrains; ++g)
            voices[v].grains[g].active = false;
    }

    for (uint32_t n = 0; n < 128; ++n)
        noteQueues[n].clear();

    modMatrix.clear();
    // Demo defaults (so it immediately feels "alive")
    modMatrix.slots[(uint32_t)GristMod::Target::Position][0].source = GristMod::Source::LFO1;
    modMatrix.slots[(uint32_t)GristMod::Target::Position][0].amount = 0.25f;
    modMatrix.slots[(uint32_t)GristMod::Target::Pitch][0].source = GristMod::Source::LFO2;
    modMatrix.slots[(uint32_t)GristMod::Target::Pitch][0].amount = 0.10f;

    for (int i = 0; i < 8; ++i) fMacro[i] = 0.0f;
}

uint32_t Grist::rngU32()
{
    // LCG (good enough for modulation noise; deterministic)
    rngState = rngState * 1664525u + 1013904223u;
    return rngState;
}

float Grist::rngFloat01()
{
    // 24-bit mantissa
    return (float)((rngU32() >> 8) & 0x00FFFFFFu) / (float)0x01000000u;
}

void Grist::activate()
{
    gateOn = false;
    currentNote = 60;
    currentVelocity = 0.8f;

    for (uint32_t v = 0; v < kMaxVoices; ++v)
    {
        voices[v].active = false;
        voices[v].gate = false;
        voices[v].releasing = false;
        voices[v].env = 0.0f;
        voices[v].pitchEnv = 0.0f;
        voices[v].samplesToNextGrain = 0.0;
        for (uint32_t g = 0; g < Voice::kMaxGrains; ++g)
            voices[v].grains[g].active = false;
    }

    for (uint32_t n = 0; n < 128; ++n)
        noteQueues[n].clear();

    // Try loading default sample location on activate (no dialogs needed).
    {
        std::lock_guard<std::mutex> lock(sampleMutex);
        if (sample)
            return;
    }

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
    else if (index == 1)
    {
        state.key = "sample_status";
        state.defaultValue = "";
        state.hints = 0;
        state.label = "Sample Status";
    }
    else if (index == 2)
    {
        state.key = "sample_error";
        state.defaultValue = "";
        state.hints = 0;
        state.label = "Sample Error";
    }
    else if (index == 3)
    {
        state.key = "grains";
        state.defaultValue = "";
        state.hints = 0;
        state.label = "Grain Spawn Viz";
    }
    else if (index == 4)
    {
        state.key = "grains_active";
        state.defaultValue = "";
        state.hints = 0;
        state.label = "Grain Active Viz";
    }
    else if (index == 5)
    {
        state.key = "mod_matrix";
        state.defaultValue = "";
        state.hints = 0;
        state.label = "Mod Matrix";
    }
}

void Grist::setState(const char* key, const char* value)
{
    if (key == nullptr)
        return;

    // Output-only states (we still accept them from host silently).
    if (std::strcmp(key, "sample_status") == 0 || std::strcmp(key, "sample_error") == 0 || std::strcmp(key, "grains") == 0 || std::strcmp(key, "grains_active") == 0)
        return;

    if (std::strcmp(key, "mod_matrix") == 0)
    {
        // Non-RT: parse routing string into fixed arrays.
        // Format: target:slot:source:amount;...
        modMatrix.clear();
        if (value == nullptr || value[0] == '\0')
            return;

        const char* p = value;
        while (*p)
        {
            // token up to ';'
            char tok[96];
            uint32_t ti = 0;
            while (*p && *p != ';' && ti + 1 < sizeof(tok)) tok[ti++] = *p++;
            tok[ti] = '\0';
            if (*p == ';') ++p;

            // split tok by ':'
            char* a = tok;
            char* b = std::strchr(a, ':');
            if (!b) continue;
            *b++ = '\0';
            char* c = std::strchr(b, ':');
            if (!c) continue;
            *c++ = '\0';
            char* d = std::strchr(c, ':');
            if (!d) continue;
            *d++ = '\0';

            const GristMod::Target tgt = GristMod::Matrix::parseTarget(a);
            const long slot = std::strtol(b, nullptr, 10);
            const GristMod::Source src = GristMod::Matrix::parseSource(c);
            const float amt = std::strtof(d, nullptr);

            if (slot < 0 || slot >= (long)GristMod::kMaxSlotsPerTarget) continue;
            modMatrix.slots[(uint32_t)tgt][(uint32_t)slot].source = src;
            modMatrix.slots[(uint32_t)tgt][(uint32_t)slot].amount = fclampf(amt, -1.0f, 1.0f);
        }
        return;
    }

    if (std::strcmp(key, "sample") != 0)
        return;

    if (value == nullptr || value[0] == '\0')
        return;

    lastSampleError.clear();

    bool ok = false;
    if (std::strcmp(value, "__DEFAULT__") == 0)
    {
        ok = loadDefaultSample();
    }
    else
    {
        ok = loadAudioFile(value);
    }

    if (ok)
    {
        // Push the resolved path back into the state so the UI (and host) have the real filename
        // even when the UI requests "__DEFAULT__".
        {
            std::lock_guard<std::mutex> lock(sampleMutex);
            if (sample)
                updateStateValue("sample", sample->path.c_str());
        }
        updateStateValue("sample_status", "ok");
        updateStateValue("sample_error", "");
    }
    else
    {
        updateStateValue("sample_status", "error");
        updateStateValue("sample_error", lastSampleError.empty() ? "Failed to load sample" : lastSampleError.c_str());
    }
}

void Grist::initParameter(uint32_t index, Parameter& parameter)
{
    parameter.hints = kParameterIsAutomatable;

    switch (index)
    {
    case kParamGain:
        parameter.name = "Gain";
        parameter.symbol = "gain";
        // Allow boosting above unity to make the instrument feel less quiet.
        // Note: values > 1.0 can clip depending on host levels.
        parameter.ranges.def = 1.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 2.0f;
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

    case kParamPitchEnvAmt:
        parameter.name = "Pitch Env";
        parameter.symbol = "pitch_env_amt";
        parameter.unit = "st";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -48.0f;
        parameter.ranges.max = 48.0f;
        break;

    case kParamPitchEnvDecayMs:
        parameter.name = "Pitch Decay";
        parameter.symbol = "pitch_env_decay_ms";
        parameter.unit = "ms";
        parameter.ranges.def = 120.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 5000.0f;
        break;

    case kParamAttackMs:
        parameter.name = "Attack";
        parameter.symbol = "attack_ms";
        parameter.unit = "ms";
        parameter.ranges.def = 5.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 2000.0f;
        break;

    case kParamReleaseMs:
        parameter.name = "Release";
        parameter.symbol = "release_ms";
        parameter.unit = "ms";
        parameter.ranges.def = 120.0f;
        parameter.ranges.min = 5.0f;
        parameter.ranges.max = 5000.0f;
        break;

    case kParamKillOnRetrig:
        parameter.name = "Kill Retrig";
        parameter.symbol = "kill_on_retrig";
        parameter.hints |= kParameterIsBoolean;
        parameter.ranges.def = 1.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        break;

    case kParamNewVoiceOnRetrig:
        parameter.name = "New Voice";
        parameter.symbol = "new_voice_on_retrig";
        parameter.hints |= kParameterIsBoolean;
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        break;

    case kParamLfo1RateHz:
        parameter.name = "LFO1 Rate";
        parameter.symbol = "lfo1_rate_hz";
        parameter.unit = "Hz";
        parameter.ranges.def = 0.25f;
        parameter.ranges.min = 0.01f;
        parameter.ranges.max = 20.0f;
        break;

    case kParamLfo1Shape:
        parameter.name = "LFO1 Shape";
        parameter.symbol = "lfo1_shape";
        parameter.hints |= kParameterIsInteger;
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 4.0f; // 0=sine,1=tri,2=saw,3=square,4=s&h
        break;

    case kParamLfo2RateHz:
        parameter.name = "LFO2 Rate";
        parameter.symbol = "lfo2_rate_hz";
        parameter.unit = "Hz";
        parameter.ranges.def = 0.10f;
        parameter.ranges.min = 0.01f;
        parameter.ranges.max = 20.0f;
        break;

    case kParamLfo2Shape:
        parameter.name = "LFO2 Shape";
        parameter.symbol = "lfo2_shape";
        parameter.hints |= kParameterIsInteger;
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 4.0f;
        break;

    case kParamX:
        parameter.name = "X";
        parameter.symbol = "x";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -1.0f;
        parameter.ranges.max = 1.0f;
        break;

    case kParamY:
        parameter.name = "Y";
        parameter.symbol = "y";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -1.0f;
        parameter.ranges.max = 1.0f;
        break;

    case kParamMacro1: case kParamMacro2: case kParamMacro3: case kParamMacro4:
    case kParamMacro5: case kParamMacro6: case kParamMacro7: case kParamMacro8:
    {
        const uint32_t m = index - kParamMacro1;
        static const char* names[8] = {"Macro 1","Macro 2","Macro 3","Macro 4","Macro 5","Macro 6","Macro 7","Macro 8"};
        static const char* syms[8]  = {"macro1","macro2","macro3","macro4","macro5","macro6","macro7","macro8"};
        parameter.name = names[m];
        parameter.symbol = syms[m];
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -1.0f;
        parameter.ranges.max = 1.0f;
        break;
    }
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
    case kParamPitchEnvAmt: return fPitchEnvAmt;
    case kParamPitchEnvDecayMs: return fPitchEnvDecayMs;
    case kParamAttackMs: return fAttackMs;
    case kParamReleaseMs: return fReleaseMs;
    case kParamKillOnRetrig: return fKillOnRetrig;
    case kParamNewVoiceOnRetrig: return fNewVoiceOnRetrig;

    case kParamLfo1RateHz: return fLfo1RateHz;
    case kParamLfo1Shape: return fLfo1Shape;
    case kParamLfo2RateHz: return fLfo2RateHz;
    case kParamLfo2Shape: return fLfo2Shape;
    case kParamX: return fX;
    case kParamY: return fY;
    case kParamMacro1: return fMacro[0];
    case kParamMacro2: return fMacro[1];
    case kParamMacro3: return fMacro[2];
    case kParamMacro4: return fMacro[3];
    case kParamMacro5: return fMacro[4];
    case kParamMacro6: return fMacro[5];
    case kParamMacro7: return fMacro[6];
    case kParamMacro8: return fMacro[7];

    default: return 0.0f;
    }
}

void Grist::setParameterValue(uint32_t index, float value)
{
    switch (index)
    {
    case kParamGain:
        fGain = fclampf(value, 0.0f, 2.0f);
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
    case kParamPitchEnvAmt:
        fPitchEnvAmt = fclampf(value, -48.0f, 48.0f);
        break;
    case kParamPitchEnvDecayMs:
        fPitchEnvDecayMs = fclampf(value, 0.0f, 5000.0f);
        break;
    case kParamAttackMs:
        fAttackMs = fclampf(value, 0.0f, 2000.0f);
        break;
    case kParamReleaseMs:
        fReleaseMs = fclampf(value, 5.0f, 5000.0f);
        break;
    case kParamKillOnRetrig:
        fKillOnRetrig = (value >= 0.5f) ? 1.0f : 0.0f;
        break;
    case kParamNewVoiceOnRetrig:
        fNewVoiceOnRetrig = (value >= 0.5f) ? 1.0f : 0.0f;
        break;

    case kParamLfo1RateHz:
        fLfo1RateHz = fclampf(value, 0.01f, 20.0f);
        break;
    case kParamLfo1Shape:
        fLfo1Shape = fclampf(std::round(value), 0.0f, 4.0f);
        break;
    case kParamLfo2RateHz:
        fLfo2RateHz = fclampf(value, 0.01f, 20.0f);
        break;
    case kParamLfo2Shape:
        fLfo2Shape = fclampf(std::round(value), 0.0f, 4.0f);
        break;

    case kParamX:
        fX = fclampf(value, -1.0f, 1.0f);
        break;
    case kParamY:
        fY = fclampf(value, -1.0f, 1.0f);
        break;

    case kParamMacro1: fMacro[0] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro2: fMacro[1] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro3: fMacro[2] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro4: fMacro[3] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro5: fMacro[4] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro6: fMacro[5] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro7: fMacro[6] = fclampf(value, -1.0f, 1.0f); break;
    case kParamMacro8: fMacro[7] = fclampf(value, -1.0f, 1.0f); break;
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
    return loadAudioFile(p.c_str());
}

static inline bool endsWithCaseInsensitive(const std::string& s, const char* suffix)
{
    const size_t sl = s.size();
    const size_t tl = std::strlen(suffix);
    if (tl == 0 || sl < tl) return false;
    for (size_t i = 0; i < tl; ++i)
    {
        const char a = (char)std::tolower((unsigned char)s[sl - tl + i]);
        const char b = (char)std::tolower((unsigned char)suffix[i]);
        if (a != b) return false;
    }
    return true;
}

bool Grist::loadAudioFile(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        lastSampleError = "Empty filename";
        return false;
    }

    const std::string p(path);

    if (endsWithCaseInsensitive(p, ".wav") || endsWithCaseInsensitive(p, ".wave"))
        return loadWavFile(path);

    if (endsWithCaseInsensitive(p, ".mp3"))
        return loadMp3File(path);

    lastSampleError = "Unsupported file type";
    return false;
}

bool Grist::loadWavFile(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        lastSampleError = "Empty filename";
        return false;
    }

    drwav wav;
    if (!drwav_init_file(&wav, path, nullptr))
    {
        lastSampleError = "Unable to open/decode WAV";
        return false;
    }

    const uint32_t ch = wav.channels;
    const uint32_t sr = wav.sampleRate;
    const uint64_t frames = wav.totalPCMFrameCount;
    if (ch < 1 || ch > 2)
    {
        drwav_uninit(&wav);
        lastSampleError = "Unsupported channel count";
        return false;
    }
    if (frames == 0)
    {
        drwav_uninit(&wav);
        lastSampleError = "Empty file";
        return false;
    }

    std::vector<float> interleaved;
    interleaved.resize((size_t)frames * ch);
    const uint64_t read = drwav_read_pcm_frames_f32(&wav, frames, interleaved.data());
    drwav_uninit(&wav);
    if (read == 0)
    {
        lastSampleError = "Read failed";
        return false;
    }

    std::shared_ptr<SampleData> s(new SampleData());
    s->L.resize((size_t)read);
    s->R.resize((size_t)read);
    s->sampleRate = sr;
    s->path = path ? path : "";

    if (ch == 1)
    {
        for (uint64_t i = 0; i < read; ++i)
        {
            const float v = interleaved[(size_t)i];
            s->L[(size_t)i] = v;
            s->R[(size_t)i] = v;
        }
    }
    else
    {
        for (uint64_t i = 0; i < read; ++i)
        {
            s->L[(size_t)i] = interleaved[(size_t)i * 2 + 0];
            s->R[(size_t)i] = interleaved[(size_t)i * 2 + 1];
        }
    }

    {
        const std::lock_guard<std::mutex> lock(sampleMutex);
        sample = s;
    }

    return true;
}

bool Grist::loadMp3File(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        lastSampleError = "Empty filename";
        return false;
    }

    drmp3 mp3;
    if (!drmp3_init_file(&mp3, path, nullptr))
    {
        lastSampleError = "Unable to open/decode MP3";
        return false;
    }

    const uint32_t ch = mp3.channels;
    const uint32_t sr = mp3.sampleRate;
    drmp3_uint64 frames = drmp3_get_pcm_frame_count(&mp3);
    if (ch < 1 || ch > 2)
    {
        drmp3_uninit(&mp3);
        lastSampleError = "Unsupported channel count";
        return false;
    }
    if (frames == 0 || frames == DRMP3_UINT64_MAX)
    {
        // fallback: just read until EOF, growing as needed
        frames = 0;
    }

    std::vector<float> interleaved;
    if (frames > 0)
        interleaved.resize((size_t)frames * ch);

    drmp3_uint64 readFrames = 0;
    if (frames > 0)
    {
        readFrames = drmp3_read_pcm_frames_f32(&mp3, frames, interleaved.data());
    }
    else
    {
        // Unknown length; read in chunks.
        const drmp3_uint64 chunk = 4096;
        std::vector<float> tmp;
        tmp.resize((size_t)chunk * ch);
        for (;;)
        {
            const drmp3_uint64 got = drmp3_read_pcm_frames_f32(&mp3, chunk, tmp.data());
            if (got == 0) break;
            const size_t old = interleaved.size();
            interleaved.resize(old + (size_t)got * ch);
            std::memcpy(interleaved.data() + old, tmp.data(), (size_t)got * ch * sizeof(float));
            readFrames += got;
        }
    }

    drmp3_uninit(&mp3);

    if (readFrames == 0)
    {
        lastSampleError = "Read failed";
        return false;
    }

    std::shared_ptr<SampleData> s(new SampleData());
    s->L.resize((size_t)readFrames);
    s->R.resize((size_t)readFrames);
    s->sampleRate = sr;
    s->path = path ? path : "";

    if (ch == 1)
    {
        for (drmp3_uint64 i = 0; i < readFrames; ++i)
        {
            const float v = interleaved[(size_t)i];
            s->L[(size_t)i] = v;
            s->R[(size_t)i] = v;
        }
    }
    else
    {
        for (drmp3_uint64 i = 0; i < readFrames; ++i)
        {
            s->L[(size_t)i] = interleaved[(size_t)i * 2 + 0];
            s->R[(size_t)i] = interleaved[(size_t)i * 2 + 1];
        }
    }

    {
        const std::lock_guard<std::mutex> lock(sampleMutex);
        sample = s;
    }

    return true;
}

void Grist::run(const float** /*inputs*/, float** outputs, uint32_t frames,
                const MidiEvent* midiEvents, uint32_t midiEventCount)
{
    float* outL = outputs[0];
    float* outR = outputs[1];

    auto lfoValue = [&](float phase, int shape, float& hold) -> float {
        // phase 0..1
        const float p = phase - std::floor(phase);
        switch (shape)
        {
        default:
        case 0: // sine
            return std::sin(6.283185307179586f * p);
        case 1: // triangle
            return 4.0f * std::fabs(p - 0.5f) - 1.0f;
        case 2: // saw
            return 2.0f * p - 1.0f;
        case 3: // square
            return p < 0.5f ? 1.0f : -1.0f;
        case 4: // s&h (sample at phase wrap)
            return hold;
        }
    };

    float lfo1Hold = 0.0f;
    float lfo2Hold = 0.0f;

    for (uint32_t i = 0; i < frames; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }

    // Grab sample snapshot (shared_ptr keeps data alive without holding lock)
    std::shared_ptr<const SampleData> s;
    {
        std::lock_guard<std::mutex> lock(sampleMutex);
        s = sample;
    }
    if (!s || s->L.empty() || s->R.empty())
        return;

    const size_t len = s->L.size();
    if (len < 2 || s->sampleRate == 0)
        return;

    // --- MIDI -> voice allocation ---
    // Policy: optionally re-use voice if same note is already active, else steal first inactive, else steal quietest.
    auto findVoiceForNote = [&](int note) -> int {
        for (uint32_t v = 0; v < kMaxVoices; ++v)
            if (voices[v].active && voices[v].note == note)
                return (int)v;
        return -1;
    };

    auto removeVoiceFromQueues = [&](int v) {
        for (uint32_t n = 0; n < 128; ++n)
            noteQueues[n].remove(v);
    };

    auto allocVoice = [&]() -> int {
        for (uint32_t v = 0; v < kMaxVoices; ++v)
            if (!voices[v].active)
                return (int)v;
        // steal the quietest envelope
        uint32_t best = 0;
        float bestEnv = voices[0].env;
        for (uint32_t v = 1; v < kMaxVoices; ++v)
        {
            if (voices[v].env < bestEnv)
            {
                bestEnv = voices[v].env;
                best = v;
            }
        }
        return (int)best;
    };

    for (uint32_t i = 0; i < midiEventCount; ++i)
    {
        const MidiEvent& ev = midiEvents[i];
        if (ev.size < 3) continue;
        const uint8_t st = ev.data[0] & 0xF0;
        const int note = (int)(ev.data[1] & 0x7F);
        const uint8_t vel = ev.data[2] & 0x7F;
        const bool isNoteOn  = (st == 0x90) && (vel > 0);
        const bool isNoteOff = (st == 0x80) || ((st == 0x90) && (vel == 0));

        if (isNoteOn)
        {
            int v = -1;
            if (fNewVoiceOnRetrig < 0.5f)
                v = findVoiceForNote(note);
            if (v < 0) v = allocVoice();

            // If we're stealing/reusing a voice, ensure it isn't still referenced by any note queue.
            removeVoiceFromQueues(v);

            Voice& voice = voices[(uint32_t)v];
            voice.active = true;
            voice.gate = true;
            voice.releasing = false;
            voice.note = note;
            voice.velocity = (float)vel / 127.0f;
            voice.env = 0.0f; // attack ramp
            voice.pitchEnv = fPitchEnvAmt;
            voice.samplesToNextGrain = 0.0;

            // optionally kill old grains in this voice on retrigger
            if (fKillOnRetrig >= 0.5f)
            {
                for (uint32_t g = 0; g < Voice::kMaxGrains; ++g)
                    voice.grains[g].active = false;
            }

            // Track this note-on so a later note-off can release the matching event.
            noteQueues[(uint32_t)note].push(v);
        }
        else if (isNoteOff)
        {
            int v = -1;
            if (noteQueues[(uint32_t)note].pop(v))
            {
                if (v >= 0 && v < (int)kMaxVoices)
                {
                    Voice& voice = voices[(uint32_t)v];
                    voice.gate = false;
                    voice.releasing = true;
                }
            }
            else
            {
                // fallback: release any currently-playing voice for this note
                v = findVoiceForNote(note);
                if (v >= 0)
                {
                    Voice& voice = voices[(uint32_t)v];
                    voice.gate = false;
                    voice.releasing = true;
                }
            }
        }
    }

    // --- shared grain constants ---
    // Note: grain size + density can be modulated; the block-level constants are only base values.
    const double density = std::max(0.0, (double)fDensity);

    const uint32_t attackSamples = (uint32_t)std::max(1.0, ((double)fAttackMs / 1000.0) * fSampleRate);
    const float attackInc = (fAttackMs <= 0.0f) ? 1.0f : (1.0f / (float)attackSamples);

    const uint32_t releaseSamples = (uint32_t)std::max(1.0, ((double)fReleaseMs / 1000.0) * fSampleRate);
    const float releaseDec = 1.0f / (float)releaseSamples;

    // per-note pitch envelope decay (semitones per sample)
    const uint32_t pitchDecaySamples = (uint32_t)std::max(1.0, ((double)fPitchEnvDecayMs / 1000.0) * fSampleRate);
    const float pitchStep = (fPitchEnvDecayMs <= 0.0f) ? 1e9f : (std::abs(fPitchEnvAmt) / (float)pitchDecaySamples);

    const double twoPi = 6.283185307179586;

    // --- render ---
    const float lfo1Inc = fLfo1RateHz / (float)fSampleRate;
    const float lfo2Inc = fLfo2RateHz / (float)fSampleRate;
    const int lfo1Shape = (int)fLfo1Shape;
    const int lfo2Shape = (int)fLfo2Shape;

    for (uint32_t i = 0; i < frames; ++i)
    {
        // advance LFOs
        const float prev1 = lfo1Phase;
        const float prev2 = lfo2Phase;
        lfo1Phase += lfo1Inc;
        lfo2Phase += lfo2Inc;
        if (lfo1Shape == 4 && (lfo1Phase >= 1.0f || lfo1Phase < prev1)) lfo1Hold = rngFloat01() * 2.0f - 1.0f;
        if (lfo2Shape == 4 && (lfo2Phase >= 1.0f || lfo2Phase < prev2)) lfo2Hold = rngFloat01() * 2.0f - 1.0f;
        if (lfo1Phase >= 1.0f) lfo1Phase -= std::floor(lfo1Phase);
        if (lfo2Phase >= 1.0f) lfo2Phase -= std::floor(lfo2Phase);

        const float lfo1 = lfoValue(lfo1Phase, lfo1Shape, lfo1Hold);
        const float lfo2 = lfoValue(lfo2Phase, lfo2Shape, lfo2Hold);

        float mixL = 0.0f;
        float mixR = 0.0f;

        for (uint32_t v = 0; v < kMaxVoices; ++v)
        {
            Voice& voice = voices[v];
            if (!voice.active)
                continue;

            // envelope
            if (voice.releasing)
            {
                voice.env -= releaseDec;
                if (voice.env <= 0.0f)
                {
                    voice.env = 0.0f;
                    voice.active = false;
                    voice.releasing = false;
                    // grains will be ignored once inactive
                    continue;
                }
            }
            else if (voice.gate)
            {
                // attack (simple linear ramp)
                if (voice.env < 1.0f)
                {
                    voice.env += attackInc;
                    if (voice.env > 1.0f) voice.env = 1.0f;
                }
            }

            // pitch envelope (decays toward 0 semitones)
            if (voice.pitchEnv > 0.0f)
            {
                voice.pitchEnv -= pitchStep;
                if (voice.pitchEnv < 0.0f) voice.pitchEnv = 0.0f;
            }
            else if (voice.pitchEnv < 0.0f)
            {
                voice.pitchEnv += pitchStep;
                if (voice.pitchEnv > 0.0f) voice.pitchEnv = 0.0f;
            }

            // spawn grains (only while gate held)
            if (voice.gate && density > 0.0)
            {
                voice.samplesToNextGrain -= 1.0;
                while (voice.samplesToNextGrain <= 0.0)
                {
                    // modulation helpers (per voice, uses current LFO values)
                    auto srcValue = [&](GristMod::Source src) -> float {
                        switch (src)
                        {
                        default:
                        case GristMod::Source::None: return 0.0f;
                        case GristMod::Source::LFO1: return lfo1;
                        case GristMod::Source::LFO2: return lfo2;
                        case GristMod::Source::Env1: return voice.env * 2.0f - 1.0f;
                        case GristMod::Source::Velocity: return voice.velocity * 2.0f - 1.0f;
                        case GristMod::Source::Keytrack: return fclampf((float)(voice.note - 60) / 24.0f, -1.0f, 1.0f);
                        case GristMod::Source::X: return fX;
                        case GristMod::Source::Y: return fY;
                        case GristMod::Source::Macro1: return fMacro[0];
                        case GristMod::Source::Macro2: return fMacro[1];
                        case GristMod::Source::Macro3: return fMacro[2];
                        case GristMod::Source::Macro4: return fMacro[3];
                        case GristMod::Source::Macro5: return fMacro[4];
                        case GristMod::Source::Macro6: return fMacro[5];
                        case GristMod::Source::Macro7: return fMacro[6];
                        case GristMod::Source::Macro8: return fMacro[7];
                        }
                    };

                    auto targetMod = [&](GristMod::Target tgt) -> float {
                        float m = 0.0f;
                        for (uint32_t si = 0; si < GristMod::kMaxSlotsPerTarget; ++si)
                        {
                            const auto& sl = modMatrix.slots[(uint32_t)tgt][si];
                            if (sl.source == GristMod::Source::None || sl.amount == 0.0f) continue;
                            m += srcValue(sl.source) * sl.amount;
                        }
                        return fclampf(m, -1.0f, 1.0f);
                    };

                    // find slot
                    int slot = -1;
                    for (uint32_t gi = 0; gi < Voice::kMaxGrains; ++gi)
                        if (!voice.grains[gi].active) { slot = (int)gi; break; }

                    if (slot >= 0)
                    {

                        // Apply modulation. Ranges are intentionally conservative.
                        float center = fPosition;
                        center += targetMod(GristMod::Target::Position) * 0.50f; // +/- 50% of position
                        center = fclampf(center, 0.0f, 1.0f);

                        float spray = fSpray;
                        spray += targetMod(GristMod::Target::Spray) * 0.50f; // +/- 50%
                        spray = fclampf(spray, 0.0f, 1.0f);

                        float pitchSt = fPitch;
                        pitchSt += targetMod(GristMod::Target::Pitch) * 12.0f; // +/- 12 st
                        pitchSt = fclampf(pitchSt, -48.0f, 48.0f);

                        // Grain size modulation affects per-grain duration
                        float gSizeMs = fGrainSizeMs;
                        gSizeMs += targetMod(GristMod::Target::GrainSizeMs) * 80.0f; // +/- 80 ms
                        gSizeMs = fclampf(gSizeMs, 5.0f, 500.0f);

                        const double gDurSec = (double)gSizeMs / 1000.0;
                        const uint32_t gDur = (uint32_t)std::max(8.0, gDurSec * (double)s->sampleRate);

                        const float rr = rngFloat01() * 2.0f - 1.0f; // -1..1
                        const float pos01 = fclampf(center + rr * spray, 0.0f, 1.0f);
                        const double start = (double)pos01 * (double)(len - 2);

                        const double noteMul = midiNoteToHz(voice.note) / midiNoteToHz(60);
                        const double pitchMul = std::pow(2.0, (double)pitchSt / 12.0);
                        const double srMul = (double)s->sampleRate / fSampleRate;
                        const double pitchEnvMul = std::pow(2.0, (double)voice.pitchEnv / 12.0);
                        const double baseInc = noteMul * pitchMul * pitchEnvMul * srMul;

                        const float rp = fRandomPitch;
                        const float rps = (rngFloat01() * 2.0f - 1.0f) * rp;
                        const double randPitchMul = std::pow(2.0, (double)rps / 12.0);

                        Grain& g = voice.grains[(uint32_t)slot];
                        g.active = true;
                        g.pos = start;
                        g.startPos = start;
                        g.inc = baseInc * randPitchMul;
                        g.age = 0;
                        g.dur = gDur;

                        // simple stereo spread tied to spray (0..1)
                        const float pan = (rngFloat01() * 2.0f - 1.0f) * spray; // -spray..spray
                        const float ang = (pan * 0.5f + 0.5f) * 1.57079632679f; // 0..pi/2
                        g.panL = std::cos(ang);
                        g.panR = std::sin(ang);

                        // viz: record normalized start position (best-effort)
                        if (vizEventCount < kVizMaxEvents)
                            vizEvents[vizEventCount++] = pos01;
                    }

                    // Density modulation affects scheduling.
                    float dens = fDensity;
                    dens += targetMod(GristMod::Target::Density) * (0.75f * fDensity); // +/- 75%
                    dens = fclampf(dens, 0.1f, 200.0f);
                    const double spg = (dens > 0.0f) ? (fSampleRate / (double)dens) : 1e30;

                    voice.samplesToNextGrain += spg;
                    if (voice.samplesToNextGrain > spg)
                        break;
                }
            }

            float accL = 0.0f;
            float accR = 0.0f;

            // render grains
            for (uint32_t gi = 0; gi < Voice::kMaxGrains; ++gi)
            {
                Grain& g = voice.grains[gi];
                if (!g.active) continue;

                if (g.age >= g.dur)
                {
                    g.active = false;
                    continue;
                }

                const size_t idx = (size_t)g.pos;
                if (idx + 1 >= len)
                {
                    g.active = false;
                    continue;
                }

                const float frac = (float)(g.pos - (double)idx);

                // cubic interpolation (Catmull-Rom)
                const size_t i0 = (idx > 0) ? (idx - 1) : idx;
                const size_t i1 = idx;
                const size_t i2 = (idx + 1 < len) ? (idx + 1) : idx;
                const size_t i3 = (idx + 2 < len) ? (idx + 2) : i2;

                const float l = catmullRom(s->L[i0], s->L[i1], s->L[i2], s->L[i3], frac);
                const float r = catmullRom(s->R[i0], s->R[i1], s->R[i2], s->R[i3], frac);

                const double phase = (g.dur > 1) ? ((double)g.age / (double)(g.dur - 1)) : 1.0;
                const float w = (float)(0.5 - 0.5 * std::cos(twoPi * phase));

                // size normalization: keep energy roughly stable as grain size changes
                const float norm = 1.0f / std::sqrt(std::max(1.0f, (float)g.dur));

                accL += l * w * norm * g.panL;
                accR += r * w * norm * g.panR;

                g.pos += g.inc;
                g.age += 1;
            }

            const float vAmp = fGain * voice.velocity * voice.env;
            mixL += accL * vAmp;
            mixR += accR * vAmp;
        }

        outL[i] = mixL;
        outR[i] = mixR;
    }

    // Publish grain viz to UI at ~30 Hz (best-effort).
    // - grains: comma-separated list of 0..1 floats (spawn markers)
    // - grains_active: semicolon-separated list of "start,end,age,amp,voice" quints
    //   start/end/age/amp are 0..1, voice is 0..15
    vizDecim += frames;
    const uint32_t vizInterval = (uint32_t)std::max(1.0, fSampleRate / 30.0);
    if (vizDecim >= vizInterval)
    {
        vizDecim = 0;

        if (vizEventCount > 0)
        {
            // publish to shared viz bus (works for CLAP)
            GristVizBus::instance().publishSpawn(vizEvents, vizEventCount);

            // also try state updates (works on some formats)
            char buf[1024];
            uint32_t pos = 0;
            for (uint32_t ei = 0; ei < vizEventCount && pos + 16 < sizeof(buf); ++ei)
            {
                const int n = std::snprintf(buf + pos, sizeof(buf) - pos, (ei == 0) ? "%.4f" : ",%.4f", vizEvents[ei]);
                if (n <= 0) break;
                pos += (uint32_t)n;
                if (pos >= sizeof(buf)) { pos = (uint32_t)sizeof(buf) - 1; break; }
            }
            buf[pos] = '\0';
            updateStateValue("grains", buf);
            vizEventCount = 0;
        }

        // active grains snapshot
        GristVizBus::Active act[GristVizBus::kMaxActive];
        uint32_t count = 0;
        constexpr uint32_t kMaxActiveSend = GristVizBus::kMaxActive;

        char abuf[1536];
        uint32_t apos = 0;

        for (uint32_t v = 0; v < kMaxVoices && count < kMaxActiveSend; ++v)
        {
            const Voice& voice = voices[v];
            if (!voice.active) continue;

            for (uint32_t gi = 0; gi < Voice::kMaxGrains && count < kMaxActiveSend; ++gi)
            {
                const Grain& g = voice.grains[gi];
                if (!g.active) continue;

                const double start = g.startPos;
                const double span = g.inc * (double)g.dur;
                const double end = start + span;

                const float start01 = (float)fclampf((float)(start / (double)(len - 1)), 0.0f, 1.0f);
                const float end01 = (float)fclampf((float)(end / (double)(len - 1)), 0.0f, 1.0f);
                const float age01 = (g.dur > 0) ? fclampf((float)g.age / (float)g.dur, 0.0f, 1.0f) : 1.0f;

                // window level at current age (0..1)
                const double phase = (g.dur > 1) ? ((double)g.age / (double)(g.dur - 1)) : 1.0;
                const float w = (float)(0.5 - 0.5 * std::cos(twoPi * phase));
                const float amp01 = fclampf(w * voice.env * voice.velocity, 0.0f, 1.0f);

                act[count] = { start01, end01, age01, amp01, v };

                // also encode as string state (best-effort)
                const int n = std::snprintf(abuf + apos, sizeof(abuf) - apos,
                                           (count == 0) ? "%.4f,%.4f,%.4f,%.4f,%u" : ";%.4f,%.4f,%.4f,%.4f,%u",
                                           start01, end01, age01, amp01, v);
                if (n > 0)
                {
                    apos += (uint32_t)n;
                    if (apos + 24 >= sizeof(abuf))
                        apos = 0; // give up on string if too big
                }

                ++count;
            }
        }

        if (count > 0)
            GristVizBus::instance().publishActive(act, count);

        if (apos > 0)
        {
            abuf[std::min<uint32_t>(apos, (uint32_t)sizeof(abuf) - 1)] = '\0';
            updateStateValue("grains_active", abuf);
        }
    }
}

Plugin* createPlugin() { return new Grist(); }

END_NAMESPACE_DISTRHO
