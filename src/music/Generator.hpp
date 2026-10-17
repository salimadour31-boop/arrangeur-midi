// Generator.hpp — real-time MIDI arranger: one key -> chord (scale or progression mode), voicing and voice
// leading, strum, tempo-synced arpeggiator (patterns, rate, swing, octaves, gate), bass line, humanize.
// Sample-accurate: every action is scheduled in a time-sorted queue and fired when its sample is reached.
// No allocation after construction. MIT licence.
#pragma once
#include "Theory.hpp"
#include <cmath>

namespace am {

enum Mode { mScale = 0, mProgression, mThru, kNumModes };
enum KeyMap { kmWhiteKeys = 0, kmSnap };
enum ArpPattern { apUp = 0, apDown, apUpDown, apRandom, apThumb, apBroken, apChord, kNumArpPatterns };
static const char* const kArpFr[] = {"Montant", "Descendant", "Montant-descendant", "Aléatoire", "Pouce (basse alternée)", "Brisé (1-3-2-4)", "Accord (rythme)"};
static const char* const kArpEn[] = {"Up", "Down", "Up-down", "Random", "Thumb (alternating bass)", "Broken (1-3-2-4)", "Chord (rhythm)"};
static const char* const kArpKab[] = {"Yettali", "Yettrusu", "Yettali-yettrusu", "S ccekk", "Adebbuz", "Yerreẓ (1-3-2-4)", "Akurd (riṭm)"};
static const double kRateBeats[] = {4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125};
static const char* const kRateNames[] = {"1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
constexpr int kNumRates = 8;
enum BassMode { bmOff = 0, bmHeld, bmBeats, bmRootFifth, bmOctaves, bmSyncopated, kNumBassModes };
static const char* const kBassFr[] = {"Désactivée", "Fondamentale tenue", "Noires", "Fondamentale-quinte", "Octaves (croches)", "Syncopée"};
static const char* const kBassEn[] = {"Off", "Held root", "Quarter notes", "Root-fifth", "Octaves (8ths)", "Syncopated"};
static const char* const kBassKab[] = {"Tensa", "Tasarut teṭṭef", "Tiknariyin", "Tasarut-tis xemsa", "Ttamiwin", "Tasinkupt"};
enum StrumDir { sdDown = 0, sdUp, sdAlternate };

struct Settings {
    int mode = mScale, keyMap = kmWhiteKeys, tonic = 0, scale = 0, chordType = ctTriad, preset = 0;
    int voicing = vClose, inversion = 0, octave = 0; bool voiceLead = true; bool latch = false;
    float strumMs = 0; int strumDir = sdDown;
    bool arpOn = false; int arpPattern = apUp, arpRate = 5, arpOctaves = 1; float swing = 0, gate = 0.8f;
    int bassMode = bmOff;
    float humVel = 0, humTimeMs = 0, velocity = 0;   // velocity 0 = follow the keyboard, else fixed 1..127
    int chChord = 0, chBass = 1;                      // MIDI channels (0-based)
};

struct Transport { bool playing = false; double bpm = 120; double beat = 0; /* beat position at frame 0 */ };
struct MidiOut { uint32_t frame; uint8_t data[3]; };

class Generator {
public:
    static constexpr int kQueue = 1024;

    void setSampleRate(double sr) { sr_ = sr > 0 ? sr : 48000; }
    Settings& settings() { return s_; }
    const Settings& settings() const { return s_; }
    uint32_t rngState() const { return rng_; }
    void seed(uint32_t s) { rng_ = s ? s : 1; }

    // Current chord (for the UI): root note (-1 = none), quality code, voiced notes.
    int currentRoot() const { return active_ ? cur_.root : -1; }
    int currentQuality() const { return active_ ? identifyQuality(raw_) : -1; }
    const Chord& currentVoiced() const { return cur_; }
    bool chordActive() const { return active_; }

    // ---- input (call in time order within the block, frames relative to block start) ----
    void noteOn(int key, int vel, uint32_t frame) { if (nIn_ < kIn) in_[nIn_++] = {frame, true, key, vel}; }
    void noteOff(int key, uint32_t frame) { if (nIn_ < kIn) in_[nIn_++] = {frame, false, key, 0}; }

    // Stop everything now (transport jump, settings change that requires it, deactivate).
    void panic(MidiOut* out, uint32_t& nOut, uint32_t maxOut) {
        nq_ = 0; held_ = 0; active_ = false; group_++;
        for (int c = 0; c < 16; ++c) for (int n = 0; n < 128; ++n)
            if (on_[c][n]) { on_[c][n] = false; push(out, nOut, maxOut, 0, uint8_t(0x80 | c), uint8_t(n), 0); }
    }

    // Process one block; writes time-sorted MIDI events (frames relative to block start).
    void process(uint32_t frames, const Transport& tr, MidiOut* out, uint32_t& nOut, uint32_t maxOut) {
        nOut = 0;
        bpm_ = tr.bpm > 1 ? tr.bpm : 120;
        const double bps = bpm_ / 60.0 / sr_;           // beats per sample
        if (tr.playing) blockBeat_ = tr.beat;
        else            blockBeat_ = freeBeat_;
        // host loop / jump backwards / start: forget the last fired steps so the pattern keeps going
        if (blockBeat_ < expectBeat_ - 1e-6 || tr.playing != playing_) { lastStepBeat_ = -1e9; lastBassBeat_ = -1e9; }
        expectBeat_ = blockBeat_ + frames * bps;
        playing_ = tr.playing;
        uint32_t seg = 0;
        for (int e = 0; e <= nIn_; ++e) {
            const uint32_t segEnd = e < nIn_ ? std::min(in_[e].frame, frames) : frames;
            if (segEnd > seg) { scheduleSteps(seg, segEnd, bps); seg = segEnd; }
            if (e < nIn_) handleInput(in_[e], bps);
        }
        nIn_ = 0;
        if (!tr.playing) freeBeat_ = blockBeat_ + frames * bps;
        flush(time_ + frames, out, nOut, maxOut);
        time_ += frames;
    }

private:
    struct In { uint32_t frame; bool on; int key, vel; };
    enum EvKind : uint8_t { evRelease = 0, evOff = 1, evOn = 2 };
    struct Ev { int64_t t; EvKind kind; uint8_t ch, note, vel; uint32_t group, gen; };

    // ---- helpers ----
    uint32_t rnd() { rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_; }
    float rndf() { return float(rnd() & 0xFFFFFF) / float(0xFFFFFF); }      // 0..1
    double beatAt(uint32_t frame, double bps) const { return blockBeat_ + frame * bps; }
    int64_t humanizeT(int64_t t) { return s_.humTimeMs > 0 ? t + int64_t(rndf() * s_.humTimeMs * 0.001 * sr_) : t; }
    uint8_t vel(int base, int accent = 0) {
        float v = s_.velocity >= 1 ? s_.velocity : float(base);
        v += float(accent);
        if (s_.humVel > 0) v += (rndf() * 2 - 1) * s_.humVel * 30.f;
        return uint8_t(std::max(1, std::min(127, int(std::lround(v)))));
    }
    void schedule(const Ev& e) {
        if (nq_ >= kQueue) return;                   // queue full: drop (never blocks)
        int i = nq_++;
        while (i > 0 && (q_[i - 1].t > e.t || (q_[i - 1].t == e.t && q_[i - 1].kind > e.kind))) { q_[i] = q_[i - 1]; --i; }
        q_[i] = e;
    }
    static void push(MidiOut* out, uint32_t& n, uint32_t maxN, uint32_t frame, uint8_t a, uint8_t b, uint8_t c) {
        if (n < maxN) { out[n].frame = frame; out[n].data[0] = a; out[n].data[1] = b; out[n].data[2] = c; ++n; }
    }

    void flush(int64_t until, MidiOut* out, uint32_t& nOut, uint32_t maxOut) {
        int k = 0;
        for (; k < nq_ && q_[k].t < until; ++k) {
            const Ev& e = q_[k];
            const uint32_t f = uint32_t(std::max<int64_t>(0, e.t - time_));
            if (e.kind == evRelease) {
                for (int c = 0; c < 16; ++c) for (int n = 0; n < 128; ++n)
                    if (on_[c][n] && owner_[c][n] == e.group) { on_[c][n] = false; push(out, nOut, maxOut, f, uint8_t(0x80 | c), uint8_t(n), 0); }
            } else if (e.kind == evOff) {
                if (on_[e.ch][e.note] && gen_[e.ch][e.note] == e.gen) { on_[e.ch][e.note] = false; push(out, nOut, maxOut, f, uint8_t(0x80 | e.ch), e.note, 0); }
            } else {
                if (!active_ || e.group != group_) continue;                       // chord released before this note fired
                if (on_[e.ch][e.note]) push(out, nOut, maxOut, f, uint8_t(0x80 | e.ch), e.note, 0);   // retrigger
                on_[e.ch][e.note] = true; owner_[e.ch][e.note] = e.group; gen_[e.ch][e.note] = e.gen;
                push(out, nOut, maxOut, f, uint8_t(0x90 | e.ch), e.note, e.vel);
            }
        }
        for (int i = k; i < nq_; ++i) q_[i - k] = q_[i];
        nq_ -= k;
    }

    void noteAt(int64_t t, int ch, int note, uint8_t v, int64_t offAt) {
        if (note < 0 || note > 127) return;
        const uint32_t g = ++genCounter_;
        schedule({t, evOn, uint8_t(ch), uint8_t(note), v, group_, g});
        if (offAt > t) schedule({offAt, evOff, uint8_t(ch), uint8_t(note), 0, group_, g});
    }

    // ---- chord construction ----
    Chord buildRaw(int key) const {
        const int center = 60 + 12 * s_.octave;
        if (s_.mode == mProgression) {
            const Preset& p = kPresets[std::max(0, std::min(kNumPresets - 1, s_.preset))];
            static const int idx[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
            const int i = idx[pcOf(key)] % p.n;
            Chord c = qualityChord(48 + s_.tonic + p.c[i].off, p.c[i].q);
            (void)center;
            return c;
        }
        const Scale& sc = kScales[std::max(0, std::min(kNumScales - 1, s_.scale))];
        const int deg = s_.keyMap == kmWhiteKeys ? whiteKeyDegree(sc, key) : snapToDegree(sc, s_.tonic, key) - 5 * sc.n;
        return diatonicChord(sc, s_.tonic, deg, ChordType(s_.chordType), 60);
    }

    void startChord(int key, int velIn, int64_t t, double beat) {
        const bool wasActive = active_;
        if (wasActive) schedule({t, evRelease, 0, 0, 0, group_, 0});
        ++group_;
        raw_ = buildRaw(key);
        const int center = 60 + 12 * s_.octave;
        cur_ = voiceChord(raw_, Voicing(s_.voicing), s_.inversion, center, s_.voiceLead, hasPrev_ ? &prev_ : nullptr);
        prev_ = cur_; hasPrev_ = true;
        bassNote_ = std::max(24, 36 + pcOf(raw_.root) + 12 * std::min(0, s_.octave));   // C2..B2 (lower if octave < 0)
        active_ = true; chordVel_ = velIn; arpIdx_ = 0; strumFlip_ = !strumFlip_;
        if (!s_.arpOn) playBlock(t, uint8_t(velIn), -1);
        // tempo-synced parts: if the host transport is stopped, restart the internal clock on the first key
        if (!playing_ && !wasActive) { blockBeat_ += -beat; chordStartBeat_ = 0; }
        else chordStartBeat_ = beat;
        if (s_.bassMode == bmHeld) noteAt(humanizeT(t), s_.chBass, bassNote_, vel(velIn), -1);
        lastStepBeat_ = -1e9; lastBassBeat_ = -1e9;
        // with the host playing, a key pressed just after a grid step still gets that step (tolerance)
        if (s_.arpOn && playing_) {
            const double L = kRateBeats[std::max(0, std::min(kNumRates - 1, s_.arpRate))];
            const double k = std::floor(beat / L), since = beat - k * L, tol = std::min(0.25 * L, 0.03 * bpm_ / 60.0);
            if (since < tol) arpStep(t, int64_t(k), L);
        }
    }

    void playBlock(int64_t t, uint8_t baseVel, int64_t offAt) {
        // strum: spread the note-ons over strumMs (down = low -> high)
        const int n = cur_.n;
        bool up = s_.strumDir == sdUp || (s_.strumDir == sdAlternate && strumFlip_);
        for (int i = 0; i < n; ++i) {
            const int idx = up ? n - 1 - i : i;
            const double d = n > 1 ? s_.strumMs * 0.001 * sr_ * i / (n - 1) : 0;
            noteAt(humanizeT(t + int64_t(d)), s_.chChord, cur_.notes[idx], vel(baseVel), offAt);
        }
    }

    void stopChord(int64_t t) {
        if (!active_) return;
        schedule({t, evRelease, 0, 0, 0, group_, 0});
        active_ = false;
        ++group_;
    }

    void handleInput(const In& e, double bps) {
        const int64_t t = time_ + e.frame;
        const double beat = beatAt(e.frame, bps);
        if (s_.mode == mThru) {
            const uint32_t g = ++genCounter_;
            if (e.on) { active_ = true; schedule({t, evOn, uint8_t(s_.chChord), uint8_t(e.key), vel(e.vel), group_, g}); thruGen_[e.key & 127] = g; }
            else schedule({t, evOff, uint8_t(s_.chChord), uint8_t(e.key), 0, group_, thruGen_[e.key & 127]});
            return;
        }
        if (e.on) {
            if (held_ < kHeld) heldKeys_[held_++] = e.key;
            startChord(e.key, e.vel, t, beat);
            lastKey_ = e.key;
        } else {
            int w = 0; for (int i = 0; i < held_; ++i) if (heldKeys_[i] != e.key) heldKeys_[w++] = heldKeys_[i];
            held_ = w;
            if (s_.latch) return;
            if (held_ == 0) stopChord(t);
            else if (e.key == lastKey_) { lastKey_ = heldKeys_[held_ - 1]; startChord(lastKey_, chordVel_, t, beat); }
        }
    }

    // ---- tempo-synced steps (arpeggiator and rhythmic bass) inside [f0, f1) ----
    double stepBeat(int64_t k, double L) const { return k * L + ((k & 1) ? s_.swing * L * 0.5 : 0.0); }

    void scheduleSteps(uint32_t f0, uint32_t f1, double bps) {
        if (!active_) return;
        const double b0 = beatAt(f0, bps), b1 = beatAt(f1, bps);
        auto frameOf = [&](double b) { return time_ + int64_t(std::llround((b - blockBeat_) / bps)); };
        if (s_.arpOn) {
            const double L = kRateBeats[std::max(0, std::min(kNumRates - 1, s_.arpRate))];
            for (int64_t k = int64_t(std::floor(b0 / L)) - 1; stepBeat(k, L) < b1; ++k) {
                const double sb = stepBeat(k, L);
                if (sb < b0 - 1e-9 || sb <= lastStepBeat_ + 1e-9) continue;
                arpStep(frameOf(sb), k, L);
            }
        }
        if (s_.bassMode >= bmBeats) bassSteps(b0, b1, frameOf, bps);
    }

    void arpStep(int64_t t, int64_t k, double L) {
        lastStepBeat_ = stepBeat(k, L);
        const int64_t len = int64_t(std::max(0.02, double(s_.gate)) * L * 60.0 / bpm_ * sr_);
        const bool onBeat = std::fabs(std::fmod(lastStepBeat_, 1.0)) < 1e-6;
        const uint8_t v = vel(chordVel_, onBeat ? 8 : 0);
        if (s_.arpPattern == apChord) { playBlock(t, v, t + len); ++arpIdx_; return; }
        const int note = arpNote(cur_, s_.arpPattern, std::max(1, s_.arpOctaves), arpIdx_++);
        noteAt(humanizeT(t), s_.chChord, note, v, t + len);
    }

public:
    // Note for arpeggio step `i` (exposed for tests). Notes are the voiced chord spread over `oct` octaves.
    int arpNote(const Chord& c, int pattern, int oct, int i) {
        int seq[kMaxChordNotes * 4]; int n = 0;
        Chord s = c; s.sort();
        for (int o = 0; o < oct && o < 4; ++o) for (int j = 0; j < s.n; ++j) seq[n++] = s.notes[j] + 12 * o;
        if (n == 0) return -1;
        switch (pattern) {
            case apDown: return seq[n - 1 - i % n];
            case apUpDown: { if (n == 1) return seq[0]; const int p = 2 * n - 2, k = i % p; return seq[k < n ? k : p - k]; }
            case apRandom: return seq[rnd() % uint32_t(n)];
            case apThumb: { if (n == 1) return seq[0]; return (i & 1) ? seq[1 + (i / 2) % (n - 1)] : seq[0]; }
            case apBroken: { // 0,2,1,3,2,4 ... : up a third, back a step
                             const int k = i % n, base = k / 2, j = (k & 1) ? base + 2 : base; return seq[j % n]; }
            case apUp: default: return seq[i % n];
        }
    }
private:
    template <class F> void bassSteps(double b0, double b1, F frameOf, double) {
        struct B { double pos, len; int kind; };   // kind 0 root, 1 fifth, 2 octave
        static const B beats[] = {{0, .9, 0}, {1, .9, 0}, {2, .9, 0}, {3, .9, 0}};
        static const B r5[] = {{0, .9, 0}, {1, .9, 1}, {2, .9, 0}, {3, .9, 1}};
        static const B oct[] = {{0, .45, 0}, {.5, .45, 2}, {1, .45, 0}, {1.5, .45, 2}, {2, .45, 0}, {2.5, .45, 2}, {3, .45, 0}, {3.5, .45, 2}};
        static const B sync[] = {{0, .7, 0}, {.75, .5, 0}, {1.5, .4, 1}, {2, .7, 0}, {2.75, .5, 0}, {3.5, .4, 2}};
        const B* pat = beats; int n = 4;
        if (s_.bassMode == bmRootFifth) { pat = r5; n = 4; }
        else if (s_.bassMode == bmOctaves) { pat = oct; n = 8; }
        else if (s_.bassMode == bmSyncopated) { pat = sync; n = 6; }
        for (int64_t bar = int64_t(std::floor(b0 / 4.0)); bar * 4.0 < b1; ++bar)
            for (int i = 0; i < n; ++i) {
                double pb = bar * 4.0 + pat[i].pos;
                if (std::fmod(pat[i].pos, 1.0) == 0.5) pb += s_.swing * 0.25;      // swing the off-beat 8ths
                if (pb < b0 - 1e-9 || pb >= b1 || pb <= lastBassBeat_ + 1e-9) continue;
                lastBassBeat_ = pb;
                const int64_t t = frameOf(pb);
                const int note = bassNote_ + (pat[i].kind == 1 ? 7 : pat[i].kind == 2 ? 12 : 0);
                noteAt(humanizeT(t), s_.chBass, note, vel(chordVel_, std::fmod(pat[i].pos, 1.0) == 0 ? 6 : -6),
                       t + int64_t(pat[i].len * 60.0 / bpm_ * sr_));
            }
    }

    // ---- state ----
    static constexpr int kIn = 512, kHeld = 16;
    Settings s_;
    double sr_ = 48000, bpm_ = 120, blockBeat_ = 0, freeBeat_ = 0, chordStartBeat_ = 0, lastStepBeat_ = -1e9, lastBassBeat_ = -1e9, expectBeat_ = 0;
    bool playing_ = false, active_ = false, hasPrev_ = false, strumFlip_ = false;
    int64_t time_ = 0;
    In in_[kIn]; int nIn_ = 0;
    Ev q_[kQueue]; int nq_ = 0;
    int heldKeys_[kHeld] = {}; int held_ = 0, lastKey_ = -1, chordVel_ = 100, arpIdx_ = 0, bassNote_ = 36;
    Chord raw_, cur_, prev_;
    uint32_t group_ = 1, genCounter_ = 0, rng_ = 0x2545F491u;
    bool on_[16][128] = {}; uint32_t owner_[16][128] = {}, gen_[16][128] = {}, thruGen_[128] = {};
};

} // namespace am
