/*
 * Transistor Distortion Plugin - UI Implementation
 */

#include "GristUI.hpp"
#include "BackgroundImage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

GristUI::GristUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      fGate(-60.0f),
      fInputGain(0.0f),
      fDrive(50.0f),
      fTone(4000.0f),
      fResonance(20.0f),
      fEnvMod(0.0f),
      fLowCut(80.0f),
      fOutput(0.0f),
      fMix(100.0f),
      activeSlider(-1)
{
    // Load shared resources (fonts)
    loadSharedResources();

    // Load background image from embedded data
    fBackground = createImageFromMemory(tDbg_png, tDbg_png_len, IMAGE_GENERATE_MIPMAPS);
    initSliders();
}

void GristUI::initSliders()
{
    const float sliderWidth = 88.0f;
    const float sliderHeight = 108.0f;
    const float topRowY = 72.0f;
    const float bottomRowY = 184.0f;

    // 5 columns for top row (including new ENV MOD), 4 columns for bottom row
    const float x1 = 26.0f;
    const float x2 = 126.0f;
    const float x3 = 226.0f;
    const float x4 = 326.0f;
    const float x5 = 426.0f;

    // Top row: Gate, Input, Drive, Tone, Env Mod
    sliders[0] = {x1, topRowY, sliderWidth, sliderHeight, 0.25f, -80.0f, 0.0f, "GATE", "dB", kParamGate, false, false};
    sliders[1] = {x2, topRowY, sliderWidth, sliderHeight, 0.0f, 0.0f, 20.0f, "INPUT", "dB", kParamInputGain, false, false};
    sliders[2] = {x3, topRowY, sliderWidth, sliderHeight, 0.5f, 0.0f, 100.0f, "DRIVE", "%", kParamDrive, false, false};
    sliders[3] = {x4, topRowY, sliderWidth, sliderHeight, 0.5f, 1000.0f, 8000.0f, "TONE", "Hz", kParamTone, true, false};
    sliders[4] = {x5, topRowY, sliderWidth, sliderHeight, 0.5f, -100.0f, 100.0f, "ENV MOD", "%", kParamEnvMod, false, true};

    // Bottom row: Reso, Low Cut, Output, Mix (centered)
    const float bx1 = 82.0f;
    const float bx2 = 194.0f;
    const float bx3 = 306.0f;
    const float bx4 = 418.0f;

    sliders[5] = {bx1, bottomRowY, sliderWidth, sliderHeight, 0.2f, 0.0f, 100.0f, "RESO", "%", kParamResonance, false, false};
    sliders[6] = {bx2, bottomRowY, sliderWidth, sliderHeight, 0.0f, 20.0f, 500.0f, "LOW CUT", "Hz", kParamLowCut, true, false};
    sliders[7] = {bx3, bottomRowY, sliderWidth, sliderHeight, 0.8f, -24.0f, 12.0f, "OUTPUT", "dB", kParamOutput, false, false};
    sliders[8] = {bx4, bottomRowY, sliderWidth, sliderHeight, 1.0f, 0.0f, 100.0f, "MIX", "%", kParamMix, false, false};

    sliders[0].value = normalizeValue(fGate, -80.0f, 0.0f);
    sliders[1].value = normalizeValue(fInputGain, 0.0f, 20.0f);
    sliders[2].value = normalizeValue(fDrive, 0.0f, 100.0f);
    sliders[3].value = (std::log(fTone / 1000.0f)) / (std::log(8000.0f / 1000.0f));
    sliders[4].value = normalizeValue(fEnvMod, -100.0f, 100.0f);
    sliders[5].value = normalizeValue(fResonance, 0.0f, 100.0f);
    sliders[6].value = (std::log(fLowCut / 20.0f)) / (std::log(500.0f / 20.0f));
    sliders[7].value = normalizeValue(fOutput, -24.0f, 12.0f);
    sliders[8].value = normalizeValue(fMix, 0.0f, 100.0f);
}

void GristUI::parameterChanged(uint32_t index, float value)
{
    switch (index)
    {
    case kParamGate:
        fGate = value;
        sliders[0].value = normalizeValue(value, -80.0f, 0.0f);
        break;
    case kParamInputGain:
        fInputGain = value;
        sliders[1].value = normalizeValue(value, 0.0f, 20.0f);
        break;
    case kParamDrive:
        fDrive = value;
        sliders[2].value = normalizeValue(value, 0.0f, 100.0f);
        break;
    case kParamTone:
        fTone = value;
        sliders[3].value = (std::log(value / 1000.0f)) / (std::log(8000.0f / 1000.0f));
        break;
    case kParamEnvMod:
        fEnvMod = value;
        sliders[4].value = normalizeValue(value, -100.0f, 100.0f);
        break;
    case kParamResonance:
        fResonance = value;
        sliders[5].value = normalizeValue(value, 0.0f, 100.0f);
        break;
    case kParamLowCut:
        fLowCut = value;
        sliders[6].value = (std::log(value / 20.0f)) / (std::log(500.0f / 20.0f));
        break;
    case kParamOutput:
        fOutput = value;
        sliders[7].value = normalizeValue(value, -24.0f, 12.0f);
        break;
    case kParamMix:
        fMix = value;
        sliders[8].value = normalizeValue(value, 0.0f, 100.0f);
        break;
    }
    repaint();
}

