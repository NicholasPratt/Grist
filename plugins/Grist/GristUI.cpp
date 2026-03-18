/*
 * Grist — UI (NanoVG)
 *
 * Chunky, hardware-ish layout:
 * - PERFORM tab: waveform+grain viz + 4 big macros + hero knobs + small knobs
 * - XY tab: big XY pad (X/Y as modulation sources)
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

static inline float lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

START_NAMESPACE_DISTRHO

// ---------------------------
// Construction + layout
// ---------------------------

GristUI::GristUI()
    : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
      btnX(0.0f), btnY(14.0f), btnW(0.0f), btnH(30.0f),
      btn2X(18.0f), btn2Y(14.0f), btn2W(220.0f), btn2H(30.0f)
{
    tabX = 18.0f;
    tabY = 14.0f;
    tabW = 92.0f;
    tabH = 30.0f;

    // buttons: [PERFORM][XY] [Load…] [Reload]
    // We'll compute final positions in layoutPerform().

    std::snprintf(sampleLabel, sizeof(sampleLabel), "Sample: ~/Documents/samples/grist.wav");

    loadSharedResources();
    layoutWaveArea();
    layoutPerform();
    layoutXY();
    initKnobs();
    initModDefaults();

    for (uint32_t i = 0; i < kMaxVizGrains; ++i)
        grainPos[i] = 0.0f;
    grainCount = 0;
}

void GristUI::layoutWaveArea()
{
    // Waveform panel lives on PERFORM tab.
    waveX = 18.0f;
    waveY = 60.0f;
    waveW = 620.0f;
    waveH = 180.0f;
}

void GristUI::layoutPerform()
{
    // tabs
    tabX = 18.0f;
    tabY = 14.0f;
    tabW = 92.0f;
    tabH = 30.0f;

    // Load button next to tabs
    btn2X = tabX + tabW * 2 + 12.0f;
    btn2Y = tabY;
    btn2W = 160.0f;
    btn2H = tabH;

    // Reload on right
    btnW = 190.0f;
    btnX = getWidth() - 18.0f - btnW;
    btnY = tabY;
    btnH = tabH;

    // Reposition waveform area to fill left side (leave room for right control column)
    waveX = 18.0f;
    waveY = 60.0f;
    waveW = getWidth() - 18.0f - 18.0f - 300.0f;
    waveH = 180.0f;
}

void GristUI::layoutXY()
{
    xyX = 18.0f;
    xyY = 60.0f;
    xyW = getWidth() - 36.0f;
    xyH = getHeight() - xyY - 18.0f;
}

void GristUI::initKnobs()
{
    // Right control column
    const float colX = waveX + waveW + 18.0f;
    const float colW = getWidth() - 18.0f - colX;

    const float macroR = 46.0f;
    const float macroGapX = 24.0f;
    const float macroGapY = 18.0f;

    // 2x2 macros
    const float m0x = colX + 18.0f + macroR;
    const float m0y = waveY + 24.0f + macroR;

    const struct { uint32_t p; const char* label; } mdefs[4] = {
        { kParamMacro1, "MACRO 1" },
        { kParamMacro2, "MACRO 2" },
        { kParamMacro3, "MACRO 3" },
        { kParamMacro4, "MACRO 4" },
    };

    for (uint32_t i = 0; i < 4; ++i)
    {
        const float cx = m0x + (i % 2) * (macroR * 2.0f + macroGapX);
        const float cy = m0y + (i / 2) * (macroR * 2.0f + macroGapY);
        macro[i] = { cx, cy, macroR, mdefs[i].p, -1.0f, 1.0f, 0.0f, mdefs[i].label, "", true, 0.0f };
    }

    // Hero knobs (under macros)
    const float heroR = 30.0f;
    const float heroTop = waveY + 2.0f * (macroR * 2.0f + macroGapY) + 40.0f;
    const float heroLeft = colX + 18.0f + heroR;
    const float heroGapY = 18.0f;

    const struct { uint32_t p; float minV; float maxV; float defV; const char* label; const char* unit; bool bipolar; } hdefs[5] = {
        { kParamGrainSizeMs, 5.0f, 250.0f, 60.0f, "SIZE", "ms", false },
        { kParamDensity,     1.0f, 80.0f,  20.0f, "DENS", "gr/s", false },
        { kParamPosition,    0.0f, 100.0f, 50.0f, "POS",  "%", false },
        { kParamSpray,       0.0f, 100.0f, 0.0f,  "SPRAY","%", false },
        { kParamPitch,      -24.0f, 24.0f, 0.0f,  "PITCH","st", true },
    };

    for (uint32_t i = 0; i < 5; ++i)
    {
        const float cx = heroLeft;
        const float cy = heroTop + i * (heroR * 2.0f + heroGapY);
        hero[i] = { cx, cy, heroR, hdefs[i].p, hdefs[i].minV, hdefs[i].maxV, hdefs[i].defV, hdefs[i].label, hdefs[i].unit, hdefs[i].bipolar, hdefs[i].defV };
    }

    // Small knobs along bottom under waveform
    const float stripY = waveY + waveH + 18.0f;
    const float stripH = getHeight() - stripY - 18.0f;

    const float smallR = 22.0f;
    const float startX = waveX + 18.0f + smallR;
    const float gapX = 24.0f;

    const struct { uint32_t p; float minV; float maxV; float defV; const char* label; const char* unit; bool bipolar; } sdefs[6] = {
        { kParamGain,           0.0f, 2.0f,    1.0f,  "GAIN", "", false },
        { kParamAttackMs,       0.0f, 2000.0f, 5.0f,  "ATK",  "ms", false },
        { kParamReleaseMs,      5.0f, 5000.0f, 120.0f,"REL",  "ms", false },
        { kParamPitchEnvAmt,   -48.0f, 48.0f,  0.0f,  "PENV", "st", true },
        { kParamPitchEnvDecayMs,0.0f, 5000.0f, 120.0f,"PDEC", "ms", false },
        { kParamRandomPitch,    0.0f, 12.0f,   0.0f,  "RND",  "st", false },
    };

    for (uint32_t i = 0; i < 6; ++i)
    {
        const float cx = startX + i * (smallR * 2.0f + gapX);
        const float cy = stripY + stripH * 0.5f;
        small[i] = { cx, cy, smallR, sdefs[i].p, sdefs[i].minV, sdefs[i].maxV, sdefs[i].defV, sdefs[i].label, sdefs[i].unit, sdefs[i].bipolar, sdefs[i].defV };
    }

    // init cached values (host will call parameterChanged too)
    for (uint32_t i = 0; i < kNumMacroKnobs; ++i) macro[i].value = macro[i].defV;
    for (uint32_t i = 0; i < kNumHeroKnobs; ++i)  hero[i].value = hero[i].defV;
    for (uint32_t i = 0; i < kNumSmallKnobs; ++i) small[i].value = small[i].defV;
}

void GristUI::setParamFromValue(uint32_t param, float v)
{
    setParameterValue(param, v);
}

bool GristUI::hitTestKnob(float x, float y, int& outGroup, int& outIndex) const
{
    auto hit = [&](const Knob* ks, uint32_t n, int group) -> bool {
        for (uint32_t i = 0; i < n; ++i)
        {
            const float dx = x - ks[i].x;
            const float dy = y - ks[i].y;
            if (dx*dx + dy*dy <= ks[i].r * ks[i].r)
            {
                outGroup = group;
                outIndex = (int)i;
                return true;
            }
        }
        return false;
    };

    if (hit(macro, kNumMacroKnobs, 0)) return true;
    if (hit(hero,  kNumHeroKnobs,  1)) return true;
    if (hit(small, kNumSmallKnobs, 2)) return true;
    return false;
}

// ---------------------------
// Mod matrix UI helpers
// ---------------------------

void GristUI::drawModSlotsForParam(uint32_t param, float x, float y)
{
    ModTarget tgt;
    if (!sliderToModTarget(param, tgt))
        return;

    const float bs = 12.0f;
    const float gap = 6.0f;

    for (uint32_t si = 0; si < kSlotsPerTarget; ++si)
    {
        const float bx = x + (float)si * (bs + gap);
        const float by = y;

        beginPath();
        roundedRect(bx, by, bs, bs, 3.0f);

        const ModSlot& sl = mod[(uint32_t)tgt][si];
        if (sl.src == ModSource::None)
            fillColor(0.13f, 0.13f, 0.14f);
        else
        {
            const float a = fclampf(std::fabs(sl.amt), 0.0f, 1.0f);
            if (sl.amt >= 0.0f)
                fillColor(0.25f + 0.35f*a, 0.20f + 0.25f*a, 0.10f);
            else
                fillColor(0.10f, 0.22f + 0.35f*a, 0.32f + 0.20f*a);
        }
        fill();
        strokeColor(0.28f, 0.28f, 0.30f);
        strokeWidth(1.0f);
        stroke();

        fontSize(8.5f);
        fillColor(0.92f, 0.92f, 0.92f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(bx + bs*0.5f, by + bs*0.5f + 0.2f, modSourceLabel(sl.src), nullptr);
    }
}

bool GristUI::hitTestModBox(float x, float y, int& outTarget, int& outSlot) const
{
    // Hero knobs only (keeps it tidy)
    for (uint32_t i = 0; i < kNumHeroKnobs; ++i)
    {
        ModTarget tgt;
        if (!sliderToModTarget(hero[i].param, tgt))
            continue;

        // boxes under the label (top-right-ish)
        const float bx0 = hero[i].x + hero[i].r + 8.0f;
        const float by0 = hero[i].y - hero[i].r - 2.0f;
        const float bs = 12.0f;
        const float gap = 6.0f;

        for (uint32_t s = 0; s < kSlotsPerTarget; ++s)
        {
            const float bx = bx0 + (float)s * (bs + gap);
            const float by = by0;
            if (x >= bx && x <= bx + bs && y >= by && y <= by + bs)
            {
                outTarget = (int)tgt;
                outSlot = (int)s;
                return true;
            }
        }
    }
    return false;
}

// ---------------------------
// Parameter + state sync
// ---------------------------

void GristUI::parameterChanged(uint32_t index, float value)
{
    auto upd = [&](Knob* ks, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i)
            if (ks[i].param == index)
                ks[i].value = value;
    };

    upd(macro, kNumMacroKnobs);
    upd(hero,  kNumHeroKnobs);
    upd(small, kNumSmallKnobs);

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

    if (std::strcmp(key, "mod_matrix") == 0)
    {
        parseModMatrixState(value);
        repaint();
        return;
    }

    if (std::strcmp(key, "sample_status") == 0)
    {
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

#if DISTRHO_UI_FILE_BROWSER
void GristUI::uiFileBrowserSelected(const char* filename)
{
    if (filename == nullptr || filename[0] == '\0')
    {
        std::snprintf(sampleLabel, sizeof(sampleLabel), "Load cancelled");
        repaint();
        return;
    }

    setState("sample", filename);
    const char* lastSlash = std::strrchr(filename, '/');
    const char* name = lastSlash ? (lastSlash + 1) : filename;
    std::snprintf(sampleLabel, sizeof(sampleLabel), "Loading: %s", name);
    repaint();
}
#endif

// ---------------------------
// Input
// ---------------------------

bool GristUI::onMouse(const MouseEvent& ev)
{
    if (ev.button != 1)
        return false;

    const float mx = ev.pos.getX();
    const float my = ev.pos.getY();

    if (ev.press)
    {
        // tabs
        const float perfX = tabX;
        const float xyXb = tabX + tabW;
        if (mx >= perfX && mx <= perfX + tabW && my >= tabY && my <= tabY + tabH)
        {
            tab = Tab::Perform;
            repaint();
            return true;
        }
        if (mx >= xyXb && mx <= xyXb + tabW && my >= tabY && my <= tabY + tabH)
        {
            tab = Tab::XY;
            repaint();
            return true;
        }

        // Load sample
        if (mx >= btn2X && mx <= btn2X + btn2W && my >= btn2Y && my <= btn2Y + btn2H)
        {
            const bool ok = requestStateFile("sample");
            std::snprintf(sampleLabel, sizeof(sampleLabel), ok ? "Choose a sample…" : "File dialog unavailable");
            repaint();
            return true;
        }

        // Reload default
        if (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH)
        {
            setState("sample", "__DEFAULT__");
            std::snprintf(sampleLabel, sizeof(sampleLabel), "Reloading default: grist.wav");
            repaint();
            return true;
        }

        if (tab == Tab::XY)
        {
            // XY pad
            if (mx >= xyX && mx <= xyX + xyW && my >= xyY && my <= xyY + xyH)
            {
                xyActive = true;
                const float nx = (mx - xyX) / xyW;
                const float ny = (my - xyY) / xyH;
                const float xv = fclampf(nx * 2.0f - 1.0f, -1.0f, 1.0f);
                const float yv = fclampf((1.0f - ny) * 2.0f - 1.0f, -1.0f, 1.0f);
                setParamFromValue(kParamX, xv);
                setParamFromValue(kParamY, yv);
                repaint();
                return true;
            }
            return false;
        }

        // PERFORM: mod boxes
        {
            int mt = -1, ms = -1;
            if (hitTestModBox(mx, my, mt, ms))
            {
                modDragTarget = mt;
                modDragSlot = ms;
                modDragStartY = my;
                modDragStartAmt = mod[(uint32_t)mt][(uint32_t)ms].amt;

                mod[(uint32_t)mt][(uint32_t)ms].src = nextModSource(mod[(uint32_t)mt][(uint32_t)ms].src);
                if (mod[(uint32_t)mt][(uint32_t)ms].src == ModSource::None)
                    mod[(uint32_t)mt][(uint32_t)ms].amt = 0.0f;
                else if (std::fabs(mod[(uint32_t)mt][(uint32_t)ms].amt) < 1e-6f)
                    mod[(uint32_t)mt][(uint32_t)ms].amt = 0.10f;

                pushModMatrixState();
                repaint();
                return true;
            }
        }

        // PERFORM: knob drag
        int g = -1, k = -1;
        if (hitTestKnob(mx, my, g, k))
        {
            activeKnobGroup = g;
            activeKnobIndex = k;
            knobDragStartY = my;

            Knob* kk = (g == 0) ? &macro[k] : (g == 1) ? &hero[k] : &small[k];
            knobDragStartValue = kk->value;
            repaint();
            return true;
        }
    }
    else
    {
        activeKnobGroup = -1;
        activeKnobIndex = -1;
        modDragTarget = -1;
        modDragSlot = -1;
        xyActive = false;
    }

    return false;
}

bool GristUI::onMotion(const MotionEvent& ev)
{
    const float mx = ev.pos.getX();
    const float my = ev.pos.getY();

    if (tab == Tab::XY)
    {
        if (!xyActive) return false;
        const float nx = fclampf((mx - xyX) / xyW, 0.0f, 1.0f);
        const float ny = fclampf((my - xyY) / xyH, 0.0f, 1.0f);
        const float xv = nx * 2.0f - 1.0f;
        const float yv = (1.0f - ny) * 2.0f - 1.0f;
        setParamFromValue(kParamX, xv);
        setParamFromValue(kParamY, yv);
        repaint();
        return true;
    }

    // Mod amount drag
    if (modDragTarget >= 0 && modDragSlot >= 0)
    {
        const float dy = (modDragStartY - my);
        const float delta = dy / 160.0f;
        float a = modDragStartAmt + delta;
        a = fclampf(a, -1.0f, 1.0f);
        mod[(uint32_t)modDragTarget][(uint32_t)modDragSlot].amt = a;
        pushModMatrixState();
        repaint();
        return true;
    }

    // knob drag
    if (activeKnobGroup >= 0 && activeKnobIndex >= 0)
    {
        Knob* kk = (activeKnobGroup == 0) ? &macro[activeKnobIndex] : (activeKnobGroup == 1) ? &hero[activeKnobIndex] : &small[activeKnobIndex];

        const float dy = (knobDragStartY - my);
        // scaled by range; 160 px for full sweep
        const float t = dy / 160.0f;
        float v = knobDragStartValue + t * (kk->maxV - kk->minV);
        v = fclampf(v, kk->minV, kk->maxV);
        kk->value = v;
        setParamFromValue(kk->param, v);
        repaint();
        return true;
    }

    return false;
}

// ---------------------------
// Drawing
// ---------------------------

void GristUI::drawTabButton(float x, float y, float w, float h, const char* label, bool on)
{
    beginPath();
    roundedRect(x, y, w, h, 8.0f);
    if (on) fillColor(0.22f, 0.20f, 0.14f);
    else    fillColor(0.14f, 0.14f, 0.15f);
    fill();
    strokeColor(0.35f, 0.33f, 0.26f);
    strokeWidth(1.0f);
    stroke();

    fontSize(12.0f);
    fillColor(on ? 0.95f : 0.80f, on ? 0.88f : 0.80f, on ? 0.45f : 0.80f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(x + w*0.5f, y + h*0.5f + 0.5f, label, nullptr);
}

void GristUI::drawKnob(const Knob& k, bool active)
{
    // knob body
    beginPath();
    circle(k.x, k.y, k.r + 10.0f);
    fillColor(0.10f, 0.10f, 0.11f);
    fill();

    beginPath();
    circle(k.x, k.y, k.r + 8.0f);
    fillColor(0.16f, 0.16f, 0.17f);
    fill();
    strokeColor(0.28f, 0.28f, 0.30f);
    strokeWidth(active ? 2.0f : 1.0f);
    stroke();

    // value arc
    const float a0 = -2.35f;
    const float a1 =  2.35f;
    const float t = (k.value - k.minV) / (k.maxV - k.minV);
    const float aa = a0 + (a1 - a0) * fclampf(t, 0.0f, 1.0f);

    beginPath();
    arc(k.x, k.y, k.r + 2.0f, a0, a1, CCW);
    strokeColor(0.20f, 0.20f, 0.21f);
    strokeWidth(6.0f);
    stroke();

    beginPath();
    arc(k.x, k.y, k.r + 2.0f, a0, aa, CCW);
    strokeColor(0.95f, 0.85f, 0.35f);
    strokeWidth(6.0f);
    stroke();

    // indicator
    const float ix = k.x + std::cos(aa) * (k.r - 4.0f);
    const float iy = k.y + std::sin(aa) * (k.r - 4.0f);
    beginPath();
    circle(ix, iy, 4.0f);
    fillColor(0.98f, 0.95f, 0.75f);
    fill();

    // label + value
    fontSize(11.5f);
    fillColor(0.92f, 0.92f, 0.92f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(k.x, k.y + k.r + 20.0f, k.label, nullptr);

    char buf[32];
    if (k.unit && k.unit[0] != '\0')
        std::snprintf(buf, sizeof(buf), "%.1f %s", k.value, k.unit);
    else
        std::snprintf(buf, sizeof(buf), "%.2f", k.value);

    fontSize(10.5f);
    fillColor(0.75f, 0.75f, 0.75f);
    text(k.x, k.y + k.r + 34.0f, buf, nullptr);
}

void GristUI::onNanoDisplay()
{
    const float W = getWidth();
    const float H = getHeight();

    // background
    beginPath();
    rect(0, 0, W, H);
    fillColor(0.07f, 0.07f, 0.075f);
    fill();

    // title
    fontSize(20.0f);
    fillColor(0.95f, 0.85f, 0.35f);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(18.0f, 46.0f, "GRIST", nullptr);

    // tabs
    drawTabButton(tabX, tabY, tabW, tabH, "PERFORM", tab == Tab::Perform);
    drawTabButton(tabX + tabW, tabY, tabW, tabH, "XY", tab == Tab::XY);

    // buttons
    beginPath();
    roundedRect(btn2X, btn2Y, btn2W, btn2H, 8.0f);
    fillColor(0.16f, 0.16f, 0.17f);
    fill();
    strokeColor(0.30f, 0.30f, 0.32f);
    strokeWidth(1.0f);
    stroke();
    fontSize(12.0f);
    fillColor(0.90f, 0.90f, 0.90f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(btn2X + btn2W*0.5f, btn2Y + btn2H*0.5f, "Load sample…", nullptr);

    beginPath();
    roundedRect(btnX, btnY, btnW, btnH, 8.0f);
    fillColor(0.16f, 0.16f, 0.17f);
    fill();
    strokeColor(0.30f, 0.30f, 0.32f);
    strokeWidth(1.0f);
    stroke();
    fontSize(12.0f);
    fillColor(0.90f, 0.90f, 0.90f);
    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
    text(btnX + btnW*0.5f, btnY + btnH*0.5f, "Reload default", nullptr);

    // sample label
    fontSize(11.0f);
    fillColor(0.72f, 0.72f, 0.72f);
    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
    text(18.0f, 58.0f, sampleLabel, nullptr);

    if (tab == Tab::XY)
    {
        // XY pad
        beginPath();
        roundedRect(xyX, xyY, xyW, xyH, 14.0f);
        fillColor(0.10f, 0.10f, 0.11f);
        fill();
        strokeColor(0.25f, 0.25f, 0.28f);
        strokeWidth(1.0f);
        stroke();

        // crosshair (from parameters)
        const float xv = fclampf((macro[0].value*0.0f) + 0.0f, -1.0f, 1.0f); // unused; keep static analysis calm
        (void)xv;
        // We don't cache X/Y knobs; just draw from last known param changes by asking UI? DPF doesn't provide getter.
        // We'll render a hint only.
        fontSize(14.0f);
        fillColor(0.90f, 0.90f, 0.90f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(xyX + xyW*0.5f, xyY + 18.0f, "X/Y PAD (drag)", nullptr);

        return;
    }

    // waveform panel
    beginPath();
    roundedRect(waveX, waveY, waveW, waveH, 14.0f);
    fillColor(0.10f, 0.10f, 0.11f);
    fill();
    strokeColor(0.22f, 0.22f, 0.25f);
    strokeWidth(1.0f);
    stroke();

    // grid
    for (int i = 1; i < 8; ++i)
    {
        const float x = waveX + (waveW * i) / 8.0f;
        beginPath();
        moveTo(x, waveY + 10.0f);
        lineTo(x, waveY + waveH - 10.0f);
        strokeColor(0.13f, 0.13f, 0.14f);
        strokeWidth(1.0f);
        stroke();
    }

    const float midY = waveY + waveH * 0.5f;
    beginPath();
    moveTo(waveX + 10.0f, midY);
    lineTo(waveX + waveW - 10.0f, midY);
    strokeColor(0.16f, 0.16f, 0.17f);
    strokeWidth(1.0f);
    stroke();

    // waveform peaks
    if (!waveMin.empty() && waveMin.size() == waveMax.size())
    {
        const uint32_t cols = (uint32_t)waveMin.size();
        const float innerX = waveX + 10.0f;
        const float innerW = waveW - 20.0f;
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

    // active grains
    if (activeCount > 0)
    {
        const float innerX = waveX + 10.0f;
        const float innerW = waveW - 20.0f;
        const float yRange = waveH * 0.42f;

        for (uint32_t g = 0; g < activeCount; ++g)
        {
            float a = 1.0f - activeGrains[g].age01;
            a = fclampf(a, 0.0f, 1.0f);
            const float alpha = 0.08f + 0.28f * a;

            float x0 = innerX + activeGrains[g].start01 * innerW;
            float x1 = innerX + activeGrains[g].end01 * innerW;
            if (x1 < x0) std::swap(x0, x1);
            if (x1 - x0 < 2.0f) x1 = x0 + 2.0f;

            float hh = std::max(2.0f, activeGrains[g].amp01 * yRange);
            const float top = midY - hh;
            const float bottom = midY + hh;

            const int v = std::max(0, activeGrains[g].voice);
            const float hue = (float)(v % 16) / 16.0f;
            const float r = std::fabs(hue * 6.0f - 3.0f) - 1.0f;
            const float gC = 2.0f - std::fabs(hue * 6.0f - 2.0f);
            const float b = 2.0f - std::fabs(hue * 6.0f - 4.0f);
            const float rr = fclampf(r, 0.0f, 1.0f);
            const float gg = fclampf(gC, 0.0f, 1.0f);
            const float bb = fclampf(b, 0.0f, 1.0f);

            beginPath();
            roundedRect(x0, top, x1 - x0, bottom - top, 3.0f);
            fillColor(rr, gg, bb, alpha);
            fill();
        }
    }

    // spawn markers
    if (grainCount > 0)
    {
        const float innerX = waveX + 10.0f;
        const float innerW = waveW - 20.0f;
        for (uint32_t g = 0; g < grainCount; ++g)
        {
            const float x = innerX + grainPos[g] * innerW;
            beginPath();
            moveTo(x, waveY + 10.0f);
            lineTo(x, waveY + waveH - 10.0f);
            strokeColor(0.95f, 0.85f, 0.35f, 0.60f);
            strokeWidth(2.0f);
            stroke();
        }
    }

    // Right column panel
    const float colX = waveX + waveW + 18.0f;
    const float colY = waveY;
    const float colW = getWidth() - 18.0f - colX;
    const float colH = waveH;

    beginPath();
    roundedRect(colX, colY, colW, colH, 14.0f);
    fillColor(0.09f, 0.09f, 0.10f);
    fill();
    strokeColor(0.20f, 0.20f, 0.22f);
    strokeWidth(1.0f);
    stroke();

    // macros
    for (uint32_t i = 0; i < kNumMacroKnobs; ++i)
        drawKnob(macro[i], activeKnobGroup == 0 && activeKnobIndex == (int)i);

    // hero knobs + mod slots
    for (uint32_t i = 0; i < kNumHeroKnobs; ++i)
    {
        drawKnob(hero[i], activeKnobGroup == 1 && activeKnobIndex == (int)i);
        // mod slots next to knob top
        const float bx = hero[i].x + hero[i].r + 8.0f;
        const float by = hero[i].y - hero[i].r - 2.0f;
        drawModSlotsForParam(hero[i].param, bx, by);
    }

    // bottom strip panel
    const float stripY = waveY + waveH + 18.0f;
    const float stripH = getHeight() - stripY - 18.0f;
    beginPath();
    roundedRect(waveX, stripY, getWidth() - 36.0f, stripH, 14.0f);
    fillColor(0.09f, 0.09f, 0.10f);
    fill();
    strokeColor(0.20f, 0.20f, 0.22f);
    strokeWidth(1.0f);
    stroke();

    for (uint32_t i = 0; i < kNumSmallKnobs; ++i)
        drawKnob(small[i], activeKnobGroup == 2 && activeKnobIndex == (int)i);
}

// ---------------------------
// Wave peaks (unchanged)
// ---------------------------

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

    const uint32_t chunkFrames = 4096;
    std::vector<float> buf;
    buf.resize((size_t)chunkFrames * ch);

    uint64_t frameIndex = 0;
    bool first = true;
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
            if (first)
            {
                waveMin[col] = s;
                waveMax[col] = s;
            }
            else
            {
                waveMin[col] = std::min(waveMin[col], s);
                waveMax[col] = std::max(waveMax[col], s);
            }
            first = false;
        }

        frameIndex += got;
    }

    drwav_uninit(&wav);
}

// ---------------------------
// Grain viz parsing (unchanged)
// ---------------------------

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
        char* end1 = nullptr;
        const float start01 = std::strtof(p, &end1);
        if (end1 == p) break;
        p = end1;
        if (*p != ',') break;
        ++p;

        char* end2 = nullptr;
        const float end01 = std::strtof(p, &end2);
        if (end2 == p) break;
        p = end2;
        if (*p != ',') break;
        ++p;

        char* end3 = nullptr;
        const float age01 = std::strtof(p, &end3);
        if (end3 == p) break;
        p = end3;
        if (*p != ',') break;
        ++p;

        char* end4 = nullptr;
        const float amp01 = std::strtof(p, &end4);
        if (end4 == p) break;
        p = end4;
        if (*p != ',') break;
        ++p;

        char* end5 = nullptr;
        const long voice = std::strtol(p, &end5, 10);
        if (end5 == p) break;
        p = end5;

        activeGrains[activeCount].start01 = fclampf(start01, 0.0f, 1.0f);
        activeGrains[activeCount].end01   = fclampf(end01, 0.0f, 1.0f);
        activeGrains[activeCount].age01   = fclampf(age01, 0.0f, 1.0f);
        activeGrains[activeCount].amp01   = fclampf(amp01, 0.0f, 1.0f);
        activeGrains[activeCount].voice   = (int)voice;
        ++activeCount;

        while (*p == ' ' || *p == '\t') ++p;
        if (*p == ';') { ++p; while (*p == ' ' || *p == '\t') ++p; continue; }
        if (*p == '\0') break;
        break;
    }
}

// ---------------------------
// Mod matrix UI (state-backed)
// ---------------------------

void GristUI::initModDefaults()
{
    for (uint32_t t = 0; t < (uint32_t)ModTarget::COUNT; ++t)
        for (uint32_t s = 0; s < kSlotsPerTarget; ++s)
            mod[t][s] = ModSlot{};

    mod[(uint32_t)ModTarget::Position][0].src = ModSource::LFO1;
    mod[(uint32_t)ModTarget::Position][0].amt = 0.25f;
    mod[(uint32_t)ModTarget::Pitch][0].src = ModSource::LFO2;
    mod[(uint32_t)ModTarget::Pitch][0].amt = 0.10f;

    pushModMatrixState();
}

const char* GristUI::modSourceId(ModSource s) const
{
    switch (s)
    {
    default:
    case ModSource::None: return "none";
    case ModSource::LFO1: return "lfo1";
    case ModSource::LFO2: return "lfo2";
    case ModSource::Env1: return "env1";
    case ModSource::Vel:  return "vel";
    case ModSource::Key:  return "key";
    case ModSource::X:    return "x";
    case ModSource::Y:    return "y";
    case ModSource::M1:   return "m1";
    case ModSource::M2:   return "m2";
    case ModSource::M3:   return "m3";
    case ModSource::M4:   return "m4";
    case ModSource::M5:   return "m5";
    case ModSource::M6:   return "m6";
    case ModSource::M7:   return "m7";
    case ModSource::M8:   return "m8";
    }
}

const char* GristUI::modSourceLabel(ModSource s) const
{
    switch (s)
    {
    default:
    case ModSource::None: return "—";
    case ModSource::LFO1: return "L1";
    case ModSource::LFO2: return "L2";
    case ModSource::Env1: return "E1";
    case ModSource::Vel:  return "Vel";
    case ModSource::Key:  return "Key";
    case ModSource::X:    return "X";
    case ModSource::Y:    return "Y";
    case ModSource::M1:   return "M1";
    case ModSource::M2:   return "M2";
    case ModSource::M3:   return "M3";
    case ModSource::M4:   return "M4";
    case ModSource::M5:   return "M5";
    case ModSource::M6:   return "M6";
    case ModSource::M7:   return "M7";
    case ModSource::M8:   return "M8";
    }
}

GristUI::ModSource GristUI::nextModSource(ModSource s) const
{
    const uint32_t v = (uint32_t)s;
    const uint32_t n = (v + 1) % (uint32_t)ModSource::COUNT;
    return (ModSource)n;
}

bool GristUI::sliderToModTarget(uint32_t param, ModTarget& tgt) const
{
    if (param == kParamPosition)     { tgt = ModTarget::Position;  return true; }
    if (param == kParamGrainSizeMs)  { tgt = ModTarget::GrainSize; return true; }
    if (param == kParamDensity)      { tgt = ModTarget::Density;   return true; }
    if (param == kParamSpray)        { tgt = ModTarget::Spray;     return true; }
    if (param == kParamPitch)        { tgt = ModTarget::Pitch;     return true; }
    return false;
}

void GristUI::pushModMatrixState()
{
    char buf[512];
    uint32_t pos = 0;

    auto append = [&](const char* s) {
        if (!s) return;
        const size_t n = std::strlen(s);
        if (pos + n + 1 >= sizeof(buf)) return;
        std::memcpy(buf + pos, s, n);
        pos += (uint32_t)n;
        buf[pos] = '\0';
    };

    auto appendf = [&](const char* fmt, const char* a, int b, const char* c, float d) {
        if (pos + 32 >= sizeof(buf)) return;
        const int n = std::snprintf(buf + pos, sizeof(buf) - pos, fmt, a, b, c, d);
        if (n > 0) pos += (uint32_t)n;
    };

    auto targetId = [&](ModTarget t) -> const char* {
        switch (t)
        {
        default:
        case ModTarget::Position:  return "pos";
        case ModTarget::GrainSize: return "size";
        case ModTarget::Density:   return "dens";
        case ModTarget::Spray:     return "spray";
        case ModTarget::Pitch:     return "pitch";
        }
    };

    bool first = true;
    for (uint32_t t = 0; t < (uint32_t)ModTarget::COUNT; ++t)
    {
        for (uint32_t s = 0; s < kSlotsPerTarget; ++s)
        {
            const ModSlot& sl = mod[t][s];
            if (sl.src == ModSource::None) continue;
            if (std::fabs(sl.amt) < 1e-6f) continue;

            if (!first) append(";");
            first = false;
            appendf("%s:%d:%s:%.4f", targetId((ModTarget)t), (int)s, modSourceId(sl.src), sl.amt);
        }
    }

    buf[std::min<uint32_t>(pos, (uint32_t)sizeof(buf) - 1)] = '\0';
    setState("mod_matrix", buf);
}

void GristUI::parseModMatrixState(const char* value)
{
    for (uint32_t t = 0; t < (uint32_t)ModTarget::COUNT; ++t)
        for (uint32_t s = 0; s < kSlotsPerTarget; ++s)
            mod[t][s] = ModSlot{};

    if (!value || !value[0])
        return;

    auto parseTarget = [&](const char* id) -> int {
        if (!id) return -1;
        if (std::strcmp(id, "pos") == 0) return (int)ModTarget::Position;
        if (std::strcmp(id, "size") == 0) return (int)ModTarget::GrainSize;
        if (std::strcmp(id, "dens") == 0) return (int)ModTarget::Density;
        if (std::strcmp(id, "spray") == 0) return (int)ModTarget::Spray;
        if (std::strcmp(id, "pitch") == 0) return (int)ModTarget::Pitch;
        return -1;
    };

    auto parseSource = [&](const char* id) -> ModSource {
        if (!id) return ModSource::None;
        if (std::strcmp(id, "lfo1") == 0) return ModSource::LFO1;
        if (std::strcmp(id, "lfo2") == 0) return ModSource::LFO2;
        if (std::strcmp(id, "env1") == 0) return ModSource::Env1;
        if (std::strcmp(id, "vel") == 0)  return ModSource::Vel;
        if (std::strcmp(id, "key") == 0)  return ModSource::Key;
        if (std::strcmp(id, "x") == 0)    return ModSource::X;
        if (std::strcmp(id, "y") == 0)    return ModSource::Y;
        if (std::strcmp(id, "m1") == 0)   return ModSource::M1;
        if (std::strcmp(id, "m2") == 0)   return ModSource::M2;
        if (std::strcmp(id, "m3") == 0)   return ModSource::M3;
        if (std::strcmp(id, "m4") == 0)   return ModSource::M4;
        if (std::strcmp(id, "m5") == 0)   return ModSource::M5;
        if (std::strcmp(id, "m6") == 0)   return ModSource::M6;
        if (std::strcmp(id, "m7") == 0)   return ModSource::M7;
        if (std::strcmp(id, "m8") == 0)   return ModSource::M8;
        return ModSource::None;
    };

    const char* p = value;
    while (*p)
    {
        char tok[96];
        uint32_t ti = 0;
        while (*p && *p != ';' && ti + 1 < sizeof(tok)) tok[ti++] = *p++;
        tok[ti] = '\0';
        if (*p == ';') ++p;

        char* a = tok;
        char* b = std::strchr(a, ':');
        if (!b) continue;
        *b++ = '\0';
        char* c = std::strchr(b, ':');
        if (!c) continue;
        *c++ = '\0';
        char* d = std::strchr(c, ':');
        if (!d) continue;
        *d++ = '\0';

        const int tgt = parseTarget(a);
        const int slot = (int)std::strtol(b, nullptr, 10);
        const ModSource src = parseSource(c);
        const float amt = (float)std::strtof(d, nullptr);

        if (tgt < 0) continue;
        if (slot < 0 || slot >= (int)kSlotsPerTarget) continue;
        mod[(uint32_t)tgt][(uint32_t)slot].src = src;
        mod[(uint32_t)tgt][(uint32_t)slot].amt = fclampf(amt, -1.0f, 1.0f);
    }
}

UI* createUI() { return new GristUI(); }

END_NAMESPACE_DISTRHO
