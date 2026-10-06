#include "Params.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace pa
{
namespace
{
const ParamInfo kInfos[kNumParams] = {
#define PA_INFO(e, id, name, kind, mn, mx, df, ce, unit, ch, grp, help) \
    ParamInfo { id, name, Kind::kind, (float) (mn), (float) (mx), (float) (df), (float) (ce), unit, ch, grp, help },
    PA_PARAMS (PA_INFO)
#undef PA_INFO
};

const std::unordered_map<std::string, int>& idMap()
{
    static const std::unordered_map<std::string, int> m = [] {
        std::unordered_map<std::string, int> r;
        for (int i = 0; i < kNumParams; ++i) r[kInfos[i].id] = i;
        return r;
    }();
    return m;
}

float skewFor (const ParamInfo& p) noexcept
{
    if (p.centre <= p.minV || p.centre >= p.maxV || p.kind != Kind::Float) return 1.0f;
    return (float) (std::log (0.5) / std::log ((p.centre - p.minV) / (p.maxV - p.minV)));
}

const char* noteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
} // namespace

const ParamInfo& paramInfo (int index) noexcept { return kInfos[index]; }

int paramIndexForId (const std::string& id) noexcept
{
    auto it = idMap().find (id);
    return it == idMap().end() ? -1 : it->second;
}

std::vector<std::string> paramChoices (int index)
{
    std::vector<std::string> out;
    const char* s = kInfos[index].choices;
    if (s == nullptr || *s == 0) return out;
    std::string cur;
    for (; *s; ++s)
    {
        if (*s == '|') { out.push_back (cur); cur.clear(); }
        else cur.push_back (*s);
    }
    out.push_back (cur);
    return out;
}

float paramSnap (int index, float v) noexcept
{
    const auto& p = kInfos[index];
    if (! std::isfinite (v)) v = p.def;
    v = std::fmin (std::fmax (v, p.minV), p.maxV);
    if (p.kind != Kind::Float) v = std::round (v);
    return v;
}

float paramToNormalised (int index, float value) noexcept
{
    const auto& p = kInfos[index];
    value = paramSnap (index, value);
    const float prop = (value - p.minV) / (p.maxV - p.minV);
    const float skew = skewFor (p);
    return skew == 1.0f ? prop : std::pow (prop, skew);
}

float paramFromNormalised (int index, float norm) noexcept
{
    const auto& p = kInfos[index];
    norm = std::fmin (std::fmax (norm, 0.0f), 1.0f);
    const float skew = skewFor (p);
    const float prop = skew == 1.0f ? norm : std::pow (norm, 1.0f / skew);
    return paramSnap (index, p.minV + (p.maxV - p.minV) * prop);
}

std::string paramValueToText (int index, float v)
{
    const auto& p = kInfos[index];
    v = paramSnap (index, v);
    char buf[64];
    switch (p.kind)
    {
        case Kind::Bool: return v >= 0.5f ? "On" : "Off";
        case Kind::Choice:
        {
            auto ch = paramChoices (index);
            const int i = (int) v;
            return (i >= 0 && i < (int) ch.size()) ? ch[(size_t) i] : std::to_string (i);
        }
        case Kind::Int:
        {
            const int i = (int) v;
            if (std::strcmp (p.unit, "note") == 0)
            {
                if (i < 0) return "Off";
                std::snprintf (buf, sizeof (buf), "%s%d", noteNames[i % 12], i / 12 - 1);
                return buf;
            }
            if (std::strcmp (p.unit, "st") == 0) { std::snprintf (buf, sizeof (buf), "%+d st", i); return buf; }
            if (index == MidiChannel && i == 0) return "Omni";
            if (index >= Int1 && index <= Int6) { std::snprintf (buf, sizeof (buf), "%+d", i); return buf; }
            return std::to_string (i);
        }
        case Kind::Float:
        default:
        {
            const std::string u = p.unit;
            if (u == "dB")
            {
                if (v <= -59.95f) return "-inf dB";
                std::snprintf (buf, sizeof (buf), "%.1f dB", v);
                return buf;
            }
            if (u == "Hz")
            {
                if (v >= 1000.0f) std::snprintf (buf, sizeof (buf), "%.1f kHz", v / 1000.0f);
                else if (v < 10.0f) std::snprintf (buf, sizeof (buf), "%.2f Hz", v);
                else std::snprintf (buf, sizeof (buf), "%.0f Hz", v);
                return buf;
            }
            if (u == "ms")
            {
                if (v >= 1000.0f) std::snprintf (buf, sizeof (buf), "%.2f s", v / 1000.0f);
                else std::snprintf (buf, sizeof (buf), "%.0f ms", v);
                return buf;
            }
            if (u == "s") { std::snprintf (buf, sizeof (buf), v < 10 ? "%.1f s" : "%.0f s", v); return buf; }
            if (u == "%") { std::snprintf (buf, sizeof (buf), "%.0f %%", v); return buf; }
            if (u == "st") { std::snprintf (buf, sizeof (buf), "%+.1f st", v); return buf; }
            if (u == "ct") { std::snprintf (buf, sizeof (buf), "%.0f ct", v); return buf; }
            if (u == "x") { std::snprintf (buf, sizeof (buf), "%.2fx", v); return buf; }
            if (u == "BPM") { std::snprintf (buf, sizeof (buf), "%.1f BPM", v); return buf; }
            std::snprintf (buf, sizeof (buf), "%.2f", v);
            return buf;
        }
    }
}

bool paramTextToValue (int index, const std::string& textIn, float& out)
{
    const auto& p = kInfos[index];
    std::string t;
    for (char c : textIn) if (c != ' ') t.push_back (c);
    if (t.empty()) return false;
    if (p.kind == Kind::Bool)
    {
        if (t == "On" || t == "on" || t == "1") { out = 1; return true; }
        if (t == "Off" || t == "off" || t == "0") { out = 0; return true; }
        return false;
    }
    if (p.kind == Kind::Choice)
    {
        auto ch = paramChoices (index);
        for (size_t i = 0; i < ch.size(); ++i)
        {
            std::string c;
            for (char x : ch[i]) if (x != ' ') c.push_back (x);
            if (c == t) { out = (float) i; return true; }
        }
        char* end = nullptr;
        const long v = std::strtol (t.c_str(), &end, 10);
        if (end != t.c_str()) { out = paramSnap (index, (float) v); return true; }
        return false;
    }
    if (std::strcmp (p.unit, "note") == 0 && ! t.empty() && std::isalpha ((unsigned char) t[0]))
    {
        for (int pass = 0; pass < 2; ++pass)
        for (int n = 11; n >= 0; --n)
        {
            const size_t len = std::strlen (noteNames[n]);
            if (len != (pass == 0 ? 2u : 1u)) continue;
            if (t.compare (0, len, noteNames[n]) == 0)
            {
                const int oct = std::atoi (t.c_str() + len);
                out = paramSnap (index, (float) ((oct + 1) * 12 + n));
                return true;
            }
        }
        return false;
    }
    if (t == "-inf" || t == "-infdB") { out = p.minV; return true; }
    char* end = nullptr;
    double v = std::strtod (t.c_str(), &end);
    if (end == t.c_str()) return false;
    const std::string suffix (end);
    if (suffix.rfind ("k", 0) == 0 && std::strcmp (p.unit, "Hz") == 0) v *= 1000.0;
    if (suffix == "s" && std::strcmp (p.unit, "ms") == 0) v *= 1000.0;
    out = paramSnap (index, (float) v);
    return true;
}

ParamSet::ParamSet()
{
    for (int k = 0; k < kNumParams; ++k) v[(size_t) k] = kInfos[k].def;
}

double divisionBeats (int index) noexcept
{
    static const double beats[15] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0, 3.0, 4.0, 8.0 };
    if (index < 0) index = 0;
    if (index > 14) index = 14;
    return beats[index];
}

void migrateLegacyHarmony (ParamSet& ps) noexcept
{
    const int m = ps.i (HarmMethod);
    if (m == 0) { ps[HarmEnable] = 0.0f; ps[HarmMethod] = 1.0f; }
    else if (m != 1) ps[HarmMethod] = 1.0f;
}

int effectiveHarmonyMethod (const ParamSet& ps) noexcept
{
    return (ps.b (HarmEnable) && ps.i (HarmMethod) != 0) ? 1 : 0; // MethodClassic : MethodOff
}

} // namespace pa
