/*
 * Grist — Granular Sample Synth (DPF)
 *
 * v1 placeholder DSP: sine synth (MIDI in → audio out)
 */

#ifndef GRIST_HPP_INCLUDED
#define GRIST_HPP_INCLUDED

#include "DistrhoPlugin.hpp"

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

    void activate() override;
    void sampleRateChanged(double newSampleRate) override;

    // MIDI-capable run signature
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override;

private:
    // Parameters (v1)
    float fGain;
    float fGrainSizeMs;
    float fDensity;
    float fPosition;
    float fSpray;
    float fPitch;       // semitone offset
    float fRandomPitch;

    // Sine placeholder synth state
    double fSampleRate;
    bool gateOn;
    int currentNote;
    float currentVelocity; // 0..1
    double phase;

    double midiNoteToHz(int note) const;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Grist)
};

END_NAMESPACE_DISTRHO

#endif // GRIST_HPP_INCLUDED
