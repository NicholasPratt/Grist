/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2024 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "DistrhoUI.hpp"
#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

// -----------------------------------------------------------------------------------------------------------

class GristUI : public UI
{
public:
    GristUI()
        : UI(600, 300),
          fDraggingParam(-1)
    {
        // Initialize default values and ranges
        // Note: These should match the DSP side defaults in Grist.cpp

        // 0: Gate
        fParams[0] = -60.0f;
        fRanges[0] = { -90.0f, 0.0f, false };
        fLabels[0] = "Gate";
        fUnits[0]  = "dB";

        // 1: Low Cut
        fParams[1] = 20.0f;
        fRanges[1] = { 20.0f, 1000.0f, true };
        fLabels[1] = "Low Cut";
        fUnits[1]  = "Hz";

        // 2: Drive
        fParams[2] = 0.0f;
        fRanges[2] = { 0.0f, 60.0f, false };
        fLabels[2] = "Drive";
        fUnits[2]  = "dB";

        // 3: Tone
        fParams[3] = 16000.0f;
        fRanges[3] = { 500.0f, 20000.0f, true };
        fLabels[3] = "Tone";
        fUnits[3]  = "Hz";

        // 4: Presence
        fParams[4] = 0.0f;
        fRanges[4] = { -12.0f, 12.0f, false };
        fLabels[4] = "Presence";
        fUnits[4]  = "dB";

        // 5: Mix
        fParams[5] = 1.0f;
        fRanges[5] = { 0.0f, 1.0f, false };
        fLabels[5] = "Mix";
        fUnits[5]  = "";

        // 6: Level
        fParams[6] = -3.0f;
        fRanges[6] = { -60.0f, 6.0f, false };
        fLabels[6] = "Level";
        fUnits[6]  = "dB";

        // 7: Bias
        fParams[7] = 0.0f;
        fRanges[7] = { -1.0f, 1.0f, false };
        fLabels[7] = "Bias";
        fUnits[7]  = "";

        setGeometryConstraints(600, 300, true);
    }

protected:
    /* --------------------------------------------------------------------------------------------------------
     * DSP/Plugin Callbacks */

    void parameterChanged(uint32_t index, float value) override
    {
        if (index < 8)
        {
            fParams[index] = value;
            repaint();
        }
    }

    /* --------------------------------------------------------------------------------------------------------
     * Widget Callbacks */

    void onNanoDisplay() override
    {
        const float width  = getWidth();
        const float height = getHeight();

        // Background
        beginPath();
        rect(0, 0, width, height);
        fillColor(Color(40, 40, 40));
        fill();

        // Title
        fontSize(24.0f);
        fillColor(Color(200, 200, 200));
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(width / 2, 10, "TRANSISTOR DISTORTION", nullptr);

        // Draw Sliders
        float sliderWidth = width / 4.0f;
        float sliderHeight = (height - 50) / 2.0f;

        for (int i = 0; i < 8; ++i)
        {
            int row = i / 4;
            int col = i % 4;
            float x = col * sliderWidth;
            float y = 50 + row * sliderHeight;

            drawSlider(i, x, y, sliderWidth, sliderHeight);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.press && ev.button == 1)
        {
            int index = getParamIndexAt(ev.pos.getX(), ev.pos.getY());
            if (index != -1)
            {
                fDraggingParam = index;
                updateParamFromMouse(ev.pos.getX(), ev.pos.getY(), true);
                return true;
            }
        }
        else if (!ev.press && ev.button == 1)
        {
            if (fDraggingParam != -1)
            {
                editParameter(fDraggingParam, false); // End edit
                fDraggingParam = -1;
                return true;
            }
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fDraggingParam != -1)
        {
            updateParamFromMouse(ev.pos.getX(), ev.pos.getY(), false);
            return true;
        }
        return false;
    }

private:
    struct Range { float min, max; bool log; };

    float fParams[8];
    Range fRanges[8];
    const char* fLabels[8];
    const char* fUnits[8];
    int fDraggingParam;

    void drawSlider(int index, float x, float y, float w, float h)
    {
        float trackX = x + w/2;
        float trackY = y + 30;
        float trackH = h - 60;

        // Label
        fontSize(16.0f);
        fillColor(Color(180, 180, 180));
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(x + w/2, y + 5, fLabels[index], nullptr);

        // Track
        beginPath();
        rect(trackX - 2, trackY, 4, trackH);
        fillColor(Color(20, 20, 20));
        fill();

        // Handle
        float norm = normalize(fParams[index], fRanges[index]);
        float handleY = trackY + trackH * (1.0f - norm);

        beginPath();
        circle(trackX, handleY, 8);
        fillColor(Color(255, 150, 0)); // Orange
        fill();
        strokeColor(Color(20, 20, 20));
        strokeWidth(1.0f);
        stroke();

        // Value Label
        char buf[32];
        if (index == 5) // Mix - display as percentage
            std::sprintf(buf, "%.0f%%", fParams[index] * 100.0f);
        else if (index == 7) // Bias - no unit, 2 decimal places
            std::sprintf(buf, "%.2f", fParams[index]);
        else if (std::abs(fParams[index]) < 10.0f)
            std::sprintf(buf, "%.1f %s", fParams[index], fUnits[index]);
        else
            std::sprintf(buf, "%.0f %s", fParams[index], fUnits[index]);

        fontSize(14.0f);
        fillColor(Color(220, 220, 220));
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(x + w/2, y + h - 22, buf, nullptr);
    }

    int getParamIndexAt(float mx, float my)
    {
        float width = getWidth();
        float height = getHeight();
        float sliderWidth = width / 4.0f;
        float sliderHeight = (height - 50) / 2.0f;

        if (my < 50) return -1;

        int col = (int)(mx / sliderWidth);
        int row = (int)((my - 50) / sliderHeight);

        if (col >= 0 && col < 4 && row >= 0 && row < 2)
            return row * 4 + col;

        return -1;
    }

    void updateParamFromMouse(float mx, float my, bool start)
    {
        if (fDraggingParam == -1) return;

        float height = getHeight();
        float sliderHeight = (height - 50) / 2.0f;

        int row = fDraggingParam / 4;
        float y = 50 + row * sliderHeight;
        float trackY = y + 30;
        float trackH = sliderHeight - 60;

        // Clamp mouse Y to track
        float val = 1.0f - (my - trackY) / trackH;
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;

        float realVal = denormalize(val, fRanges[fDraggingParam]);

        if (start) editParameter(fDraggingParam, true);
        setParameterValue(fDraggingParam, realVal);
        fParams[fDraggingParam] = realVal; // Update local immediately for smooth drag
        repaint();
    }

    float normalize(float val, const Range& r)
    {
        if (r.log)
            return (std::log(val) - std::log(r.min)) / (std::log(r.max) - std::log(r.min));
        return (val - r.min) / (r.max - r.min);
    }

    float denormalize(float norm, const Range& r)
    {
        if (r.log)
            return std::exp(norm * (std::log(r.max) - std::log(r.min)) + std::log(r.min));
        return norm * (r.max - r.min) + r.min;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristUI)
};

UI* createUI()
{
    return new GristUI();
}

END_NAMESPACE_DISTRHO