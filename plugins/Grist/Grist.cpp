/*
 * Transistor Distortion Plugin - DSP Implementation
 */

#include "Grist.hpp"

START_NAMESPACE_DISTRHO

Grist::Grist()
    : Plugin(kParamCount, 0, 0), // parameters, programs, states
      fGate(-60.0f),
      fInputGain(0.0f),
      fDrive(50.0f),
      fTone(4000.0f),
      fResonance(20.0f),
      fEnvMod(0.0f),
      fLowCut(80.0f),
      fOutput(0.0f),
      fMix(100.0f),
      gateEnvelope(0.0f),
      gateAttackCoef(0.0f),
      gateReleaseCoef(0.0f),
      toneEnvelope(0.0f),
      toneEnvAttack(0.0f),
      toneEnvRelease(0.0f),
      fSampleRate(48000.0)
{
    updateFilters();
}

void Grist::initParameter(uint32_t index, Parameter& parameter)
{
    switch (index)
    {
    case kParamGate:
        parameter.name = "Gate";
        parameter.symbol = "gate";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "dB";
        parameter.ranges.def = -60.0f;
        parameter.ranges.min = -80.0f;
        parameter.ranges.max = 0.0f;
        break;

    case kParamInputGain:
        parameter.name = "Input Gain";
        parameter.symbol = "input_gain";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "dB";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 20.0f;
        break;

    case kParamDrive:
        parameter.name = "Drive";
        parameter.symbol = "drive";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "%";
        parameter.ranges.def = 50.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 100.0f;
        break;

    case kParamTone:
        parameter.name = "Tone";
        parameter.symbol = "tone";
        parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
        parameter.unit = "Hz";
        parameter.ranges.def = 4000.0f;
        parameter.ranges.min = 1000.0f;
        parameter.ranges.max = 8000.0f;
        break;

    case kParamResonance:
        parameter.name = "Resonance";
        parameter.symbol = "resonance";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "%";
        parameter.ranges.def = 20.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 100.0f;
        break;

    case kParamEnvMod:
        parameter.name = "Env Mod";
        parameter.symbol = "envmod";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "%";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -100.0f;
        parameter.ranges.max = 100.0f;
        break;

    case kParamLowCut:
        parameter.name = "Low Cut";
        parameter.symbol = "lowcut";
        parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
        parameter.unit = "Hz";
        parameter.ranges.def = 80.0f;
        parameter.ranges.min = 20.0f;
        parameter.ranges.max = 500.0f;
        break;

    case kParamOutput:
        parameter.name = "Output";
        parameter.symbol = "output";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "dB";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -24.0f;
        parameter.ranges.max = 12.0f;
        break;

    case kParamMix:
        parameter.name = "Mix";
        parameter.symbol = "mix";
        parameter.hints = kParameterIsAutomatable;
        parameter.unit = "%";
        parameter.ranges.def = 100.0f;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 100.0f;
        break;
    }
}

float Grist::getParameterValue(uint32_t index) const
{
    switch (index)
    {
    case kParamGate:
        return fGate;
    case kParamInputGain:
        return fInputGain;
    case kParamDrive:
        return fDrive;
    case kParamTone:
        return fTone;
    case kParamResonance:
        return fResonance;
    case kParamEnvMod:
        return fEnvMod;
    case kParamLowCut:
        return fLowCut;
    case kParamOutput:
        return fOutput;
    case kParamMix:
        return fMix;
    default:
        return 0.0f;
    }
}

void Grist::setParameterValue(uint32_t index, float value)
{
    switch (index)
    {
    case kParamGate:
        fGate = value;
        break;
    case kParamInputGain:
        fInputGain = value;
        break;
    case kParamDrive:
        fDrive = value;
        waveShaper.setDrive(value / 100.0f);
        break;
    case kParamTone:
        fTone = value;
        updateFilters();
        break;
    case kParamResonance:
        fResonance = value;
        updateFilters();
        break;
    case kParamEnvMod:
        fEnvMod = value;
        break;
    case kParamLowCut:
        fLowCut = value;
        updateFilters();
        break;
    case kParamOutput:
        fOutput = value;
        break;
    case kParamMix:
        fMix = value;
        break;
    }
}

void Grist::activate()
{
    // Reset filter states
    inputHighPass.reset();
    preEmphasis.reset();
    toneFilter.reset();
    highBoost.reset();
    outputHighPass.reset();
    dcBlocker.reset();
    oversampler.reset();

    // Reset gate envelope
    gateEnvelope = 0.0f;

    // Reset tone envelope
    toneEnvelope = 0.0f;
}

void Grist::deactivate()
{
    // Nothing to do
}

void Grist::sampleRateChanged(double newSampleRate)
{
    fSampleRate = newSampleRate;
    updateFilters();
}

void Grist::updateFilters()
{
    // Variable low cut (high-pass filter)
    inputHighPass.setHighPass(fLowCut, 0.7071f, fSampleRate);

    // Pre-emphasis: slight high boost before distortion
    preEmphasis.setHighShelf(2500.0f, 1.5f, 0.7071f, fSampleRate);

    // Tone control: resonant low-pass filter
    // Q ranges from 0.5 (no resonance) to 8.0 (high resonance)
    float Q = 0.5f + (fResonance / 100.0f) * 7.5f;
    toneFilter.setLowPass(fTone, Q, fSampleRate);

    // Post-distortion presence boost at 3kHz (subtle)
    highBoost.setHighShelf(3000.0f, 1.3f, 0.7071f, fSampleRate);

    // Fixed 200Hz high-pass at output to kill sub-bass buildup
    outputHighPass.setHighPass(200.0f, 0.7071f, fSampleRate);

    // DC blocker at 10Hz
    dcBlocker.setHighPass(10.0f, fSampleRate);

    // Gate envelope coefficients (fast attack ~1ms, slow release ~100ms)
    gateAttackCoef = std::exp(-1.0f / (0.001f * fSampleRate));
    gateReleaseCoef = std::exp(-1.0f / (0.1f * fSampleRate));

    // Tone envelope coefficients (very fast attack ~0.5ms for transients, moderate release ~50ms)
    toneEnvAttack = std::exp(-1.0f / (0.0005f * fSampleRate));
    toneEnvRelease = std::exp(-1.0f / (0.05f * fSampleRate));

    // Update waveshaper drive
    waveShaper.setDrive(fDrive / 100.0f);
}

