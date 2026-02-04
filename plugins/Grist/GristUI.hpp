/*
 * Transistor Distortion Plugin - UI Header
 */

#ifndef TRANSISTOR_DISTORTION_UI_HPP_INCLUDED
#define TRANSISTOR_DISTORTION_UI_HPP_INCLUDED

#include "DistrhoUI.hpp"
#include "DistrhoPluginInfo.h"

START_NAMESPACE_DISTRHO

class GristUI : public UI {
public:
    GristUI();

protected:
    // DSP/Plugin Callbacks
    void parameterChanged(uint32_t index, float value) override;

    // Widget Callbacks
    void onNanoDisplay() override;
    bool onMouse(const MouseEvent& ev) override;
    bool onMotion(const MotionEvent& ev) override;

private:
    // Parameter values (mirrored from plugin)
    float fGate;
    float fInputGain;
    float fDrive;
    float fTone;
    float fResonance;
    float fEnvMod;
    float fLowCut;
    float fOutput;
    float fMix;

    // Slider interaction state
    struct Slider {
        float x, y;           // Top-left position
        float width, height;  // Slider card size
        float value;          // Current value (0-1 normalized)
        float minValue;       // Minimum parameter value
        float maxValue;       // Maximum parameter value
        const char* label;    // Display label
        const char* unit;     // Unit string
        uint32_t paramIndex;  // Parameter index
        bool isLogarithmic;   // Use log scaling for display
        bool isBipolar;       // True for parameters centered at 0
    };

    static constexpr int NUM_SLIDERS = 9;
    Slider sliders[NUM_SLIDERS];

    int activeSlider;      // Currently dragged slider (-1 if none)

    // Colors
    struct Colors {
        static constexpr float bgR = 0.1f, bgG = 0.11f, bgB = 0.1f;
        static constexpr float accentR = 0.92f, accentG = 0.55f, accentB = 0.14f;
        static constexpr float textR = 0.92f, textG = 0.9f, textB = 0.86f;
        static constexpr float dimTextR = 0.62f, dimTextG = 0.62f, dimTextB = 0.58f;
        static constexpr float panelR = 0.14f, panelG = 0.15f, panelB = 0.13f;
    };

    // Helper functions
    void initSliders();
    void drawSlider(const Slider& slider, bool isActive);
    void drawTitle();
    int getSliderAt(float x, float y);
    float sliderValueFromY(const Slider& slider, float y) const;
    float normalizeValue(float value, float min, float max);
    float denormalizeValue(float normalized, float min, float max);

    // Background image
    NanoImage fBackground;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristUI)
};

END_NAMESPACE_DISTRHO

#endif // TRANSISTOR_DISTORTION_UI_HPP_INCLUDED
