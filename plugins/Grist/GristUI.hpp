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
    void uiIdle() override;

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

    // New UI is knob-based; we keep sliders temporarily for params that haven't been migrated.

    enum class Tab : uint8_t { Perform = 0, XY };
    Tab tab = Tab::Perform;

    struct Knob {
        float x, y, r;
        uint32_t param;
        float minV, maxV;
        float defV;
        const char* label;
        const char* unit;
        bool bipolar;
        float value; // cached
    };

    // Buttons
    float btnX, btnY, btnW, btnH;      // reload
    float btn2X, btn2Y, btn2W, btn2H;  // load
    float tabX, tabY, tabW, tabH;

    char sampleLabel[120];

    // Knobs on PERFORM
    static constexpr uint32_t kNumMacroKnobs = 2;
    static constexpr uint32_t kNumHeroKnobs  = 5;
    static constexpr uint32_t kNumSmallKnobs = 6;

    Knob macro[kNumMacroKnobs];
    Knob hero[kNumHeroKnobs];
    Knob small[kNumSmallKnobs];

    int activeKnobGroup = -1; // 0=macro,1=hero,2=small
    int activeKnobIndex = -1;
    float knobDragStartY = 0.0f;
    float knobDragStartValue = 0.0f;

    // XY tab
    float xyX = 0.0f, xyY = 0.0f, xyW = 0.0f, xyH = 0.0f;
    bool xyActive = false;
    float xVal = 0.0f; // cached param (kParamX)
    float yVal = 0.0f; // cached param (kParamY)

    void initKnobs();
    void layoutPerform();
    void layoutXY();
    void setParamFromValue(uint32_t param, float v);
    bool hitTestKnob(float x, float y, int& outGroup, int& outIndex) const;
    void drawKnob(const Knob& k, bool active);
    void drawModSlotsForParam(uint32_t param, float x, float y);
    void drawTabButton(float x, float y, float w, float h, const char* label, bool on);

    // --- modulation UI (slot boxes) ---
    enum class ModSource : uint8_t {
        None = 0,
        LFO1,
        LFO2,
        Env1,
        Vel,
        Key,
        X,
        Y,
        M1, M2, M3, M4, M5, M6, M7, M8,
        COUNT
    };

    enum class ModTarget : uint8_t {
        Position = 0,
        GrainSize,
        Density,
        Spray,
        Pitch,
        COUNT
    };

    struct ModSlot { ModSource src = ModSource::None; float amt = 0.0f; };
    static constexpr uint32_t kSlotsPerTarget = 3;
    ModSlot mod[(uint32_t)ModTarget::COUNT][kSlotsPerTarget];

    int modDragTarget = -1; // ModTarget index
    int modDragSlot = -1;   // 0..2
    float modDragStartY = 0.0f;
    float modDragStartAmt = 0.0f;

    void initModDefaults();
    void pushModMatrixState();
    void parseModMatrixState(const char* value);
    const char* modSourceId(ModSource s) const;
    const char* modSourceLabel(ModSource s) const;
    ModSource nextModSource(ModSource s) const;
    bool sliderToModTarget(uint32_t param, ModTarget& tgt) const;
    bool hitTestModBox(float x, float y, int& outTarget, int& outSlot) const;

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

    // bus polling
    uint32_t lastSpawnSeq = 0;
    uint32_t lastActiveSeq = 0;

    struct ActiveGrain {
        float start01 = 0.0f;
        float end01 = 0.0f;
        float age01 = 0.0f; // 0 new -> 1 old
        float amp01 = 0.0f; // 0..1 visual amplitude
        int voice = 0;
    };

    static constexpr uint32_t kMaxActiveViz = 64;
    ActiveGrain activeGrains[kMaxActiveViz];
    uint32_t activeCount = 0;

    void layoutWaveArea();
    void rebuildWavePeaks();
    void parseGrainViz(const char* value);
    void parseActiveGrainViz(const char* value);

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristUI)
};

END_NAMESPACE_DISTRHO

#endif // GRIST_UI_HPP_INCLUDED
