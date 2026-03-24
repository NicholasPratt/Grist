/*
 * Grist — Granular Sample Synth (DPF)
 *
 * v0.2: WAV sample load + simple sample playback (MIDI in → audio out)
 * Next: granular engine (WIP)
 */

#ifndef GRIST_HPP_INCLUDED
#define GRIST_HPP_INCLUDED

#include "DistrhoPlugin.hpp"

#include <vector>
#include <mutex>
#include <string>
#include <memory>

#include "ModMatrix.hpp"

START_NAMESPACE_DISTRHO

class Grist : public Plugin {
public:
    Grist();

protected:
    // Plugin info
    const char* getLabel() const override { return "Grist"; }
    const char* getDescription() const override { return "Granular sample synth (WIP)"; }
    const char* getMaker() const override { return "ArchieAudio"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override { return d_cconst('G','r','i','s'); }

    void initParameter(uint32_t index, Parameter& parameter) override;
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    void initState(uint32_t index, State& state) override;
    void setState(const char* key, const char* value) override;

    void activate() override;
    void sampleRateChanged(double newSampleRate) override;

    // MIDI-capable run signature
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override;

private:
    // Parameters
    float fGain;
    float fGrainSizeMs;
    float fDensity;
    float fPosition;
    float fSpray;
    float fPitch;       // semitone offset
    float fRandomPitch;
    float fPitchEnvAmt;       // semitones (+/-)
    float fPitchEnvDecayMs;   // ms
    float fAttackMs;
    float fReleaseMs;
    float fKillOnRetrig;        // 0/1 (DPF doesn't have bool params everywhere)
    float fNewVoiceOnRetrig;    // 0/1
    float fLatch;               // 0/1 — hold notes until re-triggered

    bool latchedNotes[128];     // true = this note is currently held by latch

    // Modulation source parameters
    float fLfo1RateHz;
    float fLfo1Shape;
    float fLfo1Amp;
    float fLfo2RateHz;
    float fLfo2Shape;
    float fLfo2Amp;
    float fKeyModScale;    // 0..1 (maps to ~1%..50% position change per semitone when using Key→Position)
    float fFilterCutoff;   // Hz, 20..20000
    float fFilterRes;      // 0..1
    float fPitchLock;      // 0/1
    float fFilterType;     // 0=HPF, 1=LPF

    // Reverb
    float fRevMix;     // 0..1
    float fRevLength;  // 0..1 (maps to delay scaling)
    float fRevHPF;     // Hz

    // Stereo biquad filter state (used for filter + reverb HPF)
    float fBqX1L, fBqX2L, fBqY1L, fBqY2L;
    float fBqX1R, fBqX2R, fBqY1R, fBqY2R;

    float fRevHpX1L, fRevHpX2L, fRevHpY1L, fRevHpY2L;
    float fRevHpX1R, fRevHpX2R, fRevHpY1R, fRevHpY2R;
    float fRevHpB0 = 1.0f, fRevHpB1 = 0.0f, fRevHpB2 = 0.0f, fRevHpA1 = 0.0f, fRevHpA2 = 0.0f;

    // Simple Schroeder-ish reverb state
    struct Comb {
        std::vector<float> buf;
        uint32_t idx = 0;
        float fb = 0.7f;
        float damp = 0.2f;
        float lp = 0.0f;
    };
    struct Allpass {
        std::vector<float> buf;
        uint32_t idx = 0;
        float fb = 0.5f;
    };

    Comb revCombL[4];
    Comb revCombR[4];
    Allpass revApL[2];
    Allpass revApR[2];

    void reverbInit();
    void reverbUpdate();
    float reverbProcessComb(Comb& c, float x);
    float reverbProcessAllpass(Allpass& a, float x);

    float fX;
    float fY;

    float fMacro[8];

    // Sample range selection (normalized 0..1)
    float fSampleStart01;
    float fSampleEnd01;

    // Runtime
    double fSampleRate;
    bool gateOn;
    int currentNote;
    float currentVelocity; // 0..1

    // Sample preview (audition)
    bool previewActive = false;
    double previewPos = 0.0;     // sample frames (fractional)
    double previewEnd = 0.0;     // sample frames
    double previewInc = 1.0;     // per output sample
    float previewGain = 0.35f;
    uint32_t previewDecim = 0;

    struct SampleData {
        std::vector<float> L;
        std::vector<float> R;
        uint32_t sampleRate = 0;
        std::string path;
    };

    std::mutex sampleMutex;
    std::shared_ptr<const SampleData> sample; // swapped on load; held by audio thread per block

    // Grain engine (simple first pass)
    struct Grain {
        bool active = false;
        double pos = 0.0;        // current sample index (fractional)
        double startPos = 0.0;   // start sample index (fractional)
        double inc = 1.0;        // playback increment per output sample
        uint32_t age = 0;        // samples rendered
        uint32_t dur = 0;        // duration in output samples
        float panL = 1.0f;       // simple per-grain pan gains
        float panR = 1.0f;
    };

    // Polyphonic voices
    struct Voice {
        bool active = false;
        bool gate = false;
        bool releasing = false;
        int note = 60;
        float velocity = 1.0f;

        // simple amp envelope (0..1)
        float env = 0.0f;

        // per-note pitch envelope (semitones, decays toward 0)
        float pitchEnv = 0.0f;

        // per-voice grain scheduling
        static constexpr uint32_t kMaxGrains = 16;
        Grain grains[kMaxGrains];
        double samplesToNextGrain = 0.0;
    };

    static constexpr uint32_t kMaxVoices = 16;
    Voice voices[kMaxVoices];

    // Per-midi-note voice queues (for New Voice mode note-off matching)
    struct NoteQueue {
        int buf[kMaxVoices];
        uint32_t head = 0;
        uint32_t tail = 0;
        uint32_t count = 0;

        void clear() { head = tail = count = 0; }
        bool push(int v) {
            if (count >= kMaxVoices) return false;
            buf[tail] = v;
            tail = (tail + 1) % kMaxVoices;
            ++count;
            return true;
        }
        bool pop(int& v) {
            if (count == 0) return false;
            v = buf[head];
            head = (head + 1) % kMaxVoices;
            --count;
            return true;
        }
        bool remove(int v) {
            if (count == 0) return false;
            int tmp[kMaxVoices];
            uint32_t n = 0;
            bool removed = false;
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t idx = (head + i) % kMaxVoices;
                const int cur = buf[idx];
                if (!removed && cur == v) { removed = true; continue; }
                tmp[n++] = cur;
            }
            head = 0;
            tail = n % kMaxVoices;
            count = n;
            for (uint32_t i = 0; i < n; ++i) buf[i] = tmp[i];
            return removed;
        }
    };

    NoteQueue noteQueues[128];

    uint32_t rngState = 0x12345678u;

    // Modulation
    GristMod::Matrix modMatrix;
    float lfo1Phase = 0.0f;
    float lfo2Phase = 0.0f;

    // --- UI visualization (throttled, best-effort) ---
    // We push normalized grain start positions (0..1) whenever a grain spawns,
    // and occasionally publish them via an output-only state for the UI.
    static constexpr uint32_t kVizMaxEvents = 64;
    float vizEvents[kVizMaxEvents];
    uint32_t vizEventCount = 0;
    uint32_t vizDecim = 0;

    double midiNoteToHz(int note) const;
    bool loadAudioFile(const char* path);
    bool loadWavFile(const char* path);
    bool loadMp3File(const char* path);
    bool loadDefaultSample();

    // Non-RT load diagnostics (used to report failures to UI)
    std::string lastSampleError;

    // Random helpers
    inline uint32_t rngU32();
    inline float rngFloat01();

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Grist)
};

END_NAMESPACE_DISTRHO

#endif // GRIST_HPP_INCLUDED
