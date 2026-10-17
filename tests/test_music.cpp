// test_music.cpp — offline tests of the music logic (no host): scales, chords, presets, voicings, voice
// leading, strum, arpeggiator timing/patterns/swing/gate, bass lines, humanize bounds, stuck-note fuzzing,
// MIDI file export, preview synth.
#include "music/MidiFile.hpp"
#include "music/Synth.hpp"
#include <cstdio>
#include <map>
#include <set>
#include <vector>

using namespace am;
static int g_fail = 0, g_pass = 0;
#define CHECK(c, ...) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::set<int> pcs(const Chord& c) { std::set<int> s; for (int i = 0; i < c.n; ++i) s.insert(pcOf(c.notes[i])); return s; }
static std::set<int> S(std::initializer_list<int> l) { std::set<int> s; for (int x : l) s.insert(pcOf(x)); return s; }

struct Ev { int64_t t; uint8_t st, note, vel; };
// Drive a generator: list of (sample, key, on/off); returns all output events with absolute times.
struct Drive { int64_t t; int key; bool on; };
static std::vector<Ev> run(Generator& g, std::vector<Drive> in, int64_t total, bool playing = true, double bpm = 120, uint32_t block = 256) {
    std::vector<Ev> out; MidiOut buf[4096];
    const double spb = 48000.0 * 60.0 / bpm;
    size_t k = 0;
    for (int64_t t = 0; t < total; t += block) {
        while (k < in.size() && in[k].t < t + block) { const uint32_t f = uint32_t(std::max<int64_t>(0, in[k].t - t)); if (in[k].on) g.noteOn(in[k].key, 100, f); else g.noteOff(in[k].key, f); ++k; }
        Transport tr; tr.playing = playing; tr.bpm = bpm; tr.beat = t / spb;
        uint32_t n = 0; g.process(block, tr, buf, n, 4096);
        uint32_t lastF = 0;
        for (uint32_t i = 0; i < n; ++i) { if (!(buf[i].frame >= lastF && buf[i].frame < block)) CHECK(false, "events sorted and inside block"); lastF = buf[i].frame;
            out.push_back({t + buf[i].frame, buf[i].data[0], buf[i].data[1], buf[i].data[2]}); }
    }
    return out;
}
static bool isOn(const Ev& e) { return (e.st & 0xF0) == 0x90 && e.vel > 0; }
static bool isOff(const Ev& e) { return (e.st & 0xF0) == 0x80; }
// every note-on is matched by a note-off later, no double note-on without off; returns stuck count
static int stuck(const std::vector<Ev>& ev) {
    std::map<int, int> on; int bad = 0;
    for (auto& e : ev) { const int k = (e.st & 15) * 128 + e.note; if (isOn(e)) { if (on[k]) ++bad; on[k] = 1; } else if (isOff(e)) on[k] = 0; }
    for (auto& p : on) bad += p.second;
    return bad;
}

static void testScales() {
    for (int i = 0; i < kNumScales; ++i) {
        const Scale& s = kScales[i];
        bool ok = s.iv[0] == 0 && s.n >= 5 && s.n <= 7;
        for (int k = 1; k < s.n; ++k) ok = ok && s.iv[k] > s.iv[k - 1] && s.iv[k] < 12;
        CHECK(ok, "scale %s well-formed", s.en);
        CHECK(degreePitch(s, 60, s.n) == 72 && degreePitch(s, 60, -s.n) == 48, "scale %s octave wrap", s.en);
    }
    const Scale& maj = kScales[0];
    CHECK(whiteKeyDegree(maj, 60) == 0 && whiteKeyDegree(maj, 62) == 1 && whiteKeyDegree(maj, 71) == 6, "white keys = degrees");
    CHECK(whiteKeyDegree(maj, 72) == 7 && whiteKeyDegree(maj, 59) == -1 && whiteKeyDegree(maj, 61) == 0, "white keys octave / black keys");
    CHECK(snapToDegree(maj, 0, 64) - 5 * 7 == 2 && snapToDegree(maj, 0, 63) - 5 * 7 == 1, "snap E4 -> III, Eb4 -> II");
    CHECK(snapToDegree(kScales[9], 2, 62) - 35 == 0 && snapToDegree(kScales[9], 2, 66) - 35 == 2, "D Hijaz: D -> I, F# -> III");
}

