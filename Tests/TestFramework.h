// Minimal test harness (no external dependencies).
#pragma once
#include <atomic>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace pat
{
struct TestCase { std::string name; std::function<void()> fn; };
std::vector<TestCase>& registry();
struct Registrar { Registrar (const char* n, std::function<void()> f) { registry().push_back ({ n, std::move (f) }); } };
void fail (const char* file, int line, const std::string& msg);
void note (const std::string& msg);
void metric (const std::string& key, double value, const std::string& unit = "");
extern std::atomic<long> gAllocCount;
extern std::atomic<bool> gAllocArmed;
} // namespace pat

#define PA_CAT2(a, b) a##b
#define PA_CAT(a, b) PA_CAT2 (a, b)
#define TEST(name) static void PA_CAT (test_, __LINE__) (); static pat::Registrar PA_CAT (reg_, __LINE__) (name, PA_CAT (test_, __LINE__)); static void PA_CAT (test_, __LINE__) ()
#define CHECK(cond) do { if (! (cond)) pat::fail (__FILE__, __LINE__, #cond); } while (0)
#define CHECK_MSG(cond, msg) do { if (! (cond)) pat::fail (__FILE__, __LINE__, std::string (#cond) + " :: " + (msg)); } while (0)
