// Synth.hpp — tiny polyphonic preview synth so the plugin makes sound on its own: two detuned
// band-limited saws (PolyBLEP) + sub sine, one-pole low-pass, ADSR. 16 voices, MIDI driven. MIT licence.
#pragma once
#include <cmath>
#include <cstdint>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace am {

class PreviewSynth {
public:
    void setSampleRate(double sr) { sr_ = float(sr > 0 ? sr : 48000); }
    void setTone(float t01) { tone_ = t01; }          // 0 = dark, 1 = bright
    void setVolume(float lin) { vol_ = lin; }
    void setRelease(float sec) { rel_ = sec; }
    int activeVoices() const { int n = 0; for (auto& v : v_) if (v.stage) ++n; return n; }

    void midi(const uint8_t* d) {
        const uint8_t st = d[0] & 0xF0;
        if (st == 0x90 && d[2] > 0) noteOn(d[1], d[2], d[0] & 0x0F);
        else if (st == 0x80 || (st == 0x90 && d[2] == 0)) noteOff(d[1], d[0] & 0x0F);
        else if (st == 0xB0 && (d[1] == 123 || d[1] == 120)) for (auto& v : v_) v.stage = 0;
    }

    // Adds into out (mono).
    void render(float* out, uint32_t n) {
        const float a = 1.0f - std::exp(-1.0f / (0.004f * sr_)), d = 1.0f - std::exp(-1.0f / (0.35f * sr_));
        const float r = 1.0f - std::exp(-6.9f / (std::max(0.01f, rel_) * sr_));   // -60 dB after rel_ s
        for (auto& v : v_) {
            if (!v.stage) continue;
            const float cutoffHz = 300.f + tone_ * tone_ * 9000.f + v.vel * 1500.f;
            const float lp = 1.0f - std::exp(-2.0f * float(M_PI) * std::min(cutoffHz, 0.45f * sr_) / sr_);
            for (uint32_t i = 0; i < n; ++i) {
                if (v.stage == 1) { v.env += (1.05f - v.env) * a; if (v.env >= 1.f) { v.env = 1.f; v.stage = 2; } }
                else if (v.stage == 2) v.env += (0.6f - v.env) * d;
                else { v.env -= v.env * r; if (v.env < 1e-4f) { v.stage = 0; break; } }
                const float s = 0.5f * (saw(v.p1, v.dt1) + saw(v.p2, v.dt2)) + 0.35f * std::sin(2.f * float(M_PI) * v.p3);
                v.p1 += v.dt1; if (v.p1 >= 1.f) v.p1 -= 1.f;
                v.p2 += v.dt2; if (v.p2 >= 1.f) v.p2 -= 1.f;
                v.p3 += v.dt1 * 0.5f; if (v.p3 >= 1.f) v.p3 -= 1.f;
                v.z += (s - v.z) * lp;
                out[i] += v.z * v.env * v.vel * vol_ * 0.18f;
            }
        }
    }

private:
    struct Voice { int stage = 0, note = 0, ch = 0; float vel = 0, env = 0, p1 = 0, p2 = 0, p3 = 0, dt1 = 0, dt2 = 0, z = 0; unsigned age = 0; };
    static float blep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    static float saw(float p, float dt) { return 2.f * p - 1.f - blep(p, dt); }

    void noteOn(int note, int vel, int ch) {
        Voice* best = nullptr;
        for (auto& v : v_) if (v.stage && v.note == note && v.ch == ch) best = &v;          // retrigger same note
        if (!best) for (auto& v : v_) if (!v.stage) { best = &v; break; }
        if (!best) { best = &v_[0]; for (auto& v : v_) if (v.age < best->age) best = &v; } // steal oldest
        const float f = 440.f * std::pow(2.f, (note - 69) / 12.f);
        best->note = note; best->ch = ch; best->vel = vel / 127.f; best->stage = 1; best->age = ++age_;
        best->dt1 = f * 1.003f / sr_; best->dt2 = f * 0.997f / sr_;
        if (best->env < 1e-3f) { best->p1 = 0.f; best->p2 = 0.37f; best->p3 = 0.f; best->z = 0.f; }
    }
    void noteOff(int note, int ch) { for (auto& v : v_) if (v.stage && v.stage != 3 && v.note == note && v.ch == ch) v.stage = 3; }

    Voice v_[16];
    float sr_ = 48000, tone_ = 0.5f, vol_ = 0.7f, rel_ = 0.3f;
    unsigned age_ = 0;
};

} // namespace am
