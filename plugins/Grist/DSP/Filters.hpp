/*
 * Transistor Distortion Plugin - Biquad Filters
 */

#ifndef FILTERS_HPP_INCLUDED
#define FILTERS_HPP_INCLUDED

#include <cmath>

class BiquadFilter {
public:
    BiquadFilter()
        : b0(1.0f), b1(0.0f), b2(0.0f), a1(0.0f), a2(0.0f),
          z1(0.0f), z2(0.0f) {}

    void reset() {
        z1 = z2 = 0.0f;
    }

    void setHighPass(float freq, float Q, float sampleRate) {
        const float w0 = 2.0f * M_PI * freq / sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw0) / 2.0f) / a0;
        b1 = (-(1.0f + cosw0)) / a0;
        b2 = ((1.0f + cosw0) / 2.0f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass(float freq, float Q, float sampleRate) {
        const float w0 = 2.0f * M_PI * freq / sampleRate;
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

    void setHighShelf(float freq, float gain, float Q, float sampleRate) {
        const float A = std::sqrt(gain);
        const float w0 = 2.0f * M_PI * freq / sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha;
        b0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha)) / a0;
        b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
        b2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha)) / a0;
        a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha) / a0;
    }

    void setLowShelf(float freq, float gain, float Q, float sampleRate) {
        const float A = std::sqrt(gain);
        const float w0 = 2.0f * M_PI * freq / sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        const float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha;
        b0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt(A) * alpha)) / a0;
        b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
        b2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha)) / a0;
        a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
        a2 = ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt(A) * alpha) / a0;
    }

    float process(float input) {
        // Transposed direct form II
        const float out = b0 * input + z1;
        z1 = b1 * input - a1 * out + z2;
        z2 = b2 * input - a2 * out;
        return out;
    }

private:
    float b0, b1, b2, a1, a2;
    float z1, z2;
};

class OnePoleFilter {
public:
    OnePoleFilter() : a0(1.0f), b1(0.0f), z1(0.0f) {}

    void reset() {
        z1 = 0.0f;
    }

    void setLowPass(float freq, float sampleRate) {
        const float fc = freq / sampleRate;
        b1 = std::exp(-2.0f * M_PI * fc);
        a0 = 1.0f - b1;
    }

    void setHighPass(float freq, float sampleRate) {
        const float fc = freq / sampleRate;
        b1 = std::exp(-2.0f * M_PI * fc);
        a0 = (1.0f + b1) / 2.0f;
    }

    float processLP(float input) {
        z1 = a0 * input + b1 * z1;
        return z1;
    }

    float processHP(float input) {
        const float lp = a0 * input + b1 * z1;
        z1 = lp;
        return input - lp;
    }

private:
    float a0, b1;
    float z1;
};

#endif // FILTERS_HPP_INCLUDED
