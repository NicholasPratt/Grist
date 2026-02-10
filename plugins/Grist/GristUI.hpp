/*
 * Grist — UI (simple sliders)
 */

#ifndef GRIST_UI_HPP_INCLUDED
#define GRIST_UI_HPP_INCLUDED

#include "DistrhoUI.hpp"
#include "DistrhoPluginInfo.h"

#include <vector>
#include <string>

START_NAMESPACE_DISTRHO

class GristUI : public UI {
public:
    GristUI();

protected:
    void parameterChanged(uint32_t index, float value) override;
    void stateChanged(const char* key, const char* value) override;

#if DISTRHO_UI_FILE_BROWSER
    void uiFileBrowserSelected(const char* filename) override;
#endif

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

    static constexpr uint32_t kNumSliders = 11;
    // Simple buttons
    float btnX, btnY, btnW, btnH;      // reload
    float btn2X, btn2Y, btn2W, btn2H;  // hint
    char sampleLabel[120];
    Slider sliders[kNumSliders];
    int active;

    void initSliders();
    int hitTest(float x, float y) const;
    float yToNorm(const Slider& s, float y) const;
    float normToValue(const Slider& s) const;
    void setParamFromNorm(uint32_t sliderIdx, float norm);

    // --- waveform + grain viz ---
    float waveX = 18.0f;
    float waveY = 72.0f;
    float waveW = 0.0f;
    float waveH = 110.0f;

    std::string samplePath;
    std::vector<float> waveMin; // per-column min
    std::vector<float> waveMax; // per-column max

    static constexpr uint32_t kMaxVizGrains = 64;
    float grainPos[kMaxVizGrains];
    uint32_t grainCount = 0;

    void layoutWaveArea();
    void rebuildWavePeaks();
    void parseGrainViz(const char* value);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristUI)
};

END_NAMESPACE_DISTRHO

#endif // GRIST_UI_HPP_INCLUDED