static void testChords() {
    const Scale& maj = kScales[0];
    struct T { int deg; ChordType t; std::set<int> pc; const char* name; } cases[] = {
        {0, ctTriad, S({0, 4, 7}), ""}, {1, ctTriad, S({2, 5, 9}), "m"}, {4, ctTriad, S({7, 11, 2}), ""}, {6, ctTriad, S({11, 2, 5}), "dim"},
        {4, ctSeventh, S({7, 11, 2, 5}), "7"}, {0, ctSeventh, S({0, 4, 7, 11}), "maj7"}, {1, ctSeventh, S({2, 5, 9, 0}), "m7"},
        {6, ctSeventh, S({11, 2, 5, 9}), "m7b5"}, {0, ctSus4, S({0, 5, 7}), "sus4"}, {0, ctSus2, S({0, 2, 7}), "sus2"},
        {0, ctPower, S({0, 7}), "5"}, {0, ctAdd9, S({0, 4, 7, 2}), "add9"}, {4, ctNinth, S({7, 11, 2, 5, 9}), "9"},
    };
    for (auto& c : cases) {
        Chord ch = diatonicChord(maj, 0, c.deg, c.t);
        CHECK(pcs(ch) == c.pc, "C major degree %d type %d notes", c.deg, int(c.t));
        CHECK(std::string(qualitySuffix(identifyQuality(ch))) == c.name, "name %s got %s", c.name, qualitySuffix(identifyQuality(ch)));
        CHECK(pcOf(ch.root) == pcOf(maj.iv[c.deg]), "root");
    }
    // A minor (tonic 9) natural: i = Am, III = C, v = Em ; harmonic minor V = E major
    CHECK(pcs(diatonicChord(kScales[1], 9, 0, ctTriad)) == S({9, 0, 4}), "Am i");
    CHECK(pcs(diatonicChord(kScales[2], 9, 4, ctTriad)) == S({4, 8, 11}), "A harmonic minor V = E");
    // D Hijaz: I = D major, II = Eb major, iv = Gm, vii = Cm
    const Scale& hij = kScales[9];
    CHECK(pcs(diatonicChord(hij, 2, 0, ctTriad)) == S({2, 6, 9}), "D Hijaz I = D");
    CHECK(pcs(diatonicChord(hij, 2, 1, ctTriad)) == S({3, 7, 10}), "D Hijaz II = Eb");
    CHECK(pcs(diatonicChord(hij, 2, 3, ctTriad)) == S({7, 10, 2}), "D Hijaz iv = Gm");
    CHECK(pcs(diatonicChord(hij, 2, 6, ctTriad)) == S({0, 3, 7}), "D Hijaz vii = Cm");
    // every scale/degree/type yields notes inside the scale (except power chord octave) and in MIDI range
    for (int s = 0; s < kNumScales; ++s) for (int t = 0; t < kNumChordTypes; ++t) for (int d = -7; d < 14; ++d) {
        Chord c = diatonicChord(kScales[s], 5, d, ChordType(t));
        bool in = c.n >= 3;
        for (int i = 0; i < c.n; ++i) { bool found = false; for (int k = 0; k < kScales[s].n; ++k) found |= pcOf(c.notes[i] - 5) == kScales[s].iv[k]; in = in && (found || t == ctPower) && c.notes[i] > 0 && c.notes[i] < 128; }
        if (!in) { CHECK(false, "diatonic chord out of scale s=%d t=%d d=%d", s, t, d); }
    }
    ++g_pass;
}

static void testPresets() {
    for (int p = 0; p < kNumPresets; ++p) {
        const Preset& pr = kPresets[p];
        CHECK(pr.n >= 3 && pr.n <= 7, "preset %s size", pr.en);
        for (int i = 0; i < pr.n; ++i) {
            Chord c = qualityChord(60 + 3 + pr.c[i].off, pr.c[i].q);
            CHECK(identifyQuality(c) == int(pr.c[i].q), "preset %s chord %d identifies", pr.en, i);
        }
    }
    // Pop in G: G D Em C via generator (progression mode, keys C D E F)
    Generator g; g.setSampleRate(48000); g.settings().mode = mProgression; g.settings().tonic = 7; g.settings().preset = 0;
    const int keys[4] = {60, 62, 64, 65}; const int roots[4] = {7, 2, 4, 0}; const int qs[4] = {qMaj, qMaj, qMin, qMaj};
    for (int i = 0; i < 4; ++i) {
        run(g, {{0, keys[i], true}}, 512);
        CHECK(pcOf(g.currentRoot()) == roots[i] && g.currentQuality() == qs[i], "Pop in G chord %d root %d q %d", i, g.currentRoot(), g.currentQuality());
        run(g, {{0, keys[i], false}}, 512);
    }
    CHECK(g.currentRoot() == -1, "released -> no chord");
}