void GristUI::onNanoDisplay()
{
    const float width = getWidth();
    const float height = getHeight();

    // Draw background image scaled to fit
    if (fBackground.isValid())
    {
        const Paint bgPaint = imagePattern(0, 0, width, height, 0.0f, fBackground, 1.0f);
        beginPath();
        rect(0, 0, width, height);
        fillPaint(bgPaint);
        fill();
    }
    else
    {
        // Fallback gradient if image fails to load
        const Color bgTop(20, 22, 18);
        const Color bgBottom(35, 30, 24);
        const Paint bg = linearGradient(0.0f, 0.0f, 0.0f, height, bgTop, bgBottom);
        beginPath();
        rect(0, 0, width, height);
        fillPaint(bg);
        fill();
    }

    // Title is in background image, no need to draw it

    for (int i = 0; i < NUM_SLIDERS; ++i)
    {
        drawSlider(sliders[i], i == activeSlider);
    }
}

void GristUI::drawTitle()
{
    fontSize(22.0f);
    fillColor(Colors::accentR, Colors::accentG, Colors::accentB);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(24.0f, 24.0f, "TRANSISTOR DISTORTION", nullptr);

    fontSize(10.0f);
    fillColor(Colors::dimTextR, Colors::dimTextG, Colors::dimTextB);
    text(24.0f, 42.0f, "Gate-driven grit with resonant tone shaping", nullptr);
}

