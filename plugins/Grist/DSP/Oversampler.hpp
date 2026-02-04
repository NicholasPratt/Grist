/*
 * Transistor Distortion Plugin - 4x Oversampler
 * Uses cascaded half-band filters for anti-aliasing
 */

#ifndef OVERSAMPLER_HPP_INCLUDED
#define OVERSAMPLER_HPP_INCLUDED

#include <cmath>
#include <array>

// Simple 2-pole IIR lowpass for half-band filtering
class HalfBandFilter {
public:
    HalfBandFilter() {
        reset();
        // Butterworth LP at 0.25 * Fs (half-band)
        // Pre-computed coefficients for normalized frequency
        computeCoefficients();
    }

    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
    }

    void computeCoefficients() {
        // 2-pole Butterworth at fc = 0.25 (quarter of sample rate for 2x oversampling)
        const float fc = 0.25f;
        const float Q = 0.7071f; // Butterworth Q
        const float w0 = 2.0f * M_PI * fc;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = 1.0f + alpha;
        b0 = ((1.0f - cosw0) / 2.0f) / a0;
        b1 = (1.0f - cosw0) / a0;
        b2 = ((1.0f - cosw0) / 2.0f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float process(float input) {
        float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        return output;
    }

private:
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
};

class Oversampler4x {
public:
    static constexpr int FACTOR = 4;

    Oversampler4x() {
        reset();
    }

    void reset() {
        // Reset all filter stages
        for (int i = 0; i < 2; ++i) {
            upsampleFilters1[i].reset();
            upsampleFilters2[i].reset();
            downsampleFilters1[i].reset();
            downsampleFilters2[i].reset();
        }
    }

    // Upsample a single input sample to 4 output samples
    void upsample(float input, float* output) {
        // Stage 1: 1x -> 2x
        float stage1[2];
        stage1[0] = upsampleFilters1[0].process(input * 2.0f);
        stage1[1] = upsampleFilters1[0].process(0.0f);

        // Apply second filter for better stopband
        stage1[0] = upsampleFilters1[1].process(stage1[0]);
        stage1[1] = upsampleFilters1[1].process(stage1[1]);

        // Stage 2: 2x -> 4x
        output[0] = upsampleFilters2[0].process(stage1[0] * 2.0f);
        output[1] = upsampleFilters2[0].process(0.0f);
        output[2] = upsampleFilters2[0].process(stage1[1] * 2.0f);
        output[3] = upsampleFilters2[0].process(0.0f);

        // Second filter stage
        output[0] = upsampleFilters2[1].process(output[0]);
        output[1] = upsampleFilters2[1].process(output[1]);
        output[2] = upsampleFilters2[1].process(output[2]);
        output[3] = upsampleFilters2[1].process(output[3]);
    }

    // Downsample 4 input samples to a single output sample
    float downsample(const float* input) {
        // Stage 1: 4x -> 2x (process all 4 samples, keep every other)
        float stage1[2];

        // Filter and decimate
        float filtered[4];
        filtered[0] = downsampleFilters2[0].process(input[0]);
        filtered[1] = downsampleFilters2[0].process(input[1]);
        filtered[2] = downsampleFilters2[0].process(input[2]);
        filtered[3] = downsampleFilters2[0].process(input[3]);

        filtered[0] = downsampleFilters2[1].process(filtered[0]);
        filtered[1] = downsampleFilters2[1].process(filtered[1]);
        filtered[2] = downsampleFilters2[1].process(filtered[2]);
        filtered[3] = downsampleFilters2[1].process(filtered[3]);

        stage1[0] = filtered[1]; // Keep samples 1 and 3
        stage1[1] = filtered[3];

        // Stage 2: 2x -> 1x
        float out1 = downsampleFilters1[0].process(stage1[0]);
        float out2 = downsampleFilters1[0].process(stage1[1]);

        out1 = downsampleFilters1[1].process(out1);
        out2 = downsampleFilters1[1].process(out2);

        return out2; // Return final decimated sample
    }

private:
    // Two stages of 2x oversampling = 4x total
    // Each stage has 2 cascaded filters for steeper rolloff
    std::array<HalfBandFilter, 2> upsampleFilters1;   // 1x -> 2x
    std::array<HalfBandFilter, 2> upsampleFilters2;   // 2x -> 4x
    std::array<HalfBandFilter, 2> downsampleFilters1; // 2x -> 1x
    std::array<HalfBandFilter, 2> downsampleFilters2; // 4x -> 2x
};

#endif // OVERSAMPLER_HPP_INCLUDED