static void testVoicing() {
    Chord c = diatonicChord(kScales[0], 0, 0, ctTriad);     // C E G at 60
    Chord v0 = closeVoicing(c, 0), v1 = closeVoicing(c, 1), v2 = closeVoicing(c, 2);
    CHECK(v0.notes[0] == 60 && v0.notes[2] == 67, "root position");
    CHECK(v1.notes[0] == 64 && v1.notes[2] == 72, "1st inversion E G C");
    CHECK(v2.notes[0] == 67 && v2.notes[1] == 72 && v2.notes[2] == 76, "2nd inversion G C E");
    Chord c7 = closeVoicing(diatonicChord(kScales[0], 0, 0, ctSeventh), 0);   // C E G B
    Chord d2 = applyVoicing(c7, vDrop2);
    CHECK(d2.notes[0] == 55 && d2.notes[1] == 60 && d2.notes[3] == 71, "drop 2: G2 C E B");
    Chord op = applyVoicing(v0, vOpen);
    CHECK(op.notes[0] == 48 && op.notes[1] == 64, "open: low root");
    Chord sp = applyVoicing(c7, vSpread);
    CHECK(sp.notes[sp.n - 1] - sp.notes[0] > 12, "spread wider than an octave");
    // voice leading: I-V-vi-IV in C, total movement much smaller than root-position jumps, register stable
    const int degs[8] = {0, 4, 5, 3, 0, 4, 5, 3};
    Chord prev; bool has = false; int moveVL = 0, moveRP = 0; Chord prevRP; bool hasRP = false;
    for (int d : degs) {
        Chord raw = diatonicChord(kScales[0], 0, d, ctTriad);
        Chord vl = voiceChord(raw, vClose, 0, 60, true, has ? &prev : nullptr);
        Chord rp = voiceChord(raw, vClose, 0, 60, false, nullptr);
        if (has) moveVL += voiceMovement(prev, vl);
        if (hasRP) moveRP += voiceMovement(prevRP, rp);
        CHECK(std::abs(centroid(vl) - 60) <= 7, "voice-led chord stays near centre (%d)", centroid(vl));
        CHECK(pcs(vl) == pcs(raw), "voice leading keeps the chord tones");
        prev = vl; has = true; prevRP = rp; hasRP = true;
    }
    CHECK(moveVL * 2 <= moveRP, "voice leading moves less (%d vs %d)", moveVL, moveRP);
    CHECK(moveVL <= 7 * 8, "average movement <= 4 semitones per voice-pair (%d)", moveVL);
}

