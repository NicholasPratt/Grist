/*
 * Grist — UI (simple sliders)
 */

#include "GristUI.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static inline float fclampf(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

START_NAMESPACE_DISTRHO

GristUI::GristUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      active(-1),
      btnX(400.0f), btnY(14.0f), btnW(220.0f), btnH(28.0f),
      btn2X(18.0f), btn2Y(14.0f), btn2W(360.0f), btn2H(28.0f)
{
    std::snprintf(sampleLabel, sizeof(sampleLabel), "Sample path: ~/Documents/samples/grist.wav");
    loadSharedResources();
    initSliders();
}

void GristUI::initSliders()
{
    const float margin = 18.0f;
    const float gap = 10.0f;
    const float sliderW = (getWidth() - margin*2 - gap*(kNumSliders-1)) / (float)kNumSliders;
    const float sliderH = getHeight() - 80.0f;
    const float y = 52.0f;

    const struct { uint32_t p; float minV; float maxV; const char* label; const char* unit; bool bipolar; } defs[kNumSliders] = {
        { kParamGain, 0.0f, 1.0f, "Gain", "", false },
        { kParamGrainSizeMs, 5.0f, 250.0f, "Size", "ms", false },
        { kParamDensity, 1.0f, 80.0f, "Dens", "gr/s", false },
        { kParamPosition, 0.0f, 100.0f, "Pos", "%", false },
        { kParamSpray, 0.0f, 100.0f, "Spray", "%", false },
        { kParamPitch, -24.0f, 24.0f, "Pitch", "st", true },
        { kParamRandomPitch, 0.0f, 12.0f, "Rnd", "st", false },
    };

    for (uint32_t i = 0; i < kNumSliders; ++i)
    {
        const float x = margin + i*(sliderW + gap);
        sliders[i].x = x;
        sliders[i].y = y;
        sliders[i].w = sliderW;
        sliders[i].h = sliderH;
        sliders[i].param = defs[i].p;
        sliders[i].minV = defs[i].minV;
        sliders[i].maxV = defs[i].maxV;
        sliders[i].label = defs[i].label;
        sliders[i].unit = defs[i].unit;
        sliders[i].isBipolar = defs[i].bipolar;
        sliders[i].norm = 0.5f;
    }
}

void GristUI::parameterChanged(uint32_t index, float value)
{
    for (uint32_t i = 0; i < kNumSliders; ++i)
    {
        if (sliders[i].param != index) continue;
        const float minV = sliders[i].minV;
        const float maxV = sliders[i].maxV;
        float n = (value - minV) / (maxV - minV);
        sliders[i].norm = fclampf(n, 0.0f, 1.0f);
        repaint();
        return;
    }
}

void GristUI::stateChanged(const char* key, const char* value)
{
    if (key && std::strcmp(key, "sample") == 0)
    {
        if (value && value[0] != '\0')
        {
            const char* lastSlash = std::strrchr(value, '/');
            const char* name = lastSlash ? (lastSlash + 1) : value;
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Sample: %s", name);
        }
        else
        {
            std::snprintf(sampleLabel, sizeof(sampleLabel), "No sample loaded");
        }
        repaint();
    }
}

#if DISTRHO_UI_FILE_BROWSER
void GristUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr || filename[0] == '\0')
    {
        std::snprintf(sampleLabel, sizeof(sampleLabel), "Load cancelled");
        repaint();
        return;
    }

    // Send to DSP as a state (filename path)
    setState("sample", filename);
    // UI label will update once stateChanged is called back by host/UI sync, but update optimistically too.
    const char* lastSlash = std::strrchr(filename, '/');
    const char* name = lastSlash ? (lastSlash + 1) : filename;
    std::snprintf(sampleLabel, sizeof(sampleLabel), "Loading: %s", name);
    repaint();
}
#endif

int GristUI::hitTest(float x, float y) const
{
    for (uint32_t i = 0; i < kNumSliders; ++i)
    {
        const Slider& s = sliders[i];
        if (x >= s.x && x <= s.x + s.w && y >= s.y && y <= s.y + s.h)
            return (int)i;
    }
    return -1;
}

float GristUI::yToNorm(const Slider& s, float y) const
{
    // 0 at bottom, 1 at top
    float t = (s.y + s.h - y) / s.h;
    return fclampf(t, 0.0f, 1.0f);
}

