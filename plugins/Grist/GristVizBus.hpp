/*
 * GristVizBus.hpp
 *
 * CLAP backend in DPF currently does not propagate updateStateValue() to the UI.
 * For real-time visualizations we use a simple in-process shared bus between
 * DSP (audio thread) and UI thread.
 *
 * NOTE: This is best-effort and currently single-instance oriented.
 */

#ifndef GRIST_VIZ_BUS_HPP_INCLUDED
#define GRIST_VIZ_BUS_HPP_INCLUDED

#include <atomic>
#include <cstdint>

struct GristVizBus
{
    static constexpr uint32_t kMaxSpawn = 64;
    static constexpr uint32_t kMaxActive = 64;

    struct Active
    {
        float start01;
        float end01;
        float age01;
        float amp01;
        uint32_t voice;
    };

    std::atomic<uint32_t> spawnSeq {0};
    std::atomic<uint32_t> activeSeq {0};

    // Audio thread writes, UI reads (single-writer/single-reader).
    uint32_t spawnCount = 0;
    float spawnPos[kMaxSpawn] = {};

    uint32_t activeCount = 0;
    Active active[kMaxActive] = {};

    // singleton
    static GristVizBus& instance() noexcept
    {
        static GristVizBus bus;
        return bus;
    }

    // audio thread: overwrite current spawn list then bump seq
    void publishSpawn(const float* pos01, uint32_t count) noexcept
    {
        if (count > kMaxSpawn) count = kMaxSpawn;
        spawnCount = count;
        for (uint32_t i = 0; i < count; ++i)
            spawnPos[i] = pos01[i];
        spawnSeq.fetch_add(1, std::memory_order_release);
    }

    // audio thread: overwrite active snapshot then bump seq
    void publishActive(const Active* a, uint32_t count) noexcept
    {
        if (count > kMaxActive) count = kMaxActive;
        activeCount = count;
        for (uint32_t i = 0; i < count; ++i)
            active[i] = a[i];
        activeSeq.fetch_add(1, std::memory_order_release);
    }

    // UI thread: copy if changed
    bool copySpawnIfNew(uint32_t& lastSeq, float* outPos01, uint32_t& outCount) noexcept
    {
        const uint32_t seq = spawnSeq.load(std::memory_order_acquire);
        if (seq == lastSeq)
            return false;
        lastSeq = seq;
        outCount = spawnCount;
        if (outCount > kMaxSpawn) outCount = kMaxSpawn;
        for (uint32_t i = 0; i < outCount; ++i)
            outPos01[i] = spawnPos[i];
        return true;
    }

    bool copyActiveIfNew(uint32_t& lastSeq, Active* out, uint32_t& outCount) noexcept
    {
        const uint32_t seq = activeSeq.load(std::memory_order_acquire);
        if (seq == lastSeq)
            return false;
        lastSeq = seq;
        outCount = activeCount;
        if (outCount > kMaxActive) outCount = kMaxActive;
        for (uint32_t i = 0; i < outCount; ++i)
            out[i] = active[i];
        return true;
    }
};

#endif // GRIST_VIZ_BUS_HPP_INCLUDED