static void testGeneratorBasics() {
    Generator g; g.setSampleRate(48000);
    auto ev = run(g, {{1000, 60, true}, {20000, 60, false}}, 30000);
    int ons = 0, offs = 0; std::set<int> notes;
    for (auto& e : ev) { if (isOn(e)) { ++ons; notes.insert(e.note); CHECK(e.t == 1000 && (e.st & 15) == 0, "chord note-on at the key time on ch1"); } if (isOff(e)) { ++offs; CHECK(e.t == 20000, "note-off at release"); } }
    CHECK(ons == 3 && offs == 3, "one key -> 3 notes on / off (%d/%d)", ons, offs);
    CHECK(S({60, 64, 67}) == [&] { std::set<int> s; for (int n : notes) s.insert(pcOf(n)); return s; }(), "C major chord");
    // strum 30 ms down: low -> high, 15 ms apart (720 samples)
    Generator s; s.setSampleRate(48000); s.settings().strumMs = 30; s.settings().voiceLead = false;
    ev = run(s, {{0, 60, true}, {24000, 60, false}}, 30000);
    std::vector<Ev> on; for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() == 3 && on[1].t - on[0].t == 720 && on[2].t - on[1].t == 720 && on[0].note < on[1].note && on[1].note < on[2].note, "strum down spacing");
    s.settings().strumDir = sdUp;
    ev = run(s, {{0, 62, true}, {24000, 62, false}}, 30000); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() == 3 && on[0].note > on[2].note, "strum up: high first");
    // releasing during the strum cancels the notes not yet played
    ev = run(s, {{0, 64, true}, {500, 64, false}}, 30000); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() == 1 && stuck(ev) == 0, "release mid-strum: no late notes, no stuck notes");
    // legato change: new key while holding -> old chord off, new chord on, same sample
    Generator l; l.setSampleRate(48000);
    ev = run(l, {{0, 60, true}, {9600, 65, true}, {9700, 60, false}, {20000, 65, false}}, 30000);
    CHECK(stuck(ev) == 0, "legato chord change clean");
    int onsAt = 0; for (auto& e : ev) if (isOn(e) && e.t == 9600) ++onsAt;
    CHECK(onsAt >= 2, "second chord at its key time");
    CHECK(pcOf(l.currentRoot()) == -1 || l.currentRoot() == -1, "all released");
    // latch: chord continues after release, changes on next key
    Generator la; la.setSampleRate(48000); la.settings().latch = true;
    ev = run(la, {{0, 60, true}, {100, 60, false}}, 20000);
    CHECK(la.chordActive() && stuck(ev) == 3, "latch holds the chord");
    // thru mode
    Generator th; th.setSampleRate(48000); th.settings().mode = mThru;
    ev = run(th, {{10, 61, true}, {400, 61, false}}, 2000);
    CHECK(ev.size() == 2 && ev[0].note == 61 && isOn(ev[0]) && isOff(ev[1]), "thru passes notes");
    // fixed velocity + channels
    Generator fv; fv.setSampleRate(48000); fv.settings().velocity = 90; fv.settings().chChord = 4;
    ev = run(fv, {{0, 60, true}, {100, 60, false}}, 2000);
    bool okv = true; for (auto& e : ev) if (isOn(e)) okv = okv && e.vel == 90 && (e.st & 15) == 4;
    CHECK(okv, "fixed velocity on chosen channel");
}

