# Linux Audio Plugin Development Guide

A comprehensive guide to developing native audio plugins on Linux, with a focus on creating a guitar distortion plugin.

---

## Table of Contents

1. [Plugin Formats](#plugin-formats)
2. [Development Frameworks](#development-frameworks)
3. [DSP Libraries](#dsp-libraries)
4. [Build Tools & Dependencies](#build-tools--dependencies)
5. [Project-Specific Practical Notes (This Repo)](#project-specific-practical-notes-this-repo)
6. [Testing & Debugging Tools](#testing--debugging-tools)
7. [Guitar Distortion: Algorithms & Techniques](#guitar-distortion-algorithms--techniques)
8. [Open Source Reference Projects](#open-source-reference-projects)
9. [Recommended Stack for Guitar Distortion Plugin](#recommended-stack-for-guitar-distortion-plugin)

---

## Plugin Formats

### LV2 (LADSPA Version 2)
- **Recommended for Linux** - The native, royalty-free open standard
- Platform-agnostic with liberal licensing
- Excellent host support: Ardour, Carla, Qtractor, Guitarix, REAPER
- Defined in two parts: C/C++ code + Turtle (.ttl) metadata files
- Supports audio, CV, MIDI, OSC, and custom extensions
- **Resources:**
  - [Official LV2 Documentation](https://lv2plug.in/)
  - [Programming LV2 Plugins Book](https://lv2plug.in/book/)
  - [LV2 Tutorial Series (GitHub)](https://github.com/sjaehn/lv2tutorial)

### VST3
- Steinberg's open-source format (MIT license as of October 2025)
- Wide DAW compatibility across all platforms
- Good choice for cross-platform distribution
- **Resources:**
  - [VST3 SDK (GitHub)](https://github.com/steinbergmedia/vst3sdk)

### CLAP (CLever Audio Plugin)
- **Newest format** - Open source, MIT licensed
- No fees, memberships, or proprietary agreements
- Modern design with excellent multi-threading support
- Growing adoption (Bitwig, REAPER, u-he plugins)
- **Resources:**
  - [CLAP GitHub](https://github.com/free-audio/clap)
  - [CLAP Database](https://clapdb.tech/)
  - [Awesome Linux CLAP List](https://github.com/RustoMCSpit/awesome-linux-clap-list)

### LADSPA (Legacy)
- Original Linux audio plugin API
- Simple but limited (no MIDI, no presets, no GUI)
- Still supported by most hosts
- Good for learning basics

### Format Comparison

| Format | License | GUI Support | MIDI | Presets | Recommended |
|--------|---------|-------------|------|---------|-------------|
| LV2    | ISC     | Yes         | Yes  | Yes     | **Yes**     |
| VST3   | MIT     | Yes         | Yes  | Yes     | **Yes**     |
| CLAP   | MIT     | Yes         | Yes  | Yes     | **Yes**     |
| LADSPA | LGPL    | No          | No   | No      | Legacy only |

---

## Development Frameworks

### 1. JUCE (Recommended for Beginners)
The most widely used framework for audio plugin development.

**Pros:**
- Cross-platform (Linux, macOS, Windows, iOS, Android)
- Supports VST2, VST3, AU, AUv3, LV2, AAX, CLAP (v9+)
- Comprehensive GUI toolkit
- Large community and extensive documentation
- Built-in DSP classes

**Cons:**
- Commercial licensing for closed-source products
- Large binary size
- Steeper learning curve for advanced features

**Linux Requirements:**
```bash
# Debian/Ubuntu
sudo apt install build-essential g++ libasound2-dev libjack-jackd2-dev \
    ladspa-sdk libcurl4-openssl-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
    libglu1-mesa-dev mesa-common-dev
```

**Resources:**
- [JUCE Official](https://juce.com/)
- [JUCE GitHub](https://github.com/juce-framework/JUCE)
- [JUCE Tutorials](https://juce.com/tutorials/)
- [Building LV2 with JUCE](https://jatinchowdhury18.medium.com/building-lv2-plugins-with-juce-and-cmake-d1f8937dbac3)

### 2. DPF (DISTRHO Plugin Framework)
Lightweight, open-source alternative optimized for Linux.

**Pros:**
- Fully open source (ISC/MIT license)
- Native support for LADSPA, DSSI, LV2, VST2, VST3, CLAP
- Smaller binary size than JUCE
- JACK standalone mode for quick testing
- Strong Linux community

**Cons:**
- Smaller ecosystem than JUCE
- Less documentation
- More "raw" - requires more manual work

**Getting Started:**
```bash
# Add DPF as a submodule
git submodule add https://github.com/DISTRHO/DPF dpf
```

**Resources:**
- [DPF Documentation](https://distrho.github.io/DPF/)
- [DPF GitHub](https://github.com/DISTRHO/DPF)
- [DPF Tutorial](https://dev.to/reis0/audio-plugin-developmento-with-dpf-first-plugin-1jd0)
- [Cookiecutter DPF Effect Template](https://github.com/SpotlightKid/cookiecutter-dpf-effect)

### 3. LVTK (LV2 Toolkit)
Minimal C++ header-only library specifically for LV2.

**Pros:**
- Lightweight and focused
- Header-only (easy integration)
- Pure LV2 - no abstraction overhead

**Cons:**
- LV2 only
- No built-in GUI toolkit

**Resources:**
- [LVTK GitHub](https://github.com/lvtk/lvtk)

### 4. iPlug2
Cross-platform framework targeting multiple APIs.

**Note:** Linux support is limited - not officially supported as a primary platform. Better suited for macOS/Windows development with Linux as secondary target.

**Resources:**
- [iPlug2 GitHub](https://github.com/iPlug2/iPlug2)

### 5. FAUST (Functional Audio Stream)
Domain-specific language for DSP that compiles to C++.

**Pros:**
- Write DSP in functional style
- Compiles to highly optimized C++
- Can target multiple plugin formats
- Extensive standard library
- Great for prototyping algorithms

**Cons:**
- Learning new language
- Less control over low-level implementation
- GUI requires additional framework

**Resources:**
- [FAUST Official](https://faust.grame.fr/)
- [FAUST GitHub](https://github.com/grame-cncm/faust)
- [Audio Signal Processing in FAUST (Stanford)](https://ccrma.stanford.edu/~jos/aspf/)

### Framework Comparison

| Framework | License       | Formats                        | Best For              |
|-----------|---------------|--------------------------------|-----------------------|
| JUCE      | GPL/Commercial| VST2/3, AU, LV2, AAX, CLAP    | Production plugins    |
| DPF       | ISC/MIT       | LADSPA, LV2, VST2/3, CLAP     | Open source, Linux    |
| LVTK      | ISC           | LV2 only                       | Pure LV2 plugins      |
| FAUST     | GPL           | Multiple via backends          | DSP prototyping       |

---

## DSP Libraries

### Oversampling Libraries
Essential for distortion plugins to avoid aliasing artifacts.

**HIIR (Half-band IIR filters)**
- High-quality IIR half-band filtering
- SIMD optimizations
- WTFPL license (very permissive)
- Commonly used for 2x, 4x, 8x, 16x oversampling

### General DSP Libraries

**Q DSP Library**
- Cross-platform C++ audio DSP
- Modern C++ design
- Efficient for embedded systems
- [GitHub](https://github.com/cycfi/q)

**SignalSmithDSP**
- Clean, well-documented DSP library
- Excellent accompanying articles
- Modern C++ patterns

**DSPFilters**
- MIT licensed IIR filter library
- By Vinno Falco
- Comprehensive filter implementations

### JUCE DSP Module
If using JUCE, the built-in `juce::dsp` module provides:
- `Oversampling<>` class
- `WaveShaper<>` class
- IIR/FIR filters
- FFT
- Convolution

### Curated Resource
- [Awesome MusicDSP](https://github.com/olilarkin/awesome-musicdsp) - Comprehensive list of DSP resources

---

## Build Tools & Dependencies

### Build Systems

**CMake (Recommended)**
```bash
sudo apt install cmake
```
- Industry standard
- JUCE native support
- Cross-platform

**Meson**
```bash
sudo apt install meson ninja-build
```
- Fast builds
- Clean syntax
- Good for LV2 projects

**Make**
- Simple projects
- DPF uses Makefiles

### Essential Linux Packages

```bash
# Debian/Ubuntu - Complete development environment
sudo apt install \
    build-essential \
    cmake \
    meson \
    ninja-build \
    pkg-config \
    git \
    # Audio libraries
    libasound2-dev \
    libjack-jackd2-dev \
    libpulse-dev \
    # LV2 development
    lv2-dev \
    lilv-utils \
    ladspa-sdk \
    # GUI dependencies
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxcomposite-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    # Additional for JUCE
    libfreetype-dev \
    libfontconfig1-dev \
    libcurl4-openssl-dev \
    libwebkit2gtk-4.1-dev
```

```bash
# Arch Linux
sudo pacman -S \
    base-devel \
    cmake \
    meson \
    ninja \
    git \
    alsa-lib \
    jack2 \
    lv2 \
    lilv \
    libx11 \
    mesa
```

```bash
# Fedora
sudo dnf install \
    gcc-c++ \
    cmake \
    meson \
    ninja-build \
    git \
    alsa-lib-devel \
    jack-audio-connection-kit-devel \
    lv2-devel \
    lilv-devel \
    libX11-devel \
    mesa-libGL-devel
```

### Project Structure Example

```
my-distortion-plugin/
├── CMakeLists.txt          # or meson.build
├── src/
│   ├── PluginProcessor.cpp
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp    # GUI
│   └── PluginEditor.h
├── resources/
│   └── plugin.ttl          # LV2 metadata
├── dpf/                    # or JUCE/ (as submodule)
└── README.md
```

---

## Project-Specific Practical Notes (This Repo)

This repository is a DPF-based distortion plugin named `Grist`. The core DSP lives in `plugins/Grist/Grist.cpp`, with helpers in `plugins/Grist/DSP/`.

### Build and Outputs

```bash
# Build all formats (LV2, VST2, VST3, CLAP, JACK)
make

# Generate LV2 TTL metadata after building
make lv2-ttl

# Install into user plugin folders
make install
```

- Outputs land in `bin/`: `Grist.lv2`, `Grist.vst3`, `Grist-vst.so`, and `Grist.clap`.
- UI builds require OpenGL; without it, the Makefile builds DSP-only formats.
- `make compdb` generates a compilation database for IDE tooling.

### DSP Signal Chain (As Implemented)

```
Input -> Gate -> Low Cut (HPF) -> Input Gain
     -> Pre-Emphasis High Shelf (2.5kHz)
     -> 4x Oversample -> Asymmetric Waveshaper + 2nd Harmonic
     -> Tone LPF (500-16kHz, Q 0.5-8)
     -> Presence High Shelf (3kHz)
     -> Output HPF (200Hz) -> DC Block (10Hz)
     -> Output Gain (with internal -36 dB pad) -> Limiter -> Mix
```

Practical notes from the current implementation:
- The noise gate uses a fast attack (~1 ms) and slow release (~100 ms) envelope follower. It also attenuates the dry path for clean parallel blending.
- The waveshaper is asymmetric (0.8 / -0.72 thresholds) with a soft knee and a small even-harmonic term; a final `tanh` catches overshoots.
- Oversampling is 4x using cascaded 2-pole IIR half-band filters for both up and down sampling.

### Parameters and UI Sync Points

When adding or changing parameters, keep these files in sync:
- `plugins/Grist/DistrhoPluginInfo.h` for `kParam*` ordering and `kParamCount`.
- `plugins/Grist/Grist.cpp` for defaults and ranges in `initParameter`.
- `plugins/Grist/GristUI.cpp` for slider layout, normalization, and log scaling.

The UI uses a custom NanoVG layout with 8 sliders; `Tone` and `Low Cut` are logarithmic, so their normalization math must match the DSP ranges.

### Where to Tweak Tone Quickly

- Low cut corner: `Grist::updateFilters()` -> `inputHighPass`.
- Tone range and resonance: `toneFilter` setup in `updateFilters()`.
- Presence boost amount: `highBoost.setHighShelf(3000.0f, 1.3f, ...)`.
- Internal headroom: `internalCut` is `-36 dB` inside `run()` to avoid clipping when drive is high.

---

## Testing & Debugging Tools

### Plugin Hosts

**Carla**
- Full-featured plugin host
- Supports VST2, VST3, LV2, LADSPA, CLAP
- Patchbay interface
- Essential for testing
```bash
sudo apt install carla
# With PipeWire
pw-jack carla
```

**JUCE AudioPluginHost**
- Build from JUCE source
- Good for JUCE plugin debugging
```bash
cd JUCE/extras/AudioPluginHost/Builds/LinuxMakefile
make
```

**Jalv**
- Minimal LV2 host
- Command-line based
- Good for automated testing
```bash
sudo apt install jalv
jalv.gtk3 http://example.org/myplugin
```

### Audio Routing

**PipeWire** (Modern default)
```bash
# Check status
wpctl status
# GUI patchbay
qpwgraph
```

**JACK**
```bash
# Start JACK server
jackd -d alsa -r 48000 -p 256
# GUI connections
qjackctl
```

**Helvum**
- GTK patchbay for PipeWire
```bash
flatpak install flathub org.pipewire.Helvum
```

### DAWs for Testing

- **Ardour** - Professional, open source
- **REAPER** - Commercial, excellent Linux support
- **Qtractor** - Lightweight, Qt-based
- **Bitwig Studio** - Commercial, native CLAP support
- **Zrythm** - Modern, open source

### Debugging Tools

```bash
# Check plugin metadata
lv2info http://example.org/myplugin

# List installed LV2 plugins
lv2ls

# Validate LV2 plugin
lv2lint /path/to/plugin.lv2

# GDB debugging
gdb --args carla
```

---

## Guitar Distortion: Algorithms & Techniques

### Waveshaping Fundamentals

Waveshaping applies a transfer function to shape the input signal, adding harmonic content.

#### Common Transfer Functions

```cpp
// Hard clipping
float hardClip(float x, float threshold) {
    return std::clamp(x, -threshold, threshold);
}

// Soft clipping with tanh
float tanhSoftClip(float x, float gain) {
    return std::tanh(x * gain);
}

// Arctangent soft clipping
float atanSoftClip(float x, float gain) {
    return (2.0f / M_PI) * std::atan(x * gain);
}

// Asymmetric clipping (tube-like, adds even harmonics)
float asymmetricClip(float x, float gain) {
    float shaped = std::tanh(x * gain);
    // Add slight asymmetry
    return shaped + 0.1f * shaped * shaped;
}

// Polynomial waveshaper
float cubicWaveshaper(float x) {
    // Attempt soft knee: x - x³/3
    if (x > 1.0f) return 2.0f/3.0f;
    if (x < -1.0f) return -2.0f/3.0f;
    return x - (x * x * x) / 3.0f;
}
```

#### Chebyshev Polynomials
For precise harmonic control:
- T1(x) = x (fundamental)
- T2(x) = 2x² - 1 (2nd harmonic)
- T3(x) = 4x³ - 3x (3rd harmonic)
- T4(x) = 8x⁴ - 8x² + 1 (4th harmonic)

### Dealing with Aliasing

**The Problem:** Non-linear functions (distortion) create harmonics that can exceed the Nyquist frequency, causing aliasing artifacts.

**Solution: Oversampling**
```cpp
// Typical oversampling workflow
void processBlock(float* buffer, int numSamples) {
    // 1. Upsample (e.g., 4x)
    oversampler.upsample(buffer, numSamples);

    // 2. Apply distortion at higher sample rate
    float* oversampledBuffer = oversampler.getBuffer();
    int oversampledLength = numSamples * oversamplingFactor;

    for (int i = 0; i < oversampledLength; ++i) {
        oversampledBuffer[i] = tanhSoftClip(oversampledBuffer[i], gain);
    }

    // 3. Downsample back
    oversampler.downsample(buffer, numSamples);
}
```

**Common Oversampling Factors:**
- 2x: Minimal, noticeable aliasing with high gain
- 4x: Good balance of quality and CPU
- 8x: High quality
- 16x: Professional quality, CPU intensive

### Pre/Post Filtering

**Pre-filtering (Tone Shaping)**
- Low-pass before distortion: Darker, smoother
- High-pass before distortion: Tighter bass
- Mid boost: Classic tube screamer character

**Post-filtering (Harsh Frequency Removal)**
- Low-pass around 5-8kHz: Remove harsh harmonics
- Presence boost around 2-4kHz: Clarity

### Signal Chain

Typical guitar distortion signal flow:
```
Input → High-Pass (remove DC/sub) →
Pre-EQ (tone shaping) →
Gain Stage →
Waveshaper (with oversampling) →
Post-EQ (harshness removal) →
Output Level → Output
```

### Dynamic Behavior

Static waveshapers can sound "digital." Real analog circuits have:
- **Soft knee**: Gradual transition to clipping
- **Asymmetry**: Different positive/negative clipping (even harmonics)
- **Filter interaction**: Capacitors create frequency-dependent behavior
- **Bias drift**: DC offset changes with signal level

---

## Open Source Reference Projects

### Guitar-Specific Plugins

| Project | Framework | Formats | Description |
|---------|-----------|---------|-------------|
| [GxPlugins.lv2](https://github.com/brummer10/GxPlugins.lv2) | Custom | LV2 | Guitarix distortion plugins |
| [Kapitonov-Plugins-Pack](https://github.com/olegkapitonov/Kapitonov-Plugins-Pack) | Custom | LV2 | Guitar processing suite |
| [Tamgamp.lv2](https://github.com/sadko4u/tamgamp.lv2) | Custom | LV2 | Amp simulator |
| [Misstortion1](https://github.com/pdesaulniers/misstortion1) | DPF | LV2 | Distortion (GPL v3) |
| [infamousPlugins](https://github.com/ssj71/infamousPlugins) | Custom | LV2 | Hip2B destroyer |

### General Plugin Examples

| Project | Framework | Description |
|---------|-----------|-------------|
| [DISTRHO Cardinal](https://github.com/DISTRHO/Cardinal) | DPF | VCV Rack as plugin |
| [Dragonfly Reverb](https://github.com/michaelwillis/dragonfly-reverb) | DPF | High-quality reverb |
| [LSP Plugins](https://github.com/sadko4u/lsp-plugins) | Custom | Professional suite |
| [Airwindows](https://github.com/airwindows/airwindows) | Various | 300+ plugins |

---

## Recommended Stack for Guitar Distortion Plugin

### Option A: Quick Start with DPF (Recommended)

Best for: Open source project, Linux-first, lightweight

```bash
# 1. Clone template
git clone https://github.com/DISTRHO/DPF
# Or use cookiecutter template
pip install cookiecutter
cookiecutter https://github.com/SpotlightKid/cookiecutter-dpf-effect

# 2. Build
make

# 3. Test with JACK standalone
./bin/MyDistortion
```

**Output Formats:** LV2, VST2, VST3, CLAP, JACK standalone

### Option B: JUCE (Most Versatile)

Best for: Cross-platform, GUI-heavy, commercial potential

```bash
# 1. Clone JUCE
git clone https://github.com/juce-framework/JUCE.git

# 2. Build Projucer
cd JUCE/extras/Projucer/Builds/LinuxMakefile
make

# 3. Create new project
./Projucer

# 4. Or use CMake directly with juce_add_plugin()
```

**Output Formats:** VST2, VST3, AU, LV2, AAX, CLAP (v9+)

### Option C: Pure LV2 with LVTK

Best for: Learning LV2, minimal dependencies

```bash
# Follow lv2tutorial
git clone https://github.com/sjaehn/lv2tutorial
```

### Minimum Viable Distortion Plugin Features

1. **Input Gain** - Pre-amplification (0-40 dB)
2. **Drive/Distortion** - Amount of waveshaping
3. **Tone** - Simple low-pass or tilt EQ
4. **Output Level** - Makeup gain (-inf to 0 dB)
5. **Mix** - Dry/wet blend

### Advanced Features (Phase 2)

- Multiple distortion algorithms (tube, transistor, fuzz)
- 3-band EQ
- Noise gate
- Cabinet simulation (convolution)
- Oversampling quality selector
- Preset system

---

## Quick Reference Commands

```bash
# Install everything on Ubuntu/Debian
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev \
    lv2-dev lilv-utils carla ardour qjackctl

# Clone DPF and start coding
git clone https://github.com/DISTRHO/DPF
cd DPF/examples
make

# Test LV2 plugin
lv2ls | grep -i distortion
jalv.gtk3 http://your-plugin-uri

# Run Carla with PipeWire
pw-jack carla
```

---

## Sources & Further Reading

- [Linux Audio Development](https://linuxaudio.dev/)
- [LV2 Official](https://lv2plug.in/)
- [JUCE Official](https://juce.com/)
- [DPF Documentation](https://distrho.github.io/DPF/)
- [CLAP Official](https://github.com/free-audio/clap)
- [FAUST Official](https://faust.grame.fr/)
- [KXStudio](https://kx.studio/)
- [Awesome MusicDSP](https://github.com/olilarkin/awesome-musicdsp)
- [Elementary Audio - Distortion Tutorial](https://www.elementary.audio/docs/tutorials/distortion-saturation-wave-shaping)
- [Arch Wiki - Audio Plugins](https://wiki.archlinux.org/title/Audio_plugins_package_guidelines)
- [PipeWire Guide](https://github.com/mikeroyal/PipeWire-Guide)
