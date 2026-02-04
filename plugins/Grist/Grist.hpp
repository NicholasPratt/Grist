/*
 * Transistor Distortion Plugin
 * Guitar distortion with transistor-style hard clipping
 */

#ifndef TRANSISTOR_DISTORTION_HPP_INCLUDED
#define TRANSISTOR_DISTORTION_HPP_INCLUDED

#include "DistrhoPlugin.hpp"
#include "DSP/Filters.hpp"
#include "DSP/WaveShaper.hpp"
#include "DSP/Oversampler.hpp"

START_NAMESPACE_DISTRHO

class Grist : public Plugin {
public:
    Grist();

protected:
    // Plugin info
    const char* getLabel() const override { return "Grist"; }
    const char* getDescription() const override {
        return "Transistor-style guitar distortion with asymmetric hard clipping";
    }
    const char* getMaker() const override { return "TransistorFX"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('T', 'r', 'D', 's'); }

    // Init
    void initParameter(uint32_t index, Parameter& parameter) override;

    // Internal data
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    // Audio processing
    void activate() override;
    void deactivate() override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;
    void sampleRateChanged(double newSampleRate) override;

private:
    // Parameters
    float fGate;        // -80 to 0 dB threshold
    float fInputGain;   // 0-20 dB
    float fDrive;       // 0-100%
    float fTone;        // 1000-8000 Hz
    float fResonance;   // 0-100%
    float fEnvMod;      // -100 to +100% envelope modulation
    float fLowCut;      // 20-500 Hz
    float fOutput;      // -24 to +6 dB
    float fMix;         // 0-100%

    // Noise gate state
    float gateEnvelope;      // Envelope follower
    float gateAttackCoef;    // Attack coefficient
    float gateReleaseCoef;   // Release coefficient

    // Tone envelope modulation state
    float toneEnvelope;      // Envelope follower for tone modulation
    float toneEnvAttack;     // Fast attack for transients
    float toneEnvRelease;    // Slower release

    // DSP components
    BiquadFilter inputHighPass;      // Variable low cut
    BiquadFilter preEmphasis;        // Pre-distortion high boost
    BiquadFilter toneFilter;         // Variable LP for tone control
    BiquadFilter highBoost;          // Post-distortion presence
    BiquadFilter outputHighPass;     // Fixed 200Hz HP to kill sub-bass
    OnePoleFilter dcBlocker;         // DC blocking filter

    WaveShaper waveShaper;
    Oversampler4x oversampler;

    // State
    double fSampleRate;

    // Helper functions
    void updateFilters();
    float dbToLinear(float db) const { return std::pow(10.0f, db / 20.0f); }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Grist)
};

END_NAMESPACE_DISTRHO

#endif // TRANSISTOR_DISTORTION_HPP_INCLUDED
