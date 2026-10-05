// Lock-free audio -> UI telemetry. Audio thread only writes relaxed atomics; UI reads and may drop frames.
#pragma once

#include <array>
#include <atomic>

namespace pa
{
struct Telemetry
{
    static constexpr int kHistLen = 4096;      // history buckets
    static constexpr int kBucket = 256;        // samples per bucket
    static constexpr int kSpecLen = 16384;     // wet mono samples for UI spectrum analysis

    std::atomic<float> inPeakL { 0 }, inPeakR { 0 }, outPeakL { 0 }, outPeakR { 0 }, wetPeak { 0 };
    std::array<std::atomic<float>, kHistLen> histDelay {}, histReverb {}, histWet {}, histDry {};
    std::atomic<int> histWrite { 0 };
    std::array<std::atomic<float>, kSpecLen> spec {};
    std::atomic<int> specWrite { 0 };

    std::array<std::atomic<int>, 6> voiceNote {};
    std::array<std::atomic<float>, 6> voiceEnv {};
    std::array<std::atomic<bool>, 6> voiceGate {};
    std::atomic<int> midiCounter { 0 };
    std::atomic<int> noteEventCounter { 0 };
    std::atomic<bool> protectionActive { false };
    std::atomic<int> protectionCount { 0 };
    std::atomic<int> nonfiniteCount { 0 };
    std::atomic<int> wetLatency { 0 };
    std::atomic<int> dryLatency { 0 };
    std::atomic<float> delayTimeMs { 375 };
    std::atomic<int> snapshotRequest { -1 };
    std::atomic<int> desiredMask0 { 0 }, desiredMask1 { 0 }, desiredMask2 { 0 }, desiredMask3 { 0 }; // 128-bit note mask
    std::atomic<int> activeMethod { 1 };
    std::atomic<bool> delayFrozen { false }, reverbFrozen { false };
    std::atomic<bool> holdingLast { false };
    std::atomic<float> sampleRate { 48000 };
    std::atomic<int> clearing { 0 };
    std::atomic<int> arpNote { -1 };
    std::atomic<float> wetEnergy { 0 };   // sum of squares since last read (UI exchanges with 0)
    std::atomic<int> wetEnergyCount { 0 };

    static void maxStore (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};
} // namespace pa
