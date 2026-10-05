#include "Notes.h"

namespace pa
{
// ============================================================================ NoteManager
void NoteManager::reset() noexcept
{
    numHeld = 0;
    sustainDown.fill (false);
    latched.clear(); eff.clear(); lastChord.clear();
    numRecentRel = 0;
    holdingLast = false; replacedFlag = false;
    lastReplaceTime = -1.0e9;
    prevPhys = 0;
}

int NoteManager::physCount() const noexcept
{
    int c = 0;
    for (int i = 0; i < numHeld; ++i)
    {
        bool dup = false;
        for (int j = 0; j < i; ++j) if (held[(size_t) j].note == held[(size_t) i].note) { dup = true; break; }
        if (! dup) ++c;
    }
    return c;
}

void NoteManager::noteOn (Origin o, int ch, int note, int vel, double t) noexcept
{
    if (vel <= 0) { noteOff (o, ch, note, t); return; }
    const int physBefore = physCount();
    int idx = -1;
    for (int i = 0; i < numHeld; ++i)
    {
        auto& h = held[(size_t) i];
        if (h.note == note && h.origin == (uint8_t) o && h.ch == (uint8_t) ch) { idx = i; break; }
    }
    if (idx >= 0)
    {
        auto& h = held[(size_t) idx];
        if (h.sustained) { h.sustained = false; h.count = 0; }
        h.count = (uint8_t) std::min (255, h.count + 1);
        h.vel = (uint8_t) vel;
        h.order = orderCounter++;
    }
    else if (numHeld < (int) held.size())
    {
        held[(size_t) numHeld++] = { (int8_t) note, (uint8_t) o, (uint8_t) ch, 1, false, (uint8_t) vel, orderCounter++ };
    }

    if (latch)
    {
        if (physBefore == 0 && (t - lastReplaceTime) > chordWindow)
        {
            if (latched.n > 0) replacedFlag = true;
            latched.clear();
            lastReplaceTime = t;
        }
        latched.add (note, vel, orderCounter++);
    }
    else if (holdingLast)
    {
        replacedFlag = true;
        holdingLast = false;
        lastChord.clear();
    }
    recompute (t);
}

void NoteManager::noteOff (Origin o, int ch, int note, double t) noexcept
{
    for (int i = 0; i < numHeld; ++i)
    {
        auto& h = held[(size_t) i];
        if (h.note == note && h.origin == (uint8_t) o && h.ch == (uint8_t) ch && ! h.sustained)
        {
            if (h.count > 1) { --h.count; return; }
            if (sustainDown[(size_t) (ch & 15)]) { h.sustained = true; h.count = 0; recompute (t); return; }
            const Rel r { h.note, h.vel, t, h.order };
            for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
            --numHeld;
            if (numRecentRel < (int) recentRel.size()) recentRel[(size_t) numRecentRel++] = r;
            else { for (int j = 0; j < numRecentRel - 1; ++j) recentRel[(size_t) j] = recentRel[(size_t) j + 1]; recentRel[(size_t) numRecentRel - 1] = r; }
            recompute (t);
            return;
        }
    }
}

void NoteManager::sustain (int ch, bool down, double t) noexcept
{
    sustainDown[(size_t) (ch & 15)] = down;
    if (down) return;
    bool changed = false;
    for (int i = 0; i < numHeld;)
    {
        auto& h = held[(size_t) i];
        if (h.sustained && h.ch == (uint8_t) (ch & 15))
        {
            const Rel r { h.note, h.vel, t, h.order };
            for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
            --numHeld;
            if (numRecentRel < (int) recentRel.size()) recentRel[(size_t) numRecentRel++] = r;
            changed = true;
        }
        else ++i;
    }
    if (changed) recompute (t);
}

void NoteManager::allNotesOff() noexcept
{
    numHeld = 0;
    sustainDown.fill (false);
    latched.clear(); lastChord.clear(); eff.clear();
    holdingLast = false;
    numRecentRel = 0;
    prevPhys = 0;
}

void NoteManager::allNotesOff (Origin o) noexcept
{
    for (int i = 0; i < numHeld;)
    {
        if (held[(size_t) i].origin == (uint8_t) o) { for (int j = i; j < numHeld - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --numHeld; }
        else ++i;
    }
    if (numHeld == 0)
    {
        latched.clear(); lastChord.clear(); eff.clear();
        holdingLast = false; numRecentRel = 0; prevPhys = 0;
    }
    else recompute (0.0);
}

void NoteManager::setLatch (bool on) noexcept
{
    if (on == latch) return;
    latch = on;
    latched.clear();
    if (on)
        for (int i = 0; i < numHeld; ++i) latched.add (held[(size_t) i].note, held[(size_t) i].vel, held[(size_t) i].order);
    else if (physCount() == 0 && eff.n > 0 && policy == 0)
    {
        // Leaving latch with no keys down behaves like releasing the latched chord under Hold Last.
        lastChord = eff; holdingLast = true;
    }
    recompute (0.0);
}

void NoteManager::recompute (double t) noexcept
{
    const int phys = physCount();
    if (latch)
    {
        eff = latched;
    }
    else if (phys > 0)
    {
        eff.clear();
        for (int i = 0; i < numHeld; ++i) eff.add (held[(size_t) i].note, held[(size_t) i].vel, held[(size_t) i].order);
        holdingLast = false;
    }
    else
    {
        if (policy == 0)
        {
            if (prevPhys > 0)
            {
                lastChord.clear();
                for (int i = 0; i < numRecentRel; ++i)
                    if (recentRel[(size_t) i].t >= t - 0.12) lastChord.add (recentRel[(size_t) i].note, recentRel[(size_t) i].vel, recentRel[(size_t) i].order);
                if (lastChord.n == 0) lastChord = eff;
                holdingLast = lastChord.n > 0;
            }
            if (holdingLast) eff = lastChord; else eff.clear();
        }
        else
        {
            eff.clear();
            holdingLast = false;
        }
    }
    prevPhys = phys;
}

// ============================================================================ chord / interval builders
void buildChord (const ParamSet& p, NoteSet& out) noexcept
{
    out.clear();
    int notes[8]; int n = 0;
    const int q = p.i (ChQuality);
    if (q == 8)
    {
        for (int k = 0; k < 6; ++k)
        {
            const int v = p.i (ChCustom1 + k);
            if (v >= 0 && v <= 127) notes[n++] = v;
        }
    }
    else
    {
        static const int iv[8][4] = { { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 2, 7, -1 }, { 0, 5, 7, -1 },
                                      { 0, 7, 12, -1 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 14 } };
        const int root = p.i (ChRoot) + 12 * (p.i (ChOctave) + 1);
        for (int k = 0; k < 4; ++k) if (iv[q][k] >= 0) notes[n++] = root + iv[q][k];
    }
    if (n == 0) return;
    std::sort (notes, notes + n);
    // inversion: move lowest notes up an octave
    const int inv = q == 8 ? 0 : p.i (ChInversion) % n;
    for (int k = 0; k < inv; ++k) notes[k] += 12;
    std::sort (notes, notes + n);
    const int spread = p.i (ChSpread);
    if (spread == 1 && n >= 3) notes[1] += 12;
    if (spread == 2) for (int k = 1; k < n; k += 2) notes[k] += 12;
    std::sort (notes, notes + n);
    uint32_t order = 1;
    for (int k = 0; k < n && out.n < kMaxVoices; ++k)
        if (notes[k] >= 0 && notes[k] <= 127) out.add (notes[k], 100, order++);
}

void buildIntervals (const ParamSet& p, int root, int velocity, NoteSet& out) noexcept
{
    out.clear();
    if (root < 0) return;
    const int count = p.i (IntCount);
    uint32_t order = 1;
    if (p.i (IntMode) == 0)
    {
        for (int k = 0; k < count; ++k)
        {
            const int note = root + p.i (Int1 + k);
            if (note >= 0 && note <= 127) out.add (note, velocity, order++);
        }
        return;
    }
    static const int scales[7][8] = {
        { 0, 2, 4, 5, 7, 9, 11, 7 }, { 0, 2, 3, 5, 7, 8, 10, 7 }, { 0, 2, 3, 5, 7, 9, 10, 7 }, { 0, 2, 4, 5, 7, 9, 10, 7 },
        { 0, 2, 3, 5, 7, 8, 11, 7 }, { 0, 2, 4, 7, 9, -1, -1, 5 }, { 0, 3, 5, 7, 10, -1, -1, 5 } };
    const int sc = std::clamp (p.i (IntScale), 0, 6);
    const int len = scales[sc][7];
    const int key = p.i (IntKey);
    // locate root's scale degree (snap down to the nearest scale tone)
    const int rel = ((root - key) % 12 + 12) % 12;
    int deg = 0;
    for (int d = 0; d < len; ++d) if (scales[sc][d] <= rel) deg = d;
    const int octBase = root - rel; // note of key tonic at/below root
    for (int k = 0; k < count; ++k)
    {
        const int step = deg + p.i (Int1 + k);
        const int oct = (int) std::floor ((double) step / len);
        const int d = step - oct * len;
        const int note = octBase + oct * 12 + scales[sc][d];
        if (note >= 0 && note <= 127) out.add (note, velocity, order++);
    }
}

// ============================================================================ Arpeggiator
int Arpeggiator::pickNext (const ParamSet& p, const NoteSet& pool) noexcept
{
    int list[kMaxNotes * 3]; int n = 0;
    const int octs = std::clamp (p.i (ArpOctaves), 1, 3);
    const int mode = p.i (ArpMode);
    NoteEntry base[kMaxNotes]; int nb = 0;
    for (int i = 0; i < pool.n; ++i) base[nb++] = pool.e[(size_t) i];
    if (mode == 3) std::sort (base, base + nb, [] (const NoteEntry& a, const NoteEntry& b) { return a.order < b.order; });
    else std::sort (base, base + nb, [] (const NoteEntry& a, const NoteEntry& b) { return a.note < b.note; });
    for (int o = 0; o < octs; ++o)
        for (int i = 0; i < nb; ++i)
        {
            const int note = base[i].note + 12 * o;
            if (note <= 127 && n < kMaxNotes * 3) list[n++] = note;
        }
    if (n == 0) return -1;
    switch (mode)
    {
        case 1: seqPos = (seqPos + 1) % n; return list[n - 1 - seqPos];
        case 2:
        {
            if (n == 1) return list[0];
            const int period = 2 * n - 2;
            seqPos = (seqPos + 1) % period;
            return seqPos < n ? list[seqPos] : list[period - seqPos];
        }
        case 4: return list[(int) (rng.next() % (uint32_t) n)];
        default: seqPos = (seqPos + 1) % n; return list[seqPos];
    }
}

Arpeggiator::Out Arpeggiator::tick (const ParamSet& p, const NoteSet& pool, double ppq, double bpm) noexcept
{
    Out out;
    double pos;
    if (p.b (ArpSync))
    {
        static const double stepBeatsTab[6] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const double stepBeats = stepBeatsTab[std::clamp (p.i (ArpRate), 0, 5)];
        if (ppq >= 0.0) pos = ppq / stepBeats;
        else pos = samplePos * (bpm / 60.0) / sr / stepBeats;
    }
    else pos = samplePos * p[ArpFreeRate] / sr;
    samplePos += 1.0;

    const double sw = p[ArpSwing] / 100.0 * 0.5;
    const long long s = (long long) std::floor (pos);
    auto onset = [sw] (long long k) { return (double) k + ((k & 1) ? sw : 0.0); };
    long long cur = pos >= onset (s) ? s : s - 1;

    if (pool.n == 0)
    {
        if (gateOn) { gateOn = false; out.changed = true; out.gate = false; out.note = curNote; }
        stepIndex = cur;
        return out;
    }
    if (cur != stepIndex)
    {
        stepIndex = cur;
        const int note = pickNext (p, pool);
        if (note >= 0)
        {
            int vel = 100;
            for (int i = 0; i < pool.n; ++i) if (pool.e[(size_t) i].note % 12 == note % 12) vel = pool.e[(size_t) i].vel;
            const float vv = p[ArpVelVar] / 100.0f;
            vel = std::clamp ((int) ((float) vel * (1.0f - vv * 0.6f * rng.uni())), 1, 127);
            curNote = note; curVel = vel; gateOn = true;
            const double len = onset (cur + 1) - onset (cur);
            gateOffAt = onset (cur) + len * (p[ArpGate] / 100.0);
            out.changed = true; out.gate = true; out.note = note; out.vel = vel;
        }
        return out;
    }
    if (gateOn && pos >= gateOffAt)
    {
        gateOn = false;
        out.changed = true; out.gate = false; out.note = curNote;
    }
    return out;
}

// ============================================================================ VoiceBank
void VoiceBank::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    normCoef = onePoleCoef (0.04f, sr);
    reset();
}

void VoiceBank::reset() noexcept
{
    for (auto& s : slots) s = Slot();
    normSmoothed = 1.0f;
    bendSmoothed = 0.0f;
}

int VoiceBank::allocateSlot (int polyphony) noexcept
{
    int best = -1;
    // 1. free / silent slot
    for (int i = 0; i < polyphony; ++i)
        if (! slots[(size_t) i].gate && ! slots[(size_t) i].stealing && slots[(size_t) i].env < 1.0e-4f) return i;
    // 2. quietest releasing slot
    float quiet = 1.0e9f;
    for (int i = 0; i < polyphony; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (! s.gate && ! s.stealing && s.env < quiet) { quiet = s.env; best = i; }
    }
    if (best >= 0) return best;
    // 3. oldest gated slot
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < polyphony; ++i)
    {
        const auto& s = slots[(size_t) i];
        if (! s.stealing && s.age < oldest) { oldest = s.age; best = i; }
    }
    return best;
}

