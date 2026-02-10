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

    // Runtime
    double fSampleRate;
    bool gateOn;
    int currentNote;
    float currentVelocity; // 0..1

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
        double pos = 0.0;     // sample index (fractional)
        double inc = 1.0;     // playback increment per output sample
        uint32_t age = 0;     // samples rendered
        uint32_t dur = 0;     // duration in samples
        float panL = 1.0f;    // simple per-grain pan gains
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

    // --- UI visualization (throttled, best-effort) ---
    // We push normalized grain start positions (0..1) whenever a grain spawns,
    // and occasionally publish them via an output-only state for the UI.
    static constexpr uint32_t kVizMaxEvents = 64;
    float vizEvents[kVizMaxEvents];
    uint32_t vizEventCount = 0;
    uint32_t vizDecim = 0;

    double midiNoteToHz(int note) const;
    bool loadWavFile(const char* path);
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
