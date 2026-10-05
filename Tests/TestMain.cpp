#include "TestFramework.h"

#include <chrono>
#include <cstring>

namespace pat
{
std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
static int failures = 0;
static std::string current;
static FILE* report = nullptr;
void fail (const char* file, int line, const std::string& msg)
{
    ++failures;
    std::printf ("    FAIL %s:%d  %s\n", file, line, msg.c_str());
    if (report) std::fprintf (report, "    FAIL %s:%d %s\n", file, line, msg.c_str());
}
void note (const std::string& msg)
{
    std::printf ("    note: %s\n", msg.c_str());
    if (report) std::fprintf (report, "    note: %s\n", msg.c_str());
}
void metric (const std::string& key, double value, const std::string& unit)
{
    std::printf ("    metric %s = %.6g %s\n", key.c_str(), value, unit.c_str());
    if (report) std::fprintf (report, "    metric %s = %.6g %s\n", key.c_str(), value, unit.c_str());
}
} // namespace pat

int main (int argc, char** argv)
{
    const char* filter = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--report") == 0 && i + 1 < argc) pat::report = std::fopen (argv[++i], "w");
        else filter = argv[i];
    }
    int run = 0, failedTests = 0;
    for (auto& t : pat::registry())
    {
        if (filter && t.name.find (filter) == std::string::npos) continue;
        const int before = pat::failures;
        const auto t0 = std::chrono::steady_clock::now();
        std::printf ("[ RUN  ] %s\n", t.name.c_str());
        if (pat::report) std::fprintf (pat::report, "[ RUN  ] %s\n", t.name.c_str());
        t.fn();
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        const bool ok = pat::failures == before;
        std::printf ("[ %s ] %s (%.0f ms)\n", ok ? " OK " : "FAIL", t.name.c_str(), ms);
        if (pat::report) std::fprintf (pat::report, "[ %s ] %s (%.0f ms)\n", ok ? " OK " : "FAIL", t.name.c_str(), ms);
        ++run;
        if (! ok) ++failedTests;
    }
    std::printf ("\n%d tests, %d failed, %d check failures\n", run, failedTests, pat::failures);
    if (pat::report) { std::fprintf (pat::report, "\n%d tests, %d failed, %d check failures\n", run, failedTests, pat::failures); std::fclose (pat::report); }
    return failedTests == 0 ? 0 : 1;
}