void VoiceBank::setDesired (const NoteSet& desired, bool replacement, const ParamSet& p) noexcept
{
    const int poly = std::clamp (p.i (Polyphony), 1, kMaxVoices);
    const float transSec = std::max (0.005f, p[Transition] / 1000.0f);
    const float relSec = std::max (0.005f, p[NoteRelease] / 1000.0f);
    const bool glide = p.i (TransitionMode) == 1 && replacement && p[Transition] > 1.0f;

    int releasedNow[kMaxVoices]; int nReleased = 0;
    // release slots whose note is not desired (or above polyphony)
    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& s = slots[(size_t) i];
        if (s.stealing)
        {
            if (s.pendingNote >= 0 && ! desired.contains (s.pendingNote)) s.pendingNote = -1;
            continue;
        }
        if (s.gate && (i >= poly || ! desired.contains (s.note)))
        {
            s.gate = false;
            s.relCoef = t60Coef (replacement ? transSec : relSec, sr);
            releasedNow[nReleased++] = i;
        }
    }
    // gate desired notes
    for (int k = 0; k < desired.n; ++k)
    {
        const int note = desired.e[(size_t) k].note;
        const float vel = 0.3f + 0.7f * (float) desired.e[(size_t) k].vel / 127.0f;
        bool found = false;
        for (int i = 0; i < poly; ++i)
        {
            auto& s = slots[(size_t) i];
            if ((s.note == note && ! s.stealing) || (s.stealing && s.pendingNote == note))
            {
                if (! s.stealing) { s.gate = true; s.vel = vel; }
                found = true;
                break;
            }
        }
        if (found) continue;

        const float tLog = ((float) note - 69.0f) / 12.0f;
        if (glide && nReleased > 0)
        {
            int bi = -1; float bd = 1.0e9f;
            for (int r = 0; r < nReleased; ++r)
            {
                const auto& s = slots[(size_t) releasedNow[r]];
                if (s.note < 0) continue;
                const float d = std::abs ((float) (s.note - note));
                if (d < bd) { bd = d; bi = r; }
            }
            if (bi >= 0)
            {
                auto& s = slots[(size_t) releasedNow[bi]];
                s.note = note; s.gate = true; s.vel = vel;
                s.targetLogHz = tLog;
                s.glideCoef = onePoleCoef (transSec / 3.0f, sr);
                s.age = ageCounter++;
                releasedNow[bi] = releasedNow[--nReleased];
                continue;
            }
        }
        const int idx = allocateSlot (poly);
        if (idx < 0) continue;
        auto& s = slots[(size_t) idx];
        if (s.env > 1.0e-3f)
        {
            s.stealing = true;
            s.pendingNote = note; s.pendingVel = vel;
            s.gate = false;
        }
        else
        {
            s.note = note; s.gate = true; s.vel = vel; s.env = 0.0f;
            s.logHz = s.targetLogHz = tLog; s.glideCoef = 1.0f;
            s.steal = 1.0f; s.retuned = true;
            s.age = ageCounter++;
        }
    }
}

