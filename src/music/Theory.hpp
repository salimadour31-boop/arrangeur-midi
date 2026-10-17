// Theory.hpp — scales (incl. modes and Arabic / North-African maqam approximations), diatonic chord
// building, chord naming, progression presets per style, voicings and voice leading.
// Header-only, no allocation in the real-time paths (fixed-size arrays). MIT licence.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace am {

constexpr int kMaxScaleNotes = 7;
constexpr int kMaxChordNotes = 6;

struct Scale { const char* fr; const char* en; const char* kab; int n; int iv[kMaxScaleNotes]; };

// Maqam entries are 12-tone approximations: quarter tones are rounded (documented in the README).
static const Scale kScales[] = {
    {"Majeur (ionien)",        "Major (Ionian)",        "Ameqqran",            7, {0, 2, 4, 5, 7, 9, 11}},
    {"Mineur naturel (éolien)", "Natural minor (Aeolian)", "Amecṭuḥ",          7, {0, 2, 3, 5, 7, 8, 10}},
    {"Mineur harmonique",      "Harmonic minor",        "Amecṭuḥ armunik",     7, {0, 2, 3, 5, 7, 8, 11}},
    {"Mineur mélodique",       "Melodic minor",         "Amecṭuḥ amelḍi",      7, {0, 2, 3, 5, 7, 9, 11}},
    {"Dorien",                 "Dorian",                "Dorien",              7, {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygien",               "Phrygian",              "Phrygien",            7, {0, 1, 3, 5, 7, 8, 10}},
    {"Lydien",                 "Lydian",                "Lydien",              7, {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydien",             "Mixolydian",            "Mixolydien",          7, {0, 2, 4, 5, 7, 9, 10}},
    {"Locrien",                "Locrian",               "Locrien",             7, {0, 1, 3, 5, 6, 8, 10}},
    {"Hijaz",                  "Hijaz",                 "Ḥijaz",               7, {0, 1, 4, 5, 7, 8, 10}},
    {"Hijaz Kar",              "Hijaz Kar",             "Ḥijaz Kar",           7, {0, 1, 4, 5, 7, 8, 11}},
    {"Nahawand",               "Nahawand",              "Nahawand",            7, {0, 2, 3, 5, 7, 8, 11}},
    {"Kurd",                   "Kurd",                  "Kurd",                7, {0, 1, 3, 5, 7, 8, 10}},
    {"Bayati (≈)",             "Bayati (≈)",            "Bayati (≈)",          7, {0, 1, 3, 5, 7, 8, 10}},
    {"Rast (≈)",               "Rast (≈)",              "Rast (≈)",            7, {0, 2, 4, 5, 7, 9, 10}},
    {"Saba (≈)",               "Saba (≈)",              "Saba (≈)",            7, {0, 1, 3, 4, 7, 8, 10}},
    {"Pentatonique majeure",   "Major pentatonic",      "Penta tameqqrant",    5, {0, 2, 4, 7, 9}},
    {"Pentatonique mineure",   "Minor pentatonic",      "Penta tamecṭuḥt",     5, {0, 3, 5, 7, 10}},
    {"Blues",                  "Blues",                 "Blues",               6, {0, 3, 5, 6, 7, 10}},
};
constexpr int kNumScales = int(sizeof(kScales) / sizeof(kScales[0]));

inline int pcOf(int note) { return ((note % 12) + 12) % 12; }
inline int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// Absolute pitch of scale degree `deg` (may be negative or > n: wraps octaves) above tonic note `tonicNote`.
inline int degreePitch(const Scale& s, int tonicNote, int deg) {
    const int o = floorDiv(deg, s.n);
    return tonicNote + 12 * o + s.iv[deg - o * s.n];
}

// Snap a played note to a scale degree (largest degree whose pitch is <= the note). Returns the degree index
// counted from the tonic in octave of `tonicNote` (can be negative / > n).
inline int snapToDegree(const Scale& s, int tonicPc, int note) {
    const int rel = note - tonicPc;                 // distance from a C-octave tonic reference
    const int o = floorDiv(rel, 12), pc = rel - 12 * o;
    int d = 0;
    for (int i = 0; i < s.n; ++i) if (s.iv[i] <= pc) d = i;
    return o * s.n + d;
}

// Map a key to a degree with "white keys = degrees": C=I, D=II ... B=VII, next octave continues.
// Black keys take the white key below. Returns degree relative to octave 4 (C4 = degree 0).
inline int whiteKeyDegree(const Scale& s, int note) {
    static const int idx[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    const int o = floorDiv(note, 12) - 5;           // C4 (60) -> octave 0
    return o * s.n + idx[pcOf(note)];
}

enum ChordType { ctTriad = 0, ctSeventh, ctNinth, ctSus2, ctSus4, ctPower, ctAdd9, kNumChordTypes };
static const char* const kChordTypeFr[] = {"Triade", "Septième", "Neuvième", "Sus2", "Sus4", "Quinte (power)", "Add9"};
static const char* const kChordTypeEn[] = {"Triad", "Seventh", "Ninth", "Sus2", "Sus4", "Power (5th)", "Add9"};
static const char* const kChordTypeKab[] = {"Tlata", "Tis sebɛa", "Tis tesɛa", "Sus2", "Sus4", "Tis xemsa (power)", "Add9"};

// A chord as absolute MIDI notes (root first, then upward stack, before voicing).
struct Chord {
    int n = 0;
    int notes[kMaxChordNotes] = {};
    int root = 0;       // MIDI note of the root
    void add(int v) { if (n < kMaxChordNotes) notes[n++] = v; }
    void sort() { std::sort(notes, notes + n); }
};

// Diatonic chord on degree `deg` of scale s, tonic pitch class tonicPc; root lands in octave of `baseNote`'s C.
inline Chord diatonicChord(const Scale& s, int tonicPc, int deg, ChordType type, int octaveC = 60) {
    Chord c;
    const int tonic = octaveC + tonicPc;
    auto p = [&](int d) { return degreePitch(s, tonic, deg + d); };
    c.root = p(0);
    switch (type) {
        case ctTriad:   c.add(p(0)); c.add(p(2)); c.add(p(4)); break;
        case ctSeventh: c.add(p(0)); c.add(p(2)); c.add(p(4)); c.add(p(6)); break;
        case ctNinth:   c.add(p(0)); c.add(p(2)); c.add(p(4)); c.add(p(6)); c.add(p(8)); break;
        case ctSus2:    c.add(p(0)); c.add(p(1)); c.add(p(4)); break;
        case ctSus4:    c.add(p(0)); c.add(p(3)); c.add(p(4)); break;
        case ctPower:   c.add(p(0)); c.add(p(0) + 7); c.add(p(0) + 12); break;
        case ctAdd9:    c.add(p(0)); c.add(p(2)); c.add(p(4)); c.add(p(8)); break;
        default: break;
    }
    return c;
}

// Chord qualities used by progression presets (intervals from the root).
enum Quality { qMaj, qMin, qDim, qAug, qDom7, qMaj7, qMin7, qHalfDim, qSus4, qSus2, qMin9, qMaj9, kNumQualities };
struct QualityDef { const char* suffix; int n; int iv[5]; };
static const QualityDef kQualities[kNumQualities] = {
    {"", 3, {0, 4, 7}}, {"m", 3, {0, 3, 7}}, {"dim", 3, {0, 3, 6}}, {"aug", 3, {0, 4, 8}},
    {"7", 4, {0, 4, 7, 10}}, {"maj7", 4, {0, 4, 7, 11}}, {"m7", 4, {0, 3, 7, 10}}, {"m7b5", 4, {0, 3, 6, 10}},
    {"sus4", 3, {0, 5, 7}}, {"sus2", 3, {0, 2, 7}}, {"m9", 5, {0, 3, 7, 10, 14}}, {"maj9", 5, {0, 4, 7, 11, 14}},
};

inline Chord qualityChord(int rootNote, Quality q) {
    Chord c; c.root = rootNote;
    for (int i = 0; i < kQualities[q].n; ++i) c.add(rootNote + kQualities[q].iv[i]);
    return c;
}

// Progression presets per style: chords relative to the tonic (semitones + quality). Up to 7 chords,
// triggered by the white keys C, D, E, F, G, A, B in "Progression" mode.
struct PresetChord { int off; Quality q; };
struct Preset { const char* fr; const char* en; const char* kab; const char* roman; int n; PresetChord c[7]; };
static const Preset kPresets[] = {
    {"Pop", "Pop", "Pop", "I–V–vi–IV", 4, {{0, qMaj}, {7, qMaj}, {9, qMin}, {5, qMaj}}},
    {"Ballade", "Ballad", "Ballade", "vi–IV–I–V", 4, {{9, qMin}, {5, qMaj}, {0, qMaj}, {7, qMaj}}},
    {"Années 50", "50s doo-wop", "Iseggasen 50", "I–vi–IV–V", 4, {{0, qMaj}, {9, qMin}, {5, qMaj}, {7, qMaj}}},
    {"Jazz ii–V–I", "Jazz ii–V–I", "Jazz ii–V–I", "ii7–V7–Imaj7–vi7", 4, {{2, qMin7}, {7, qDom7}, {0, qMaj7}, {9, qMin7}}},
    {"Néo-soul", "Neo-soul", "Néo-soul", "IVmaj7–iii7–vi9–Imaj9", 4, {{5, qMaj7}, {4, qMin7}, {9, qMin9}, {0, qMaj9}}},
    {"Blues", "Blues", "Blues", "I7–IV7–I7–V7–IV7–I7", 6, {{0, qDom7}, {5, qDom7}, {0, qDom7}, {7, qDom7}, {5, qDom7}, {0, qDom7}}},
    {"Mineur épique", "Epic minor", "Amecṭuḥ ameqqran", "i–VI–III–VII", 4, {{0, qMin}, {8, qMaj}, {3, qMaj}, {10, qMaj}}},
    {"Cadence andalouse", "Andalusian cadence", "Andalus", "i–VII–VI–V", 4, {{0, qMin}, {10, qMaj}, {8, qMaj}, {7, qMaj}}},
    {"Hijaz / chaâbi", "Hijaz / chaabi", "Ḥijaz / caɛbi", "I–II–iv–I–vii", 5, {{0, qMaj}, {1, qMaj}, {5, qMin}, {0, qMaj}, {10, qMin}}},
    {"Raï / Maghreb", "Rai / Maghreb", "Raï / Tamazɣa", "i–VII–VI–VII", 4, {{0, qMin}, {10, qMaj}, {8, qMaj}, {10, qMaj}}},
    {"Kurd / mélancolie", "Kurd / melancholic", "Kurd / ḥzen", "i–II–VII–i–v", 5, {{0, qMin}, {1, qMaj}, {10, qMin}, {0, qMin}, {7, qMin}}},
    {"Trap sombre", "Dark trap", "Trap aberkan", "i–VI–iv–V", 4, {{0, qMin}, {8, qMaj}, {5, qMin}, {7, qMaj}}},
};
constexpr int kNumPresets = int(sizeof(kPresets) / sizeof(kPresets[0]));

// Voicings
enum Voicing { vClose = 0, vOpen, vDrop2, vSpread, kNumVoicings };
static const char* const kVoicingFr[] = {"Serré", "Ouvert (basse grave)", "Drop 2", "Large"};
static const char* const kVoicingEn[] = {"Close", "Open (low root)", "Drop 2", "Wide"};
static const char* const kVoicingKab[] = {"Yeqqen", "Yeldi", "Drop 2", "Yehrawen"};

// Close position of chord pitch classes starting at `bottom` with inversion `inv` (0 = root position).
inline Chord closeVoicing(const Chord& c, int inv) {
    Chord r; r.root = c.root;
    if (c.n == 0) return r;
    int tones[kMaxChordNotes]; for (int i = 0; i < c.n; ++i) tones[i] = c.notes[i];
    std::sort(tones, tones + c.n);
    inv = ((inv % c.n) + c.n) % c.n;
    for (int i = 0; i < inv; ++i) {               // move the lowest note up an octave
        int v = tones[0] + 12;
        for (int k = 0; k < c.n - 1; ++k) tones[k] = tones[k + 1];
        tones[c.n - 1] = v;
        std::sort(tones, tones + c.n);
    }
    for (int i = 0; i < c.n; ++i) r.add(tones[i]);
    return r;
}

inline Chord applyVoicing(const Chord& closeV, Voicing v) {
    Chord r = closeV; r.sort();
    if (r.n < 2) return r;
    switch (v) {
        case vOpen:   r.notes[0] -= 12; break;                                   // lowest tone an octave down
        case vDrop2:  if (r.n >= 3) r.notes[r.n - 2] -= 12; break;              // 2nd from top down an octave
        case vSpread: for (int i = 1; i < r.n; i += 2) r.notes[i] += 12; break; // every other tone up
        default: break;
    }
    r.sort();
    return r;
}

inline int centroid(const Chord& c) { int s = 0; for (int i = 0; i < c.n; ++i) s += c.notes[i]; return c.n ? s / c.n : 60; }

// Movement cost between two voiced chords: each new note to its nearest previous note (sum of semitones),
// plus the reverse, so that both added and dropped voices count.
inline int voiceMovement(const Chord& a, const Chord& b) {
    if (a.n == 0 || b.n == 0) return 0;
    int cost = 0;
    for (int i = 0; i < b.n; ++i) { int m = 999; for (int j = 0; j < a.n; ++j) m = std::min(m, std::abs(b.notes[i] - a.notes[j])); cost += m; }
    for (int j = 0; j < a.n; ++j) { int m = 999; for (int i = 0; i < b.n; ++i) m = std::min(m, std::abs(b.notes[i] - a.notes[j])); cost += m; }
    return cost;
}

// Build the final voiced chord. Without voice leading: inversion `inv`, centred near `center`.
// With voice leading and a previous chord: try every inversion and octave around `center` (±7 semitones
// tolerance) and keep the one moving the least.
inline Chord voiceChord(const Chord& raw, Voicing v, int inv, int center, bool voiceLead, const Chord* prev) {
    if (raw.n == 0) return raw;
    auto place = [&](int i, int octShift) {
        Chord base = raw;
        for (int k = 0; k < base.n; ++k) base.notes[k] += octShift * 12;
        base.root += octShift * 12;
        return applyVoicing(closeVoicing(base, i), v);
    };
    // octave so that the chosen inversion is nearest to the centre
    auto bestOctave = [&](int i) { int best = 0, bd = 1 << 30; for (int o = -4; o <= 4; ++o) { int d = std::abs(centroid(place(i, o)) - center); if (d < bd) { bd = d; best = o; } } return best; };
    if (!voiceLead || prev == nullptr || prev->n == 0) return place(inv, bestOctave(inv));
    Chord best; int bestCost = 1 << 30;
    for (int i = 0; i < raw.n; ++i)
        for (int o = -4; o <= 4; ++o) {
            Chord c = place(i, o);
            const int drift = std::abs(centroid(c) - center);
            if (drift > 7) continue;                                   // keep the register stable
            const int cost = voiceMovement(*prev, c) * 4 + drift;
            if (cost < bestCost) { bestCost = cost; best = c; }
        }
    if (best.n == 0) return place(inv, bestOctave(inv));
    return best;
}

// Chord naming from root + pitch-class set (best-effort: matches the quality table, else "root(...)").
inline const char* noteNameFr(int pc) { static const char* n[12] = {"Do", "Do#", "Ré", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si"}; return n[pcOf(pc)]; }
inline const char* noteNameEn(int pc) { static const char* n[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"}; return n[pcOf(pc)]; }

// Identify the quality of a chord (by pitch-class set relative to its root). Returns -1 if unknown,
// or 100 + n for power chords.
inline int identifyQuality(const Chord& c) {
    int mask = 0;
    for (int i = 0; i < c.n; ++i) mask |= 1 << pcOf(c.notes[i] - c.root);
    if (mask == ((1 << 0) | (1 << 7))) return 100;
    for (int q = 0; q < kNumQualities; ++q) {
        int m = 0; for (int i = 0; i < kQualities[q].n; ++i) m |= 1 << (kQualities[q].iv[i] % 12);
        if (m == mask) return q;
    }
    // add9 (major / minor)
    if (mask == ((1 << 0) | (1 << 4) | (1 << 7) | (1 << 2))) return 200;
    if (mask == ((1 << 0) | (1 << 3) | (1 << 7) | (1 << 2))) return 201;
    if (mask == ((1 << 0) | (1 << 3) | (1 << 6) | (1 << 9))) return 202;   // dim7
    if (mask == ((1 << 0) | (1 << 4) | (1 << 7) | (1 << 10) | (1 << 2))) return 203; // 9
    if (mask == ((1 << 0) | (1 << 3) | (1 << 6) | (1 << 10) | (1 << 1))) return 204; // m7b5(b9) rare
    if (mask == ((1 << 0) | (1 << 3) | (1 << 7) | (1 << 11))) return 205;  // m(maj7)
    if (mask == ((1 << 0) | (1 << 4) | (1 << 8) | (1 << 11))) return 206;  // maj7#5
    return -1;
}
inline const char* qualitySuffix(int q) {
    if (q >= 0 && q < kNumQualities) return kQualities[q].suffix;
    switch (q) { case 100: return "5"; case 200: return "add9"; case 201: return "m(add9)"; case 202: return "dim7";
                 case 203: return "9"; case 204: return "m7b5(b9)"; case 205: return "m(maj7)"; case 206: return "maj7#5"; default: return "(?)"; }
}

} // namespace am
