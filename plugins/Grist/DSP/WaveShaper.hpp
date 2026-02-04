/*
 * Transistor Distortion Plugin - Waveshaper
 * Asymmetric hard clipping with soft knee for transistor-style distortion
 */

#ifndef WAVESHAPER_HPP_INCLUDED
#define WAVESHAPER_HPP_INCLUDED

#include <cmath>
#include <algorithm>

class WaveShaper {
public:
    WaveShaper()
        : positiveThreshold(0.8f),
          negativeThreshold(-0.72f),
          kneeWidth(0.1f),
          drive(1.0f) {}

    void setDrive(float driveAmount) {
        // Drive scales from 1x to 25x gain
        drive = 1.0f + driveAmount * 24.0f;
    }

    float process(float input) {
        // Apply drive gain
        float x = input * drive;

        // Asymmetric soft-knee hard clipping
        float output;

        if (x > positiveThreshold - kneeWidth) {
            if (x < positiveThreshold + kneeWidth) {
                // Soft knee region (positive)
                float delta = x - (positiveThreshold - kneeWidth);
                output = positiveThreshold - kneeWidth + delta - (delta * delta) / (4.0f * kneeWidth);
            } else {
                // Hard clip positive
                output = positiveThreshold;
            }
        } else if (x < negativeThreshold + kneeWidth) {
            if (x > negativeThreshold - kneeWidth) {
                // Soft knee region (negative)
                float delta = x - (negativeThreshold + kneeWidth);
                output = negativeThreshold + kneeWidth + delta + (delta * delta) / (4.0f * kneeWidth);
            } else {
                // Hard clip negative
                output = negativeThreshold;
            }
        } else {
            // Linear region
            output = x;
        }

        // Normalize output to prevent DC offset from asymmetry
        // Offset correction: (pos + neg) / 2 = (0.8 - 0.72) / 2 = 0.04
        output -= 0.04f;

        // Scale back to roughly unity gain at clipping
        output /= positiveThreshold;

        return output;
    }

    // Process with additional even harmonic generation
    float processWithHarmonics(float input) {
        float x = input * drive;

        // Asymmetric clipping generates both odd and even harmonics
        // The asymmetry (different positive/negative thresholds) creates even harmonics
        // which are characteristic of single-transistor distortion stages

        float output = process(input);

        // Add subtle second harmonic for more "transistor" character
        // This simulates the inherent non-linearity of transistor transfer curves
        float secondHarmonic = 0.05f * x * std::abs(x);
        output += secondHarmonic * (drive / 25.0f); // Scale with drive

        // Final soft limit to catch any overshoots
        output = std::tanh(output);

        return output;
    }

private:
    float positiveThreshold;  // Positive clipping threshold (+0.8)
    float negativeThreshold;  // Negative clipping threshold (-0.72)
    float kneeWidth;          // Soft knee width
    float drive;              // Drive multiplier
};

#endif // WAVESHAPER_HPP_INCLUDED