void VoiceBank::killAll (float fadeMs) noexcept
{
    for (auto& s : slots)
    {
        s.gate = false; s.pendingNote = -1;
        s.relCoef = t60Coef (std::max (0.002f, fadeMs / 1000.0f), sr);
    }
}

void VoiceBank::render (int n, const ParamSet& p, float bendSemis) noexcept
{
    n = std::min (n, kChunk);
    attackMs = p[NoteAttack];
    const float atk = onePoleCoef (std::max (0.0005f, attackMs / 1000.0f / 3.0f), sr);
    const float stealStep = 1.0f / (0.008f * (float) sr);
    const float a4Hz = std::max (100.0f, p[RefTuning]);

    float energy = 0.0f;
    for (auto& s : slots) energy += (s.env * s.vel) * (s.env * s.vel);
    const float normTarget = 1.0f / std::sqrt (std::max (1.0f, energy));
    const float bendCoef = onePoleCoef (0.01f, sr);

    for (int i = 0; i < n; ++i)
    {
        normSmoothed += normCoef * (normTarget - normSmoothed);
        bendSmoothed += bendCoef * (bendSemis - bendSmoothed);
        const float bendOct = bendSmoothed / 12.0f;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& s = slots[(size_t) v];
            if (s.stealing)
            {
                s.steal = std::max (0.0f, s.steal - stealStep);
                s.env *= s.relCoef;
            }
            else if (s.gate) s.env += atk * (1.0f - s.env);
            else
            {
                s.env *= s.relCoef;
                if (s.env < 1.0e-5f) s.env = 0.0f;
            }
            s.logHz += (s.targetLogHz - s.logHz) * s.glideCoef;
            gain[v][i] = s.env * s.vel * s.steal * normSmoothed;
            hz[v][i] = s.note >= 0 ? a4Hz * std::exp2 (s.logHz + bendOct) : 0.0f;
            outFade[v][i] = s.steal;
        }
    }
    // finish steals at chunk boundary
    for (auto& s : slots)
    {
        if (s.stealing && s.steal <= 0.0f)
        {
            s.stealing = false;
            s.steal = 1.0f;
            s.env = 0.0f;
            s.retuned = true;
            if (s.pendingNote >= 0)
            {
                s.note = s.pendingNote; s.vel = s.pendingVel; s.gate = true;
                s.logHz = s.targetLogHz = ((float) s.note - 69.0f) / 12.0f;
                s.glideCoef = 1.0f;
                s.age = ageCounter++;
            }
            else { s.note = -1; s.gate = false; }
            s.pendingNote = -1;
        }
        if (! s.gate && ! s.stealing && s.env <= 0.0f && s.note >= 0) { s.note = -1; s.retuned = true; }
    }
}

float VoiceBank::activity() const noexcept
{
    float a = 0.0f;
    for (const auto& s : slots) a += s.env;
    return a;
}

int VoiceBank::lowestGatedNote() const noexcept
{
    int lo = -1;
    for (const auto& s : slots) if (s.gate && s.note >= 0 && (lo < 0 || s.note < lo)) lo = s.note;
    return lo;
}

} // namespace pa