void GristUI::drawSlider(const Slider& slider, bool isActive)
{
    const float x = slider.x;
    const float y = slider.y;
    const float w = slider.width;
    const float h = slider.height;

    // Background image already has panels and labels - only draw interactive elements

    const float trackTop = y + 30.0f;
    const float trackBottom = y + h - 28.0f;
    const float trackX = x + w * 0.5f;
    const float trackHeight = trackBottom - trackTop;
    const float valueY = trackBottom - slider.value * trackHeight;

    // Draw track background
    beginPath();
    moveTo(trackX, trackTop);
    lineTo(trackX, trackBottom);
    strokeColor(0.15f, 0.15f, 0.12f);
    strokeWidth(6.0f);
    stroke();

    // Draw filled portion - from center for bipolar, from bottom for normal
    if (slider.isBipolar) {
        const float centerY = trackTop + trackHeight * 0.5f;
        // Draw center line marker
        beginPath();
        moveTo(trackX - 8.0f, centerY);
        lineTo(trackX + 8.0f, centerY);
        strokeColor(0.35f, 0.35f, 0.3f);
        strokeWidth(1.5f);
        stroke();
        // Draw from center to value
        beginPath();
        moveTo(trackX, centerY);
        lineTo(trackX, valueY);
        strokeColor(Colors::accentR, Colors::accentG, Colors::accentB);
        strokeWidth(6.0f);
        stroke();
    } else {
        beginPath();
        moveTo(trackX, trackBottom);
        lineTo(trackX, valueY);
        strokeColor(Colors::accentR, Colors::accentG, Colors::accentB);
        strokeWidth(6.0f);
        stroke();
    }

    // Draw handle
    beginPath();
    circle(trackX, valueY, isActive ? 10.0f : 8.0f);
    fillColor(Colors::accentR, Colors::accentG, Colors::accentB);
    fill();

    beginPath();
    circle(trackX, valueY, isActive ? 10.0f : 8.0f);
    strokeColor(0.05f, 0.05f, 0.03f);
    strokeWidth(1.5f);
    stroke();

    // Format value string
    char valueStr[32];
    float displayValue;

    if (slider.isLogarithmic) {
        displayValue = slider.minValue * std::pow(slider.maxValue / slider.minValue, slider.value);
        if (displayValue >= 1000.0f) {
            std::snprintf(valueStr, sizeof(valueStr), "%.1fk", displayValue / 1000.0f);
        } else {
            std::snprintf(valueStr, sizeof(valueStr), "%.0f Hz", displayValue);
        }
    } else if (slider.paramIndex == kParamGate || slider.paramIndex == kParamInputGain || slider.paramIndex == kParamOutput) {
        displayValue = denormalizeValue(slider.value, slider.minValue, slider.maxValue);
        std::snprintf(valueStr, sizeof(valueStr), "%.0f dB", displayValue);
    } else if (slider.isBipolar) {
        displayValue = denormalizeValue(slider.value, slider.minValue, slider.maxValue);
        if (displayValue > 0.5f) {
            std::snprintf(valueStr, sizeof(valueStr), "+%.0f%%", displayValue);
        } else if (displayValue < -0.5f) {
            std::snprintf(valueStr, sizeof(valueStr), "%.0f%%", displayValue);
        } else {
            std::snprintf(valueStr, sizeof(valueStr), "0%%");
        }
    } else {
        displayValue = denormalizeValue(slider.value, slider.minValue, slider.maxValue);
        std::snprintf(valueStr, sizeof(valueStr), "%.0f%%", displayValue);
    }

    // Draw value display in the value box area (background image has the box)
    const float valueBoxH = 18.0f;
    const float valueBoxY = y + h - valueBoxH - 6.0f;

    fontSize(11.0f);
    fillColor(Colors::accentR, Colors::accentG, Colors::accentB);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(trackX, valueBoxY + valueBoxH * 0.5f, valueStr, nullptr);

    // Draw tooltip near handle when actively dragging
    if (isActive)
    {
        const float tooltipW = 54.0f;
        const float tooltipH = 22.0f;
        const float tooltipX = trackX + 16.0f;
        float tooltipY = valueY - tooltipH * 0.5f;

        // Keep tooltip within slider bounds
        if (tooltipY < y + 28.0f) tooltipY = y + 28.0f;
        if (tooltipY + tooltipH > y + h - 26.0f) tooltipY = y + h - 26.0f - tooltipH;

        // Tooltip background
        beginPath();
        roundedRect(tooltipX, tooltipY, tooltipW, tooltipH, 5.0f);
        fillColor(0.0f, 0.0f, 0.0f, 0.9f);
        fill();

        beginPath();
        roundedRect(tooltipX, tooltipY, tooltipW, tooltipH, 5.0f);
        strokeColor(Colors::accentR, Colors::accentG, Colors::accentB);
        strokeWidth(1.2f);
        stroke();

        // Tooltip text
        fontSize(13.0f);
        fillColor(1.0f, 1.0f, 1.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(tooltipX + tooltipW * 0.5f, tooltipY + tooltipH * 0.5f, valueStr, nullptr);
    }
}

bool GristUI::onMouse(const MouseEvent& ev)
{
    if (ev.button != 1)
        return false;

    if (ev.press)
    {
        activeSlider = getSliderAt(ev.pos.getX(), ev.pos.getY());
        if (activeSlider >= 0)
        {
            const float newValue = sliderValueFromY(sliders[activeSlider], ev.pos.getY());
            sliders[activeSlider].value = newValue;

            float paramValue;
            if (sliders[activeSlider].isLogarithmic) {
                paramValue = sliders[activeSlider].minValue *
                    std::pow(sliders[activeSlider].maxValue / sliders[activeSlider].minValue, newValue);
            } else {
                paramValue = denormalizeValue(newValue,
                                              sliders[activeSlider].minValue,
                                              sliders[activeSlider].maxValue);
            }
            setParameterValue(sliders[activeSlider].paramIndex, paramValue);
            repaint();
            return true;
        }
    }
    else
    {
        activeSlider = -1;
    }

    return false;
}

bool GristUI::onMotion(const MotionEvent& ev)
{
    if (activeSlider < 0)
        return false;

    const float newValue = sliderValueFromY(sliders[activeSlider], ev.pos.getY());
    sliders[activeSlider].value = newValue;

    float paramValue;
    if (sliders[activeSlider].isLogarithmic) {
        paramValue = sliders[activeSlider].minValue *
            std::pow(sliders[activeSlider].maxValue / sliders[activeSlider].minValue, newValue);
    } else {
        paramValue = denormalizeValue(newValue,
                                      sliders[activeSlider].minValue,
                                      sliders[activeSlider].maxValue);
    }
    setParameterValue(sliders[activeSlider].paramIndex, paramValue);

    repaint();
    return true;
}

int GristUI::getSliderAt(float x, float y)
{
    for (int i = 0; i < NUM_SLIDERS; ++i)
    {
        if (x >= sliders[i].x && x <= sliders[i].x + sliders[i].width &&
            y >= sliders[i].y && y <= sliders[i].y + sliders[i].height)
        {
            return i;
        }
    }
    return -1;
}

float GristUI::sliderValueFromY(const Slider& slider, float y) const
{
    const float trackTop = slider.y + 30.0f;
    const float trackBottom = slider.y + slider.height - 28.0f;
    const float clampedY = std::max(trackTop, std::min(trackBottom, y));
    return (trackBottom - clampedY) / (trackBottom - trackTop);
}

float GristUI::normalizeValue(float value, float min, float max)
{
    return (value - min) / (max - min);
}

float GristUI::denormalizeValue(float normalized, float min, float max)
{
    return min + normalized * (max - min);
}

UI* createUI()
{
    return new GristUI();
}

END_NAMESPACE_DISTRHO
