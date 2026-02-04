/*
 * Grist — UI (simple sliders)
 */

#ifndef GRIST_UI_HPP_INCLUDED
#define GRIST_UI_HPP_INCLUDED

#include "DistrhoUI.hpp"
#include "DistrhoPluginInfo.h"

START_NAMESPACE_DISTRHO

class GristUI : public UI {
public:
    GristUI();

protected:
    void parameterChanged(uint32_t index, float value) override;

    void onNanoDisplay() override;
    bool onMouse(const MouseEvent& ev) override;
    bool onMotion(const MotionEvent& ev) override;

private:
    struct Slider {
        float x, y, w, h;
        uint32_t param;
        float minV, maxV;
        float norm; // 0..1
        const char* label;
        const char* unit;
        bool isBipolar;
    };

    static constexpr uint32_t kNumSliders = 7;
    Slider sliders[kNumSliders];
    int active;

    void initSliders();
    int hitTest(float x, float y) const;
    float yToNorm(const Slider& s, float y) const;
    float normToValue(const Slider& s) const;
    void setParamFromNorm(uint32_t sliderIdx, float norm);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristUI)
};

END_NAMESPACE_DISTRHO

#endif // GRIST_UI_HPP_INCLUDED