float GristUI::normToValue(const Slider& s) const
{
    return s.minV + s.norm * (s.maxV - s.minV);
}

void GristUI::setParamFromNorm(uint32_t i, float norm)
{
    sliders[i].norm = fclampf(norm, 0.0f, 1.0f);
    const float v = normToValue(sliders[i]);
    setParameterValue(sliders[i].param, v);
}

bool GristUI::onMouse(const MouseEvent& ev)
{
    if (ev.button != 1)
        return false;

    const float mx = ev.pos.getX();
    const float my = ev.pos.getY();

    if (ev.press)
    {
        // Load button
        if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH)
        {
            // No dialogs: tell DSP to reload from default path via state value "__DEFAULT__"
            setState("sample", "__DEFAULT__");
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Reloading: ~/Documents/samples/grist.wav");
            repaint();
            return true;
        }

        active = hitTest(mx, my);
        if (active >= 0)
        {
            setParamFromNorm((uint32_t)active, yToNorm(sliders[active], my));
            repaint();
            return true;
        }
    }
    else
    {
        active = -1;
    }

    return false;
}

bool GristUI::onMotion(const MotionEvent& ev)
{
    if (active < 0)
        return false;

    setParamFromNorm((uint32_t)active, yToNorm(sliders[active], ev.pos.getY()));
    repaint();
    return true;
}

void GristUI::onNanoDisplay()
{
    const float W = getWidth();
    const float H = getHeight();

    // background
    beginPath();
    rect(0, 0, W, H);
    fillColor(0.08f, 0.08f, 0.09f);
    fill();

    // title
    fontSize(20.0f);
    fillColor(0.95f, 0.85f, 0.35f);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(18.0f, 24.0f, "GRIST", nullptr);

    fontSize(11.0f);
    fillColor(0.7f, 0.7f, 0.7f);
    text(18.0f, 40.0f, "CLAP synth v0.2 (sample playback WIP)", nullptr);

    // Reload button (no dialogs)
    beginPath();
    roundedRect(btnX, btnY, btnW, btnH, 6.0f);
    fillColor(0.18f, 0.18f, 0.2f);
    fill();
    strokeColor(0.35f, 0.35f, 0.4f);
    strokeWidth(1.0f);
    stroke();

    fontSize(12.0f);
    fillColor(0.9f, 0.9f, 0.9f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(btnX + btnW*0.5f, btnY + btnH*0.5f, "Reload grist.wav", nullptr);

    // Sample label
    fontSize(11.0f);
    fillColor(0.75f, 0.75f, 0.75f);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(18.0f, 52.0f, sampleLabel, nullptr);

    for (uint32_t i = 0; i < kNumSliders; ++i)
    {
        const Slider& s = sliders[i];
        // panel
        beginPath();
        roundedRect(s.x, s.y, s.w, s.h, 6.0f);
        fillColor(0.12f, 0.12f, 0.13f);
        fill();

        // track
        const float trackX = s.x + s.w*0.5f;
        const float top = s.y + 22.0f;
        const float bottom = s.y + s.h - 22.0f;
        beginPath();
        moveTo(trackX, top);
        lineTo(trackX, bottom);
        strokeColor(0.25f, 0.25f, 0.27f);
        strokeWidth(6.0f);
        stroke();

        // handle
        const float y = bottom - s.norm*(bottom-top);
        beginPath();
        circle(trackX, y, (int)i == active ? 9.0f : 7.0f);
        fillColor(0.95f, 0.85f, 0.35f);
        fill();

        // label
        fontSize(12.0f);
        fillColor(0.9f, 0.9f, 0.9f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(s.x + s.w*0.5f, s.y + 12.0f, s.label, nullptr);

        // value
        char buf[32];
        const float v = normToValue(s);
        if (s.unit && s.unit[0] != '\0')
            std::snprintf(buf, sizeof(buf), "%.1f %s", v, s.unit);
        else
            std::snprintf(buf, sizeof(buf), "%.2f", v);
        fontSize(10.0f);
        fillColor(0.75f, 0.75f, 0.75f);
        text(s.x + s.w*0.5f, s.y + s.h - 10.0f, buf, nullptr);
    }
}

UI* createUI() { return new GristUI(); }

END_NAMESPACE_DISTRHO