static void testArp() {
    // 120 bpm, 1/16 = 0.25 beat = 6000 samples at 48 kHz; host playing; key at beat 0
    Generator g; g.setSampleRate(48000); auto& s = g.settings();
    s.arpOn = true; s.arpRate = 5; s.arpPattern = apUp; s.gate = 0.5f; s.voiceLead = false;
    auto ev = run(g, {{0, 60, true}, {48000, 60, false}}, 60000);
    std::vector<Ev> on, off; for (auto& e : ev) { if (isOn(e)) on.push_back(e); if (isOff(e)) off.push_back(e); }
    CHECK(on.size() == 8, "8 sixteenths in one second at 120 bpm (%zu)", on.size());
    bool grid = true; for (size_t i = 0; i < on.size(); ++i) grid = grid && on[i].t == int64_t(i) * 6000;
    CHECK(grid, "arp notes exactly on the 1/16 grid");
    CHECK(on.size() >= 4 && on[0].note < on[1].note && on[1].note < on[2].note && on[3].note == on[0].note, "up pattern cycles");
    CHECK(off.size() >= 1 && off[0].t == 3000, "gate 50%% -> note-off after 3000 samples (%lld)", (long long)(off.empty() ? -1 : off[0].t));
    CHECK(stuck(ev) == 0, "arp: no stuck notes");
    // down / updown / octaves
    Generator d; d.setSampleRate(48000);
    Chord c = closeVoicing(diatonicChord(kScales[0], 0, 0, ctTriad), 0);
    CHECK(d.arpNote(c, apDown, 1, 0) == 67 && d.arpNote(c, apDown, 1, 1) == 64, "down");
    int ud[6]; for (int i = 0; i < 6; ++i) ud[i] = d.arpNote(c, apUpDown, 1, i);
    CHECK(ud[0] == 60 && ud[1] == 64 && ud[2] == 67 && ud[3] == 64 && ud[4] == 60 && ud[5] == 64, "up-down without repeated ends");
    CHECK(d.arpNote(c, apUp, 2, 3) == 72 && d.arpNote(c, apUp, 2, 5) == 79, "2 octaves");
    CHECK(d.arpNote(c, apThumb, 1, 0) == 60 && d.arpNote(c, apThumb, 1, 1) == 64 && d.arpNote(c, apThumb, 1, 2) == 60 && d.arpNote(c, apThumb, 1, 3) == 67, "thumb");
    CHECK(d.arpNote(c, apBroken, 2, 0) == 60 && d.arpNote(c, apBroken, 2, 1) == 67 && d.arpNote(c, apBroken, 2, 2) == 64, "broken");
    bool inChord = true; for (int i = 0; i < 50; ++i) { int n = d.arpNote(c, apRandom, 2, i); inChord = inChord && (pcOf(n) == 0 || pcOf(n) == 4 || pcOf(n) == 7); }
    CHECK(inChord, "random stays in the chord");
    // swing 100%: odd 1/8 steps delayed by half a step (0.25 beat = 6000 samples)
    Generator w; w.setSampleRate(48000); w.settings().arpOn = true; w.settings().arpRate = 3; w.settings().swing = 1.0f;
    ev = run(w, {{0, 60, true}, {48000, 60, false}}, 50000); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() >= 3 && on[0].t == 0 && on[1].t == 12000 + 6000 && on[2].t == 24000, "swing delays off-beats");
    // tempo change follows host bpm: 90 bpm 1/8 = 0.5 beat = 16000 samples
    Generator t9; t9.setSampleRate(48000); t9.settings().arpOn = true; t9.settings().arpRate = 3;
    ev = run(t9, {{0, 60, true}, {48000, 60, false}}, 50000, true, 90); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() == 3 && on[1].t == 16000, "follows host tempo (90 bpm)");
    // host playing, key pressed mid-step: waits for the next grid step
    Generator m; m.setSampleRate(48000); m.settings().arpOn = true; m.settings().arpRate = 2;  // 1/4 = 24000
    ev = run(m, {{10000, 60, true}, {60000, 60, false}}, 70000); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(!on.empty() && on[0].t == 24000, "quantised start on host grid (%lld)", (long long)(on.empty() ? -1 : on[0].t));
    // transport stopped: starts immediately at the key, then free-runs at host tempo
    Generator f; f.setSampleRate(48000); f.settings().arpOn = true; f.settings().arpRate = 3;
    ev = run(f, {{10000, 60, true}, {58000, 60, false}}, 70000, false); on.clear(); for (auto& e : ev) if (isOn(e)) on.push_back(e);
    CHECK(on.size() >= 2 && on[0].t == 10000 && on[1].t == 22000, "stopped transport: immediate + free-running");
    // host loops back (beat jumps from 4 to 0): arp keeps playing after the jump
    {
        Generator lp; lp.setSampleRate(48000); lp.settings().arpOn = true; lp.settings().arpRate = 2; lp.settings().latch = true;
        MidiOut buf[512]; int after = 0;
        lp.noteOn(60, 100, 0);
        for (int b = 0; b < 2 * 96000 / 480; ++b) {
            const int64_t t = int64_t(b) * 480; Transport tr; tr.playing = true; tr.beat = double(t % 96000) / 24000.0;   // 1-bar loop
            uint32_t n = 0; lp.process(480, tr, buf, n, 512);
            for (uint32_t i = 0; i < n; ++i) if ((buf[i].data[0] & 0xF0) == 0x90 && t >= 96000) ++after;
        }
        CHECK(after == 4, "arp continues after a host loop jump (%d)", after);
    }
    // chord pattern = rhythmic stabs with whole chord
    Generator st; st.setSampleRate(48000); st.settings().arpOn = true; st.settings().arpPattern = apChord; st.settings().arpRate = 2;
    ev = run(st, {{0, 60, true}, {48000, 60, false}}, 50000); int stabs = 0; for (auto& e : ev) if (isOn(e)) ++stabs;
    CHECK(stabs == 6, "chord stabs: 2 quarter notes x 3 notes (%d)", stabs);
}

