// pa_bench: processing-time benchmark of representative and worst-case graphs (separate harness, no host).
// Reports per-block average / p95 / p99 / max time as a fraction of the buffer deadline, plus RSS growth.
#include "Core/Engine.h"
#include "Core/Fixtures.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
  #include <sys/resource.h>
#endif

using namespace pa;

namespace
{
long maxRssKb()
{
#if defined(__APPLE__)
    rusage u {}; getrusage (RUSAGE_SELF, &u); return u.ru_maxrss / 1024; // bytes on macOS
#elif defined(__linux__)
    rusage u {}; getrusage (RUSAGE_SELF, &u); return u.ru_maxrss;        // kB on Linux
#else
    return 0;
#endif
}

struct Config { const char* name; void (*setup) (ParamSet&); };

void base (ParamSet& p) { p[Timing] = 1; p[DuckAmount] = 20; }
} // namespace

int main (int argc, char** argv)
{
    double seconds = 20.0, sr = 48000.0;
    int block = 64;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--seconds") && i + 1 < argc) seconds = std::atof (argv[++i]);
        else if (! std::strcmp (argv[i], "--sr") && i + 1 < argc) sr = std::atof (argv[++i]);
        else if (! std::strcmp (argv[i], "--block") && i + 1 < argc) block = std::atoi (argv[++i]);
    }
    const Config configs[] = {
        { "Plate + Classic (32 bands), 3 voices", [] (ParamSet& p) { base (p); p[DelayEnable] = 0; p[ReverbMode] = 0; p[HarmMethod] = 1; } },
        { "Wash + Classic (32 bands), 3 voices", [] (ParamSet& p) { base (p); p[DelayEnable] = 0; p[ReverbMode] = 1; p[HarmMethod] = 1; } },
        { "Wash + Classic (48 bands, High quality), 3 voices", [] (ParamSet& p) { base (p); p[DelayEnable] = 0; p[ReverbMode] = 1; p[HarmMethod] = 1; p[ClBands] = 2; p[Quality] = 2; } },
        { "Plate + Classic (24 bands, Eco), 6 voices", [] (ParamSet& p) { base (p); p[DelayEnable] = 0; p[ReverbMode] = 0; p[HarmMethod] = 1; p[ClBands] = 0; p[Quality] = 0; } },
        { "Interval Clock/Reverse 3 taps + Wash + Classic 48", [] (ParamSet& p) { base (p); p[DelayMode] = 1; p[IvPitchMode] = 1; p[IvDirection] = 2; p[ReverbMode] = 1; p[HarmMethod] = 1; p[ClBands] = 2; } },
        { "Worst case: BBD+Wash parallel, 2x Classic 48 High, 6 voices", [] (ParamSet& p) { base (p); p[ReverbMode] = 1; p[HarmMethod] = 1; p[ClBands] = 2; p[Quality] = 2; p[Routing] = 0; } },
        { "Hall + Shimmer + Classic (32 bands), 3 voices", [] (ParamSet& p) { base (p); p[DelayEnable] = 0; p[ReverbMode] = 2; p[Shimmer] = 50; p[HarmMethod] = 1; } },
        { "Tape (3 heads) + Hall, harmony off", [] (ParamSet& p) { base (p); p[DelayMode] = 2; p[TpHeads] = 6; p[ReverbMode] = 2; p[HarmEnable] = 0; } },
        { "Worst case: Tape 3 heads + Wash + Shimmer, 2x Classic 48 High, 6 voices", [] (ParamSet& p) { base (p); p[DelayMode] = 2; p[TpHeads] = 6; p[ReverbMode] = 1; p[Shimmer] = 100; p[HarmMethod] = 1; p[ClBands] = 2; p[Quality] = 2; } },
        { "Worst case: Interval Stable 3 taps + Wash, 2x Classic 48, 6 voices", [] (ParamSet& p) { base (p); p[DelayMode] = 1; p[ReverbMode] = 1; p[HarmMethod] = 1; p[ClBands] = 2; p[Quality] = 2; } },
    };
    std::vector<float> fl, fr;
    generateFixture (Fixture::BreathNoise, sr, fl, fr);
    std::printf ("Playable Ambience benchmark: %.0f Hz, block %d, %.0f s audio per config (Release build)\n", sr, block, seconds);
    std::printf ("%-66s %8s %8s %8s %8s %10s %8s %9s\n", "configuration", "avg", "p95", "p99", "max", "RSS+MiB", "overruns", "warm-max");
    const double deadline = block / sr;
    for (const auto& c : configs)
    {
        const long rss0 = maxRssKb();
        auto eng = std::make_unique<Engine>();
        eng->prepare (sr, block);
        ParamSet p;
        c.setup (p);
        const bool six = std::strstr (c.name, "6 voices") != nullptr;
        std::vector<MidiEvent> ev;
        const int notes6[6] = { 48, 51, 55, 58, 62, 67 }, notes3[3] = { 48, 51, 55 };
        for (int k = 0; k < (six ? 6 : 3); ++k) ev.push_back ({ 0, 0x90, (uint8_t) (six ? notes6[k] : notes3[k]), 100, Origin::Host });
        const long rss1 = maxRssKb();
        const int total = (int) (seconds * sr);
        std::vector<float> inL ((size_t) block), inR ((size_t) block), oL ((size_t) block), oR ((size_t) block);
        std::vector<double> t;
        t.reserve ((size_t) (total / block + 1));
        TransportInfo tp;
        for (int pos = 0, b = 0; pos < total; pos += block, ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const size_t s = (size_t) ((pos + i) % (int) fl.size());
                inL[(size_t) i] = fl[s]; inR[(size_t) i] = fr[s];
            }
            const auto a = std::chrono::steady_clock::now();
            eng->process (p, tp, inL.data(), inR.data(), oL.data(), oR.data(), block, b == 0 ? ev.data() : nullptr, b == 0 ? (int) ev.size() : 0);
            const auto z = std::chrono::steady_clock::now();
            t.push_back (std::chrono::duration<double> (z - a).count() / deadline);
        }
        // discard the first 0.5 s (first-touch page faults / cache warm-up), report it separately
        const size_t warm = std::min (t.size(), (size_t) (0.5 * sr / block));
        double warmMax = 0; for (size_t k = 0; k < warm; ++k) warmMax = std::max (warmMax, t[k]);
        t.erase (t.begin(), t.begin() + (long) warm);
        size_t over = 0; for (double x : t) if (x >= 1.0) ++over;
        std::sort (t.begin(), t.end());
        double sum = 0; for (double x : t) sum += x;
        const auto at = [&t] (double q) { return t[std::min (t.size() - 1, (size_t) (q * (double) t.size()))]; };
        std::printf ("%-66s %7.1f%% %7.1f%% %7.1f%% %7.1f%% %10.1f %8zu %8.0f%%\n", c.name, 100.0 * sum / (double) t.size(), 100.0 * at (0.95), 100.0 * at (0.99),
                     100.0 * t.back(), (double) (std::max (rss1, rss0) - rss0) / 1024.0, over, 100.0 * warmMax);
    }
    std::printf ("\nValues are the share of the %d-sample deadline (%.3f ms) used by one engine instance on this machine after a 0.5 s warm-up;\n"
                 "'overruns' counts post-warm-up blocks at or above the deadline; 'warm-max' is the worst warm-up block.\n", block, deadline * 1000.0);
    return 0;
}