void Grist::run(const float** inputs, float** outputs, uint32_t frames)
{
    const float* in = inputs[0];
    float* out = outputs[0];

    // Convert parameters to linear gains
    const float gateThreshold = dbToLinear(fGate);
    const float inputGainLin = dbToLinear(fInputGain);
    // Internal -36dB cut to ensure safe output levels
    const float internalCut = dbToLinear(-36.0f);
    const float outputGainLin = dbToLinear(fOutput) * internalCut;
    const float wetMix = fMix / 100.0f;
    const float dryMix = 1.0f - wetMix;

    // Envelope modulation parameters
    // fEnvMod ranges -100 to +100, normalize to -1 to +1
    const float envModAmount = fEnvMod / 100.0f;
    // Tone frequency range in log space for smooth modulation
    const float toneMinHz = 1000.0f;
    const float toneMaxHz = 8000.0f;
    const float logToneMin = std::log(toneMinHz);
    const float logToneMax = std::log(toneMaxHz);
    const float logToneBase = std::log(fTone);

    // Q for resonant filter
    const float Q = 0.5f + (fResonance / 100.0f) * 7.5f;

    // Buffer for oversampled processing
    float oversampledBuffer[Oversampler4x::FACTOR];

    for (uint32_t i = 0; i < frames; ++i)
    {
        // Store dry signal for mixing
        const float drySample = in[i];

        // === NOISE GATE ===
        // Envelope follower (peak detection)
        const float inputLevel = std::abs(drySample);
        if (inputLevel > gateEnvelope)
            gateEnvelope = gateAttackCoef * gateEnvelope + (1.0f - gateAttackCoef) * inputLevel;
        else
            gateEnvelope = gateReleaseCoef * gateEnvelope + (1.0f - gateReleaseCoef) * inputLevel;

        // Calculate gate gain (smooth transition)
        float gateGain;
        if (gateEnvelope < gateThreshold) {
            // Below threshold - calculate attenuation
            // Soft knee: gradually reduce gain as signal drops below threshold
            float ratio = gateEnvelope / (gateThreshold + 1e-10f);
            gateGain = ratio * ratio; // Quadratic for smooth rolloff
        } else {
            gateGain = 1.0f;
        }

        // === TONE ENVELOPE FOLLOWER ===
        // Track envelope for tone modulation (uses post-gate signal level)
        const float postGateLevel = std::abs(drySample * gateGain);
        if (postGateLevel > toneEnvelope)
            toneEnvelope = toneEnvAttack * toneEnvelope + (1.0f - toneEnvAttack) * postGateLevel;
        else
            toneEnvelope = toneEnvRelease * toneEnvelope + (1.0f - toneEnvRelease) * postGateLevel;

        // Modulate tone frequency based on envelope
        // Envelope normalized (typical guitar signal peaks ~0.5-1.0)
        // Clamp envelope to 0-1 range for modulation
        const float envNormalized = std::min(1.0f, toneEnvelope * 2.0f);

        // Calculate modulated tone frequency in log space
        // Positive envMod: transients push frequency UP (brighter on attack)
        // Negative envMod: transients push frequency DOWN (darker on attack)
        float logToneMod = logToneBase + envNormalized * envModAmount * (logToneMax - logToneMin) * 0.5f;
        logToneMod = std::max(logToneMin, std::min(logToneMax, logToneMod));
        const float modulatedTone = std::exp(logToneMod);

        // Update tone filter with modulated frequency
        toneFilter.setLowPass(modulatedTone, Q, fSampleRate);

        // Apply gate to the wet path input
        float sample = drySample * gateGain;

        // Input stage: HP filter (low cut) and gain
        sample = inputHighPass.process(sample);
        sample *= inputGainLin;

        // Pre-emphasis (high boost before clipping)
        sample = preEmphasis.process(sample);

        // Upsample to 4x
        oversampler.upsample(sample, oversampledBuffer);

        // Process each oversampled sample through waveshaper
        for (int j = 0; j < Oversampler4x::FACTOR; ++j)
        {
            oversampledBuffer[j] = waveShaper.processWithHarmonics(oversampledBuffer[j]);
        }

        // Downsample back to original rate
        sample = oversampler.downsample(oversampledBuffer);

        // Tone control (resonant low-pass with envelope modulation)
        sample = toneFilter.process(sample);

        // High frequency presence boost
        sample = highBoost.process(sample);

        // Kill sub-bass (fixed 200Hz HP)
        sample = outputHighPass.process(sample);

        // DC blocking
        sample = dcBlocker.processHP(sample);

        // Output gain (includes internal -36dB cut)
        sample *= outputGainLin;

        // Hard limit to prevent clipping in DAW
        if (sample > 1.0f) sample = 1.0f;
        else if (sample < -1.0f) sample = -1.0f;

        // Dry/wet mix (gate applied to dry so parallel blend stays quiet)
        out[i] = (sample * wetMix) + (drySample * dryMix * gateGain);
    }
}

Plugin* createPlugin()
{
    return new Grist();
}

END_NAMESPACE_DISTRHO
