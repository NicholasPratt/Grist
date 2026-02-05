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
    : Plugin(kParamCount, 0, 3), // params, programs, states
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
      fSampleRate(48000.0),
      gateOn(false),
      currentNote(60),
      currentVelocity(0.8f)
{
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
}

void Grist::setState(const char* key, const char* value)
{
    if (key == nullptr)
        return;

    // Output-only states (we still accept them from host silently).
    if (std::strcmp(key, "sample_status") == 0 || std::strcmp(key, "sample_error") == 0)
        return;

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
        ok = loadWavFile(value);
    }

    if (ok)
    {
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

void Grist::run(const float** /*inputs*/, float** outputs, uint32_t frames,
                const MidiEvent* midiEvents, uint32_t midiEventCount)
{
    float* outL = outputs[0];
    float* outR = outputs[1];

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
    const double grainDurSec = (double)fGrainSizeMs / 1000.0;
    const uint32_t grainDur = (uint32_t)std::max(8.0, grainDurSec * (double)s->sampleRate);

    const double density = std::max(0.0, (double)fDensity);
    const double samplesPerGrain = (density > 0.0) ? (fSampleRate / density) : 1e30;

    const uint32_t attackSamples = (uint32_t)std::max(1.0, ((double)fAttackMs / 1000.0) * fSampleRate);
    const float attackInc = (fAttackMs <= 0.0f) ? 1.0f : (1.0f / (float)attackSamples);

    const uint32_t releaseSamples = (uint32_t)std::max(1.0, ((double)fReleaseMs / 1000.0) * fSampleRate);
    const float releaseDec = 1.0f / (float)releaseSamples;

    // per-note pitch envelope decay (semitones per sample)
    const uint32_t pitchDecaySamples = (uint32_t)std::max(1.0, ((double)fPitchEnvDecayMs / 1000.0) * fSampleRate);
    const float pitchStep = (fPitchEnvDecayMs <= 0.0f) ? 1e9f : (std::abs(fPitchEnvAmt) / (float)pitchDecaySamples);

    const double twoPi = 6.283185307179586;

    // --- render ---
    for (uint32_t i = 0; i < frames; ++i)
    {
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
                    // find slot
                    int slot = -1;
                    for (uint32_t gi = 0; gi < Voice::kMaxGrains; ++gi)
                        if (!voice.grains[gi].active) { slot = (int)gi; break; }

                    if (slot >= 0)
                    {
                        const float center = fPosition;
                        const float spray = fSpray;
                        const float rr = rngFloat01() * 2.0f - 1.0f; // -1..1
                        const float pos01 = fclampf(center + rr * spray, 0.0f, 1.0f);
                        const double start = (double)pos01 * (double)(len - 2);

                        const double noteMul = midiNoteToHz(voice.note) / midiNoteToHz(60);
                        const double pitchMul = std::pow(2.0, (double)fPitch / 12.0);
                        const double srMul = (double)s->sampleRate / fSampleRate;
                        const double pitchEnvMul = std::pow(2.0, (double)voice.pitchEnv / 12.0);
                        const double baseInc = noteMul * pitchMul * pitchEnvMul * srMul;

                        const float rp = fRandomPitch;
                        const float rps = (rngFloat01() * 2.0f - 1.0f) * rp;
                        const double randPitchMul = std::pow(2.0, (double)rps / 12.0);

                        Grain& g = voice.grains[(uint32_t)slot];
                        g.active = true;
                        g.pos = start;
                        g.inc = baseInc * randPitchMul;
                        g.age = 0;
                        g.dur = grainDur;
                    }

                    voice.samplesToNextGrain += samplesPerGrain;
                    if (voice.samplesToNextGrain > samplesPerGrain)
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
                const float l = lerp(s->L[idx], s->L[idx + 1], frac);
                const float r = lerp(s->R[idx], s->R[idx + 1], frac);

                const double phase = (g.dur > 1) ? ((double)g.age / (double)(g.dur - 1)) : 1.0;
                const float w = (float)(0.5 - 0.5 * std::cos(twoPi * phase));

                accL += l * w;
                accR += r * w;

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
}

Plugin* createPlugin() { return new Grist(); }

END_NAMESPACE_DISTRHO
