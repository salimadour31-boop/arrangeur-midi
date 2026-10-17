// MidiFile.hpp — offline rendering of a progression with the real-time Generator (same code, same
// result as live) and a minimal Standard MIDI File (format 1) writer. MIT licence.
#pragma once
#include "Generator.hpp"
#include <cstdio>
#ifdef _WIN32
#include <filesystem>
#include <fstream>
#endif
#include <string>
#include <vector>

namespace am {

struct TimedMidi { int64_t tick; uint8_t d[3]; };

// Render `barsPerChord` bars per chord of the current preset (white keys C, D, E...) through the Generator.
// Works in every mode: in Scale mode the keys are degrees I, II, III...; here we always walk the preset's
// chords via Progression mode so the file matches the chosen style. Ticks at `ppq` per quarter note.
inline std::vector<TimedMidi> renderProgression(Settings s, int barsPerChord = 1, int ppq = 480, int loops = 1, uint32_t seed = 1) {
    std::vector<TimedMidi> ev;
    if (s.mode == mThru) s.mode = mProgression;
    s.mode = mProgression; s.latch = false;
    const Preset& p = kPresets[std::max(0, std::min(kNumPresets - 1, s.preset))];
    static const int whiteKeys[7] = {60, 62, 64, 65, 67, 69, 71};
    Generator g; g.setSampleRate(48000); g.settings() = s; g.seed(seed);
    const double bpm = 120, spb = 48000.0 * 60.0 / bpm;     // samples per beat
    const int block = 480;                                   // 10 ms
    const int nChords = p.n * loops;
    const int64_t total = int64_t(nChords * barsPerChord * 4 * spb) + 48000;   // + 1 s tail for releases
    std::vector<MidiOut> buf(2048);
    int64_t t = 0; int chord = 0; bool keyDown = false;
    while (t < total) {
        // key presses at bar starts, release 1 ms before the next chord (legato like a human)
        const int64_t chordLen = int64_t(barsPerChord * 4 * spb);
        for (uint32_t f = 0; f < uint32_t(block); ++f) {
            const int64_t abs = t + f;
            if (chord < nChords && abs == int64_t(chord) * chordLen) { if (keyDown) g.noteOff(whiteKeys[(chord - 1) % p.n], f); g.noteOn(whiteKeys[chord % p.n], 100, f); keyDown = true; ++chord; }
            if (keyDown && chord == nChords && abs == int64_t(nChords) * chordLen - 48) { g.noteOff(whiteKeys[(chord - 1) % p.n], f); keyDown = false; }
        }
        Transport tr; tr.playing = true; tr.bpm = bpm; tr.beat = t / spb;
        uint32_t n = 0;
        g.process(uint32_t(block), tr, buf.data(), n, uint32_t(buf.size()));
        for (uint32_t i = 0; i < n; ++i) {
            TimedMidi m; m.tick = int64_t(std::llround((t + buf[i].frame) / spb * ppq));
            m.d[0] = buf[i].data[0]; m.d[1] = buf[i].data[1]; m.d[2] = buf[i].data[2];
            ev.push_back(m);
        }
        t += block;
    }
    return ev;
}

// Standard MIDI File, format 1: track 0 = tempo + name, then one track per MIDI channel used.
inline std::vector<uint8_t> buildSmf(const std::vector<TimedMidi>& ev, int ppq, double bpm, const std::string& name) {
    std::vector<uint8_t> out;
    auto be32 = [&](std::vector<uint8_t>& v, uint32_t x) { for (int i = 3; i >= 0; --i) v.push_back(uint8_t(x >> (8 * i))); };
    auto vlq = [&](std::vector<uint8_t>& v, uint32_t x) {
        uint8_t tmp[5]; int n = 0; tmp[n++] = x & 0x7F;
        while (x >>= 7) tmp[n++] = uint8_t(0x80 | (x & 0x7F));
        while (n) v.push_back(tmp[--n]);
    };
    auto track = [&](const std::vector<uint8_t>& body) { out.insert(out.end(), {'M', 'T', 'r', 'k'}); be32(out, uint32_t(body.size())); out.insert(out.end(), body.begin(), body.end()); };
    bool used[16] = {};
    for (auto& e : ev) used[e.d[0] & 0x0F] = true;
    int nTracks = 1; for (bool u : used) nTracks += u;
    out.insert(out.end(), {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1});
    out.push_back(uint8_t(nTracks >> 8)); out.push_back(uint8_t(nTracks));
    out.push_back(uint8_t(ppq >> 8)); out.push_back(uint8_t(ppq));
    {   // tempo track
        std::vector<uint8_t> b;
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x03}); vlq(b, uint32_t(name.size())); b.insert(b.end(), name.begin(), name.end());
        const uint32_t us = uint32_t(60000000.0 / bpm);
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x51, 0x03, uint8_t(us >> 16), uint8_t(us >> 8), uint8_t(us)});
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x58, 0x04, 4, 2, 24, 8});
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x2F, 0x00});
        track(b);
    }
    for (int ch = 0; ch < 16; ++ch) {
        if (!used[ch]) continue;
        std::vector<uint8_t> b; int64_t last = 0;
        const std::string tn = ch == 1 ? "Basse / Bass" : (ch == 0 ? "Accords / Chords" : "Canal " + std::to_string(ch + 1));
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x03}); vlq(b, uint32_t(tn.size())); b.insert(b.end(), tn.begin(), tn.end());
        for (auto& e : ev) {
            if ((e.d[0] & 0x0F) != ch) continue;
            vlq(b, uint32_t(std::max<int64_t>(0, e.tick - last))); last = std::max(last, e.tick);
            b.push_back(e.d[0]); b.push_back(e.d[1]); b.push_back(e.d[2]);
        }
        vlq(b, 0); b.insert(b.end(), {0xFF, 0x2F, 0x00});
        track(b);
    }
    return out;
}

// path is UTF-8 on every platform (wide API on Windows so accented user names work).
inline bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
#ifdef _WIN32
    std::ofstream o(std::filesystem::u8path(path), std::ios::binary);
    if (!o) return false;
    o.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    return bool(o);
#endif
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

} // namespace am
