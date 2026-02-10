/*
 * Grist — UI (simple sliders)
 */

#include "GristUI.hpp"
#include "GristVizBus.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "DSP/dr_wav.h"

static inline float fclampf(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

START_NAMESPACE_DISTRHO

GristUI::GristUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      active(-1),
      btnX(0.0f), btnY(14.0f), btnW(0.0f), btnH(30.0f),
      btn2X(18.0f), btn2Y(14.0f), btn2W(420.0f), btn2H(30.0f)
{
    // layout buttons based on window size
    btnX = btn2X + btn2W + 12.0f;
    btnW = std::max(180.0f, getWidth() - btnX - 18.0f);
    btnY = btn2Y;
    btnH = btn2H;
    std::snprintf(sampleLabel, sizeof(sampleLabel), "Sample path: ~/Documents/samples/grist.wav");
    loadSharedResources();
    layoutWaveArea();
    initSliders();

    for (uint32_t i = 0; i < kMaxVizGrains; ++i)
        grainPos[i] = 0.0f;
    grainCount = 0;
}

void GristUI::initSliders()
{
    const float margin = 18.0f;
    const float gap = 10.0f;
    const float sliderW = (getWidth() - margin*2 - gap*(kNumSliders-1)) / (float)kNumSliders;

    // leave room for waveform/grain viz
    const float y = waveY + waveH + 16.0f;
    const float sliderH = getHeight() - y - 18.0f;

    const struct { uint32_t p; float minV; float maxV; const char* label; const char* unit; bool bipolar; } defs[kNumSliders] = {
        { kParamGain, 0.0f, 1.0f, "Gain", "", false },
        { kParamGrainSizeMs, 5.0f, 250.0f, "Size", "ms", false },
        { kParamDensity, 1.0f, 80.0f, "Dens", "gr/s", false },
        { kParamPosition, 0.0f, 100.0f, "Pos", "%", false },
        { kParamSpray, 0.0f, 100.0f, "Spray", "%", false },
        { kParamPitch, -24.0f, 24.0f, "Pitch", "st", true },
        { kParamRandomPitch, 0.0f, 12.0f, "Rnd", "st", false },
        { kParamPitchEnvAmt, -48.0f, 48.0f, "PEnv", "st", true },
        { kParamPitchEnvDecayMs, 0.0f, 5000.0f, "PDec", "ms", false },
        { kParamAttackMs, 0.0f, 2000.0f, "Atk", "ms", false },
        { kParamReleaseMs, 5.0f, 5000.0f, "Rel", "ms", false },
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

void GristUI::layoutWaveArea()
{
    waveX = 18.0f;
    waveY = 72.0f;
    waveW = std::max(10.0f, getWidth() - 36.0f);
    waveH = 110.0f;
}

void GristUI::parseGrainViz(const char* value)
{
    grainCount = 0;
    if (value == nullptr || value[0] == '\0')
        return;

    const char* p = value;
    while (*p != '\0' && grainCount < kMaxVizGrains)
    {
        char* end = nullptr;
        const float v = std::strtof(p, &end);
        if (end == p)
            break;
        grainPos[grainCount++] = fclampf(v, 0.0f, 1.0f);
        p = end;
        while (*p == ',' || *p == ' ' || *p == '\t') ++p;
    }
}

void GristUI::parseActiveGrainViz(const char* value)
{
    activeCount = 0;
    if (value == nullptr || value[0] == '\0')
        return;

    const char* p = value;
    while (*p != '\0' && activeCount < kMaxActiveViz)
    {
        // start
        char* end1 = nullptr;
        const float start01 = std::strtof(p, &end1);
        if (end1 == p) break;
        p = end1;
        if (*p != ',') break;
        ++p;

        // end
        char* end2 = nullptr;
        const float end01 = std::strtof(p, &end2);
        if (end2 == p) break;
        p = end2;
        if (*p != ',') break;
        ++p;

        // age
        char* end3 = nullptr;
        const float age01 = std::strtof(p, &end3);
        if (end3 == p) break;
        p = end3;
        if (*p != ',') break;
        ++p;

        // amp
        char* end4 = nullptr;
        const float amp01 = std::strtof(p, &end4);
        if (end4 == p) break;
        p = end4;
        if (*p != ',') break;
        ++p;

        // voice index
        char* end5 = nullptr;
        const long voice = std::strtol(p, &end5, 10);
        if (end5 == p) break;
        p = end5;

        activeGrains[activeCount].start01 = fclampf(start01, 0.0f, 1.0f);
        activeGrains[activeCount].end01 = fclampf(end01, 0.0f, 1.0f);
        activeGrains[activeCount].age01 = fclampf(age01, 0.0f, 1.0f);
        activeGrains[activeCount].amp01 = fclampf(amp01, 0.0f, 1.0f);
        activeGrains[activeCount].voice = (int)voice;
        ++activeCount;

        // delimiter ; or end
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ';') { ++p; while (*p == ' ' || *p == '\t') ++p; continue; }
        if (*p == '\0') break;
        // unexpected separator
        break;
    }
}

void GristUI::rebuildWavePeaks()
{
    waveMin.clear();
    waveMax.clear();

    if (samplePath.empty() || waveW < 4.0f)
        return;

    drwav wav;
    if (!drwav_init_file(&wav, samplePath.c_str(), nullptr))
        return;

    const uint32_t ch = wav.channels;
    const uint64_t frames = wav.totalPCMFrameCount;
    if (frames < 2 || ch < 1)
    {
        drwav_uninit(&wav);
        return;
    }

    const uint32_t cols = (uint32_t)std::max(8.0f, std::floor(waveW));
    waveMin.assign(cols, 0.0f);
    waveMax.assign(cols, 0.0f);

    // Read in chunks to avoid huge allocations
    const uint32_t chunkFrames = 4096;
    std::vector<float> buf;
    buf.resize((size_t)chunkFrames * ch);

    uint64_t frameIndex = 0;
    while (frameIndex < frames)
    {
        const uint64_t toRead = std::min<uint64_t>(chunkFrames, frames - frameIndex);
        const uint64_t got = drwav_read_pcm_frames_f32(&wav, toRead, buf.data());
        if (got == 0)
            break;

        for (uint64_t i = 0; i < got; ++i)
        {
            float s = buf[(size_t)i * ch];
            if (ch > 1)
                s = 0.5f * (s + buf[(size_t)i * ch + 1]);

            const uint64_t global = frameIndex + i;
            const uint32_t col = (uint32_t)std::min<uint64_t>(cols - 1, (global * cols) / frames);
            if (global == 0)
            {
                waveMin[col] = s;
                waveMax[col] = s;
            }
            else
            {
                waveMin[col] = std::min(waveMin[col], s);
                waveMax[col] = std::max(waveMax[col], s);
            }
        }

        frameIndex += got;
    }

    drwav_uninit(&wav);
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

void GristUI::uiIdle()
{
    bool changed = false;

    // Pull viz data from in-process bus (needed for CLAP backend)
    uint32_t sc = 0;
    float sp[GristVizBus::kMaxSpawn];
    if (GristVizBus::instance().copySpawnIfNew(lastSpawnSeq, sp, sc))
    {
        grainCount = std::min<uint32_t>(sc, kMaxVizGrains);
        for (uint32_t i = 0; i < grainCount; ++i)
            grainPos[i] = sp[i];
        changed = true;
    }

    uint32_t ac = 0;
    GristVizBus::Active a[GristVizBus::kMaxActive];
    if (GristVizBus::instance().copyActiveIfNew(lastActiveSeq, a, ac))
    {
        activeCount = std::min<uint32_t>(ac, kMaxActiveViz);
        for (uint32_t i = 0; i < activeCount; ++i)
        {
            activeGrains[i].start01 = a[i].start01;
            activeGrains[i].end01 = a[i].end01;
            activeGrains[i].age01 = a[i].age01;
            activeGrains[i].amp01 = a[i].amp01;
            activeGrains[i].voice = (int)a[i].voice;
        }
        changed = true;
    }

    if (changed)
        repaint();
}

void GristUI::stateChanged(const char* key, const char* value)
{
    if (!key)
        return;

    if (std::strcmp(key, "sample") == 0)
    {
        if (value && value[0] != '\0')
        {
            samplePath = value;
            const char* lastSlash = std::strrchr(value, '/');
            const char* name = lastSlash ? (lastSlash + 1) : value;
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Sample: %s", name);
            rebuildWavePeaks();
        }
        else
        {
            samplePath.clear();
            waveMin.clear();
            waveMax.clear();
            std::snprintf(sampleLabel, sizeof(sampleLabel), "No sample loaded");
        }
        repaint();
        return;
    }

    if (std::strcmp(key, "grains") == 0)
    {
        parseGrainViz(value);
        repaint();
        return;
    }

    if (std::strcmp(key, "grains_active") == 0)
    {
        parseActiveGrainViz(value);
        repaint();
        return;
    }

    if (std::strcmp(key, "sample_status") == 0)
    {
        if (value && std::strcmp(value, "error") == 0)
        {
            // keep label as-is; error text will come via sample_error
        }
        repaint();
        return;
    }

    if (std::strcmp(key, "sample_error") == 0)
    {
        if (value && value[0] != '\0')
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Load failed: %s", value);
        repaint();
        return;
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
        // Reload default sample button
        if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH)
        {
            // Reload from default path via special state value
            setState("sample", "__DEFAULT__");
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Reloading default: grist.wav");
            repaint();
            return true;
        }

        // Load sample (open file dialog)
        if (mx >= btn2X && mx <= btn2X + btn2W && my >= btn2Y && my <= btn2Y + btn2H)
        {
            const bool ok = requestStateFile("sample");
            std::snprintf(sampleLabel, sizeof(sampleLabel), ok ? "Choose a sample…" : "File dialog unavailable");
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
    text(18.0f, 40.0f, "CLAP synth (granular WIP) — bigger UI", nullptr);

    // Load sample button (file dialog)
    beginPath();
    roundedRect(btn2X, btn2Y, btn2W, btn2H, 6.0f);
    fillColor(0.18f, 0.18f, 0.2f);
    fill();
    strokeColor(0.35f, 0.35f, 0.4f);
    strokeWidth(1.0f);
    stroke();

    fontSize(12.0f);
    fillColor(0.9f, 0.9f, 0.9f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(btn2X + btn2W*0.5f, btn2Y + btn2H*0.5f, "Load sample…", nullptr);

    // Reload default button
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
    text(btnX + btnW*0.5f, btnY + btnH*0.5f, "Reload default", nullptr);

    // Sample label
    fontSize(11.0f);
    fillColor(0.75f, 0.75f, 0.75f);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(18.0f, 52.0f, sampleLabel, nullptr);

    // Waveform + grain viz
    beginPath();
    roundedRect(waveX, waveY, waveW, waveH, 8.0f);
    fillColor(0.10f, 0.10f, 0.11f);
    fill();
    strokeColor(0.22f, 0.22f, 0.25f);
    strokeWidth(1.0f);
    stroke();

    // zero line
    const float midY = waveY + waveH * 0.5f;
    beginPath();
    moveTo(waveX + 8.0f, midY);
    lineTo(waveX + waveW - 8.0f, midY);
    strokeColor(0.18f, 0.18f, 0.2f);
    strokeWidth(1.0f);
    stroke();

    // waveform peaks
    if (!waveMin.empty() && waveMin.size() == waveMax.size())
    {
        const uint32_t cols = (uint32_t)waveMin.size();
        const float innerX = waveX + 8.0f;
        const float innerW = waveW - 16.0f;
        const float sx = innerW / (float)cols;

        for (uint32_t c = 0; c < cols; ++c)
        {
            const float x = innerX + (float)c * sx;
            const float y0 = midY - waveMax[c] * (waveH * 0.45f);
            const float y1 = midY - waveMin[c] * (waveH * 0.45f);
            beginPath();
            moveTo(x, y0);
            lineTo(x, y1);
            strokeColor(0.55f, 0.55f, 0.58f, 0.9f);
            strokeWidth(1.0f);
            stroke();
        }
    }

    // active grains (rectangles spanning source region)
    if (activeCount > 0)
    {
        const float innerX = waveX + 8.0f;
        const float innerW = waveW - 16.0f;
        const float mid = waveY + waveH * 0.5f;
        const float yRange = waveH * 0.42f;

        for (uint32_t g = 0; g < activeCount; ++g)
        {
            float a = 1.0f - activeGrains[g].age01;
            a = fclampf(a, 0.0f, 1.0f);
            const float alpha = 0.10f + 0.30f * a;

            float x0 = innerX + activeGrains[g].start01 * innerW;
            float x1 = innerX + activeGrains[g].end01 * innerW;
            if (x1 < x0) std::swap(x0, x1);
            if (x1 - x0 < 2.0f) x1 = x0 + 2.0f;

            // height = grain envelope level (animated)
            float hh = std::max(2.0f, activeGrains[g].amp01 * yRange);
            const float top = mid - hh;
            const float bottom = mid + hh;

            // per-voice color (HSV wheel)
            const int v = std::max(0, activeGrains[g].voice);
            const float hue = (float)(v % 16) / 16.0f; // 0..1
            const float r = std::fabs(hue * 6.0f - 3.0f) - 1.0f;
            const float gC = 2.0f - std::fabs(hue * 6.0f - 2.0f);
            const float b = 2.0f - std::fabs(hue * 6.0f - 4.0f);
            const float rr = fclampf(r, 0.0f, 1.0f);
            const float gg = fclampf(gC, 0.0f, 1.0f);
            const float bb = fclampf(b, 0.0f, 1.0f);

            beginPath();
            rect(x0, top, x1 - x0, bottom - top);
            fillColor(rr, gg, bb, alpha);
            fill();
        }
    }

    // spawn markers (vertical lines)
    if (grainCount > 0)
    {
        const float innerX = waveX + 8.0f;
        const float innerW = waveW - 16.0f;
        for (uint32_t g = 0; g < grainCount; ++g)
        {
            const float x = innerX + grainPos[g] * innerW;
            beginPath();
            moveTo(x, waveY + 8.0f);
            lineTo(x, waveY + waveH - 8.0f);
            strokeColor(0.95f, 0.85f, 0.35f, 0.65f);
            strokeWidth(2.0f);
            stroke();
        }
    }

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