static void testBass() {
    struct B { int mode; int perBar; } cases[] = {{bmHeld, 1}, {bmBeats, 4}, {bmRootFifth, 4}, {bmOctaves, 8}, {bmSyncopated, 6}};
    for (auto& c : cases) {
        Generator g; g.setSampleRate(48000); g.settings().bassMode = c.mode; g.settings().tonic = 2;
        auto ev = run(g, {{0, 60, true}, {96000, 60, false}}, 100000);   // 1 bar at 120 bpm
        int bass = 0; bool low = true, rootish = true;
        for (auto& e : ev) if (isOn(e) && (e.st & 15) == 1) { ++bass; low = low && e.note < 52; rootish = rootish && (pcOf(e.note) == 2 || pcOf(e.note) == 9); }
        CHECK(bass == c.perBar, "bass mode %d: %d notes per bar (got %d)", c.mode, c.perBar, bass);
        CHECK(low && rootish, "bass mode %d low register on root/fifth of D", c.mode);
        CHECK(stuck(ev) == 0, "bass mode %d: no stuck notes", c.mode);
    }
}

static void testHumanizeAndFuzz() {
    Generator h; h.setSampleRate(48000); h.settings().humTimeMs = 20; h.settings().humVel = 1;
    auto ev = run(h, {{0, 60, true}, {40000, 60, false}}, 50000);
    bool okT = true, okV = true, differ = false; int firstVel = -1;
    for (auto& e : ev) if (isOn(e)) { okT = okT && e.t >= 0 && e.t <= 960; okV = okV && e.vel >= 1 && e.vel <= 127; if (firstVel < 0) firstVel = e.vel; else differ |= e.vel != firstVel; }
    CHECK(okT, "humanize timing within 0..20 ms");
    CHECK(okV && differ, "humanize velocity varies, stays 1..127");
    // fuzz: random settings + random key sequences must never leave stuck notes after release
    uint32_t r = 12345; auto rnd = [&](int n) { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return int(r % uint32_t(n)); };
    int bad = 0, total = 0;
    for (int it = 0; it < 400; ++it) {
        Generator g; g.setSampleRate(48000); g.seed(uint32_t(it + 1)); Settings& s = g.settings();
        s.mode = rnd(2); s.keyMap = rnd(2); s.tonic = rnd(12); s.scale = rnd(kNumScales); s.chordType = rnd(kNumChordTypes); s.preset = rnd(kNumPresets);
        s.voicing = rnd(kNumVoicings); s.inversion = rnd(4); s.octave = rnd(5) - 2; s.voiceLead = rnd(2); s.strumMs = float(rnd(200));
        s.strumDir = rnd(3); s.arpOn = rnd(2); s.arpPattern = rnd(kNumArpPatterns); s.arpRate = rnd(kNumRates); s.arpOctaves = 1 + rnd(4);
        s.swing = rnd(100) / 100.f; s.gate = (5 + rnd(96)) / 100.f; s.bassMode = rnd(kNumBassModes); s.humVel = rnd(100) / 100.f; s.humTimeMs = float(rnd(40));
        s.chBass = rnd(16);
        std::vector<Drive> in; int64_t t = 0; std::vector<int> down;
        for (int k = 0; k < 30; ++k) {
            t += rnd(20000);
            if (!down.empty() && rnd(2)) { int i = rnd(int(down.size())); in.push_back({t, down[i], false}); down.erase(down.begin() + i); }
            else { int key = 36 + rnd(60); in.push_back({t, key, true}); down.push_back(key); }
        }
        for (int key : down) { t += 100; in.push_back({t, key, false}); }
        const bool play = rnd(2);
        ev = run(g, in, t + 96000 * 2, play, 60 + rnd(120), 64 + rnd(1000));
        bool rangeOk = true; for (auto& e : ev) rangeOk = rangeOk && e.note < 128 && e.vel < 128;
        bad += stuck(ev) + !rangeOk; total += int(ev.size());
    }
    CHECK(bad == 0, "fuzz: 400 random sessions, %d events, %d stuck / invalid", total, bad);
    // panic sends offs for everything
    Generator p; p.setSampleRate(48000); p.settings().latch = true;
    ev = run(p, {{0, 60, true}}, 1000);
    MidiOut buf[256]; uint32_t n = 0; p.panic(buf, n, 256);
    int offs = 0; for (uint32_t i = 0; i < n; ++i) offs += (buf[i].data[0] & 0xF0) == 0x80;
    CHECK(offs == 3 && !p.chordActive(), "panic releases all notes");
}

