#ifndef GRIST_MOD_MATRIX_HPP_INCLUDED
#define GRIST_MOD_MATRIX_HPP_INCLUDED

#include <cstdint>
#include <cstring>

// Minimal RT-safe modulation matrix (routing is configured non-RT).
//
// The idea is Lemondrop-like “slots per parameter” without declaring thousands of automatable params.
// Routing is stored in a single state string (mod_matrix) and parsed on the message thread.

namespace GristMod {

static constexpr uint32_t kMaxSlotsPerTarget = 3;

enum class Source : uint8_t {
    None = 0,
    LFO1,
    LFO2,
    Env1,
    Velocity,
    Keytrack,
    X,
    Y,
    Macro1,
    Macro2,
    Macro3,
    Macro4,
    Macro5,
    Macro6,
    Macro7,
    Macro8,
    COUNT
};

enum class Target : uint8_t {
    Position = 0,   // 0..1
    GrainSizeMs,    // ms
    Density,        // grains/sec
    Spray,          // 0..1
    Pitch,          // semitones
    SampleStart,    // 0..1 offset applied to grain playback start boundary
    SampleEnd,      // 0..1 offset applied to grain playback end boundary
    COUNT
};

struct Slot {
    Source source = Source::None;
    float amount = 0.0f; // -1..+1
};

struct Matrix {
    Slot slots[(uint32_t)Target::COUNT][kMaxSlotsPerTarget];

    void clear() {
        for (uint32_t t = 0; t < (uint32_t)Target::COUNT; ++t)
            for (uint32_t s = 0; s < kMaxSlotsPerTarget; ++s)
                slots[t][s] = Slot{};
    }

    static inline bool streq(const char* a, const char* b) {
        return a && b && std::strcmp(a,b)==0;
    }

    static Source parseSource(const char* id) {
        if (!id) return Source::None;
        if (streq(id, "lfo1")) return Source::LFO1;
        if (streq(id, "lfo2")) return Source::LFO2;
        if (streq(id, "env1")) return Source::Env1;
        if (streq(id, "vel"))  return Source::Velocity;
        if (streq(id, "key"))  return Source::Keytrack;
        if (streq(id, "x"))    return Source::X;
        if (streq(id, "y"))    return Source::Y;
        if (streq(id, "m1"))   return Source::Macro1;
        if (streq(id, "m2"))   return Source::Macro2;
        if (streq(id, "m3"))   return Source::Macro3;
        if (streq(id, "m4"))   return Source::Macro4;
        if (streq(id, "m5"))   return Source::Macro5;
        if (streq(id, "m6"))   return Source::Macro6;
        if (streq(id, "m7"))   return Source::Macro7;
        if (streq(id, "m8"))   return Source::Macro8;
        return Source::None;
    }

    static Target parseTarget(const char* id) {
        if (!id) return Target::Position;
        if (streq(id, "pos"))   return Target::Position;
        if (streq(id, "size"))  return Target::GrainSizeMs;
        if (streq(id, "dens"))  return Target::Density;
        if (streq(id, "spray")) return Target::Spray;
        if (streq(id, "pitch")) return Target::Pitch;
        if (streq(id, "sstart")) return Target::SampleStart;
        if (streq(id, "send"))   return Target::SampleEnd;
        return Target::Position;
    }
};

} // namespace GristMod

#endif
