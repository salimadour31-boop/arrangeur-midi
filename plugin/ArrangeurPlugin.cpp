// ArrangeurPlugin.cpp — DPF wrapper: MIDI in -> Generator -> MIDI out (+ built-in preview synth audio).
#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "music/Generator.hpp"
#include "music/Synth.hpp"
#include <cstring>

START_NAMESPACE_DISTRHO

class ArrangeurPlugin : public Plugin {
public:
    ArrangeurPlugin() : Plugin(kParamCount, 0, 0) {
        for (int i = 0; i < kParamCount; ++i) values_[i] = kParamDefs[i].def;
        gen_.setSampleRate(getSampleRate()); synth_.setSampleRate(getSampleRate());
        apply();
    }

protected:
    const char* getLabel() const override { return "ArrangeurMIDI"; }
    const char* getDescription() const override { return "Chords, arpeggios, strum and bass from one key, in any scale (modes, maqam). MIDI out + preview synth."; }
    const char* getMaker() const override { return "salimadour31-boop"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    void initParameter(uint32_t index, Parameter& p) override {
        const ParamDef& d = kParamDefs[index];
        p.symbol = d.symbol; p.name = d.name; p.unit = d.unit;
        p.ranges.min = d.mn; p.ranges.max = d.mx; p.ranges.def = d.def;
        p.hints = kParameterIsAutomatable;
        if (d.integer) p.hints |= kParameterIsInteger;
        if (index == pVoiceLead || index == pLatch || index == pArpOn || index == pSynthOn) p.hints |= kParameterIsBoolean;
        if (index == pLang) p.hints = kParameterIsInteger | kParameterIsHidden;
        if (index >= oRoot) p.hints = kParameterIsOutput | kParameterIsInteger;
        if (index == oBpm) p.hints = kParameterIsOutput;
    }

    float getParameterValue(uint32_t index) const override { return index < kParamCount ? values_[index] : 0.f; }

    void setParameterValue(uint32_t index, float v) override {
        if (index >= uint32_t(oRoot)) return;
        const bool structural = index == pMode || index == pChChord || index == pChBass;
        if (structural && v != values_[index]) needPanic_ = true;
        values_[index] = v;
        apply();
    }

    void activate() override { gen_.setSampleRate(getSampleRate()); synth_.setSampleRate(getSampleRate()); needPanic_ = true; }
    void sampleRateChanged(double sr) override { gen_.setSampleRate(sr); synth_.setSampleRate(sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override {
        const TimePosition& tp = getTimePosition();
        am::Transport tr;
        tr.playing = tp.playing;
        tr.bpm = tp.bbt.valid && tp.bbt.beatsPerMinute > 1 ? tp.bbt.beatsPerMinute : 120.0;
        if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0) {
            const double beatsInUnit = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) + tp.bbt.tick / tp.bbt.ticksPerBeat;
            tr.beat = beatsInUnit * 4.0 / (tp.bbt.beatType > 0 ? tp.bbt.beatType : 4.0);    // in quarter notes
        } else {
            tr.beat = double(tp.frame) / getSampleRate() * tr.bpm / 60.0;
        }

        uint32_t nOut = 0;
        if (needPanic_) {      // all notes off (MIDI out and preview synth) before anything else in this block
            gen_.panic(out_, nOut, kMaxOut); emit(nOut);
            for (uint32_t i = 0; i < nOut; ++i) synth_.midi(out_[i].data);
            needPanic_ = false;
        }
        for (uint32_t i = 0; i < midiCount; ++i) {
            const MidiEvent& e = midi[i];
            if (e.size < 3) continue;
            const uint8_t* d = e.size > MidiEvent::kDataSize ? e.dataExt : e.data;
            const uint8_t st = d[0] & 0xF0;
            if (st == 0x90 && d[2] > 0) gen_.noteOn(d[1], d[2], e.frame);
            else if (st == 0x80 || (st == 0x90 && d[2] == 0)) gen_.noteOff(d[1], e.frame);
            else if (st == 0xB0 && (d[1] == 123 || d[1] == 120)) needPanic_ = true;
        }
        gen_.process(frames, tr, out_, nOut, kMaxOut);
        emit(nOut);

        // preview synth, sample-accurate with the generated events
        float* L = outputs[0]; float* R = outputs[1];
        std::memset(L, 0, sizeof(float) * frames);
        uint32_t pos = 0;
        for (uint32_t i = 0; i < nOut; ++i) {
            const uint32_t f = std::min(out_[i].frame, frames);
            if (f > pos) { if (synthOn_) synth_.render(L + pos, f - pos); pos = f; }
            synth_.midi(out_[i].data);
        }
        if (frames > pos && synthOn_) synth_.render(L + pos, frames - pos);
        std::memcpy(R, L, sizeof(float) * frames);

        // UI feedback
        const int root = gen_.currentRoot();
        values_[oRoot] = float(root);
        values_[oQuality] = float(gen_.currentQuality());
        uint32_t m[3] = {0, 0, 0};
        if (root >= 0) { const am::Chord& c = gen_.currentVoiced(); for (int i = 0; i < c.n; ++i) { const int k = c.notes[i] - 24; if (k >= 0 && k < 60) m[k / 20] |= 1u << (k % 20); } }
        values_[oMask0] = float(m[0]); values_[oMask1] = float(m[1]); values_[oMask2] = float(m[2]);
        values_[oBpm] = float(tr.bpm); values_[oPlaying] = tr.playing ? 1.f : 0.f;
    }

private:
    void emit(uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
            MidiEvent me; me.frame = out_[i].frame; me.size = 3;
            std::memcpy(me.data, out_[i].data, 3); me.dataExt = nullptr;
            writeMidiEvent(me);
        }
    }
    void apply() {
        am::Settings& s = gen_.settings();
        auto I = [&](ParamId p) { return int(values_[p] + (values_[p] >= 0 ? 0.5f : -0.5f)); };
        s.mode = I(pMode); s.keyMap = I(pKeyMap); s.tonic = I(pTonic); s.scale = I(pScale); s.chordType = I(pChordType);
        s.preset = I(pPreset); s.voicing = I(pVoicing); s.inversion = I(pInversion); s.octave = I(pOctave);
        s.voiceLead = I(pVoiceLead) != 0; s.latch = I(pLatch) != 0; s.strumMs = values_[pStrum]; s.strumDir = I(pStrumDir);
        s.arpOn = I(pArpOn) != 0; s.arpPattern = I(pArpPattern); s.arpRate = I(pArpRate); s.arpOctaves = I(pArpOct);
        s.swing = values_[pSwing] / 100.f; s.gate = values_[pGate] / 100.f; s.bassMode = I(pBassMode);
        s.humVel = values_[pHumVel] / 100.f; s.humTimeMs = values_[pHumTime]; s.velocity = float(I(pVelocity));
        s.chChord = I(pChChord) - 1; s.chBass = I(pChBass) - 1;
        synthOn_ = I(pSynthOn) != 0;
        synth_.setVolume(values_[pSynthVol] / 100.f); synth_.setTone(values_[pSynthTone] / 100.f); synth_.setRelease(values_[pSynthRelease] / 100.f);
    }

    static constexpr uint32_t kMaxOut = 2048;
    float values_[kParamCount] = {};
    am::Generator gen_;
    am::PreviewSynth synth_;
    am::MidiOut out_[kMaxOut];
    bool needPanic_ = true, synthOn_ = true;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangeurPlugin)
};

Plugin* createPlugin() { return new ArrangeurPlugin(); }

END_NAMESPACE_DISTRHO