static void testMidiFile() {
    Settings s; s.preset = 0; s.tonic = 0; s.bassMode = bmBeats;
    auto ev = renderProgression(s, 1, 480);
    int chordOn = 0, bassOn = 0; int64_t lastTick = 0;
    std::vector<int> rootsSeen;
    for (auto& e : ev) { if ((e.d[0] & 0xF0) == 0x90 && e.d[2]) { if ((e.d[0] & 15) == 0) ++chordOn; else ++bassOn; } lastTick = std::max(lastTick, e.tick); }
    CHECK(chordOn == 12, "Pop progression: 4 chords x 3 notes (%d)", chordOn);
    CHECK(bassOn == 16, "bass on every beat for 4 bars (%d)", bassOn);
    CHECK(lastTick <= 4 * 4 * 480 + 480, "fits 4 bars");
    std::vector<Ev> asEv; for (auto& e : ev) asEv.push_back({e.tick, e.d[0], e.d[1], e.d[2]});
    CHECK(stuck(asEv) == 0, "exported file has no stuck notes");
    auto smf = buildSmf(ev, 480, 120, "test");
    CHECK(smf.size() > 60 && std::string(smf.begin(), smf.begin() + 4) == "MThd", "SMF header");
    // parse back: count tracks and note-ons
    size_t pos = 14; int tracks = 0, ons = 0;
    while (pos + 8 <= smf.size()) {
        CHECK(std::string(smf.begin() + pos, smf.begin() + pos + 4) == "MTrk", "track chunk");
        const uint32_t len = (smf[pos + 4] << 24) | (smf[pos + 5] << 16) | (smf[pos + 6] << 8) | smf[pos + 7];
        size_t p = pos + 8, end = p + len; ++tracks;
        while (p < end) {
            while (smf[p] & 0x80) ++p;
            ++p;                                        // delta
            const uint8_t st = smf[p];
            if (st == 0xFF) { const uint8_t l = smf[p + 2]; p += 3 + l; }
            else { if ((st & 0xF0) == 0x90 && smf[p + 2]) ++ons; p += 3; }
        }
        CHECK(p == end, "track parsed to its end");
        pos = end;
    }
    CHECK(tracks == 3 && ons == 28, "SMF: tempo + chords + bass tracks, 28 notes (%d tracks, %d notes)", tracks, ons);
    // arp + strum + humanize export still clean
    s.arpOn = true; s.arpRate = 5; s.strumMs = 40; s.humTimeMs = 10; s.preset = 8; s.bassMode = bmSyncopated;
    ev = renderProgression(s, 2, 960, 2); asEv.clear(); for (auto& e : ev) asEv.push_back({e.tick, e.d[0], e.d[1], e.d[2]});
    CHECK(!ev.empty() && stuck(asEv) == 0, "complex export clean (%zu events)", ev.size());
}

static void testSynth() {
    PreviewSynth s; s.setSampleRate(48000);
    std::vector<float> buf(48000, 0.f);
    const uint8_t on[3] = {0x90, 60, 100}, on2[3] = {0x90, 64, 100}, off[3] = {0x80, 60, 0}, off2[3] = {0x80, 64, 0};
    s.midi(on); s.midi(on2); s.render(buf.data(), 24000);
    float peak = 0; bool fin = true; for (int i = 0; i < 24000; ++i) { peak = std::max(peak, std::fabs(buf[i])); fin = fin && std::isfinite(buf[i]); }
    CHECK(fin && peak > 0.05f && peak < 1.0f, "synth audible and not clipping (peak %.3f)", peak);
    s.midi(off); s.midi(off2); std::fill(buf.begin(), buf.end(), 0.f); s.render(buf.data(), 48000);
    float tail = 0; for (int i = 40000; i < 48000; ++i) tail = std::max(tail, std::fabs(buf[i]));
    CHECK(tail < 1e-3f && s.activeVoices() == 0, "synth silent after release");
    for (int n = 0; n < 40; ++n) { const uint8_t m[3] = {0x90, uint8_t(30 + n), 100}; s.midi(m); }
    std::fill(buf.begin(), buf.end(), 0.f); s.render(buf.data(), 4800);
    peak = 0; for (int i = 0; i < 4800; ++i) peak = std::max(peak, std::fabs(buf[i]));
    CHECK(s.activeVoices() == 16 && std::isfinite(peak), "voice stealing keeps 16 voices");
}

int main() {
    testScales(); testChords(); testPresets(); testVoicing(); testGeneratorBasics(); testArp(); testBass(); testHumanizeAndFuzz(); testMidiFile(); testSynth();
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
