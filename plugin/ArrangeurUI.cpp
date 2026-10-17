// ArrangeurUI.cpp — NanoVG interface: current chord + keyboard, 7 clickable chord pads (audition),
// every parameter as selector / toggle / knob, MIDI file export. FR / EN / Taqbaylit.
#include "DistrhoUI.hpp"
#include "Params.hpp"
#include "music/MidiFile.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#ifdef _WIN32
# include <windows.h>
# include <direct.h>
#else
# include <sys/stat.h>
#endif

START_NAMESPACE_DISTRHO

namespace {
enum Kind { Knob, Selector, Toggle };
struct Control { ParamId id; Kind kind; float x, y, w; int labelKey; };

enum Str { sMode, sKeyMap, sTonic, sScale, sChordType, sPreset, sVoicing, sInversion, sOctave, sVoiceLead, sLatch, sStrumDir,
           sArp, sPattern, sRate, sArpOct, sBass, sChChord, sChBass, sStrum, sSwing, sGate, sHumVel, sHumTime, sVelocity,
           sSynth, sVol, sTone, sRelease, sHarmony, sVoicingHdr, sArpHdr, sFeelHdr, sChordNow, sNoChord, sPads, sExport,
           sExported, sExportFail, sHelp, sFollow, sStopped, sPlaying, sCount };
const char* const kStr[sCount][3] = {
    {"Mode", "Mode", "Askar"}, {"Touches", "Keys", "Tisura"}, {"Tonique", "Tonic", "Tasarut"}, {"Gamme / mode", "Scale / mode", "Askala"},
    {"Type d'accord", "Chord type", "Anaw n wakurd"}, {"Progression (style)", "Progression (style)", "Akala (aɣanib)"},
    {"Voicing", "Voicing", "Voicing"}, {"Renversement", "Inversion", "Asuzzer"}, {"Octave", "Octave", "Tamiwt"},
    {"Enchaînement fluide", "Voice leading", "Asenfel aleggaɣ"}, {"Maintien (latch)", "Latch", "Aṭṭaf (latch)"},
    {"Sens du strum", "Strum direction", "Tanila n strum"}, {"Arpège", "Arpeggiator", "Arpège"}, {"Motif", "Pattern", "Udem"},
    {"Vitesse (tempo hôte)", "Rate (host tempo)", "Arured (tempo)"}, {"Octaves", "Octaves", "Timiwin"}, {"Ligne de basse", "Bass line", "Abas"},
    {"Canal accords", "Chord channel", "Abadu n ikurdiyen"}, {"Canal basse", "Bass channel", "Abadu n ubas"},
    {"Strum", "Strum", "Strum"}, {"Swing", "Swing", "Swing"}, {"Durée", "Gate", "Tanzagt"}, {"Hum. vélocité", "Hum. velocity", "Agaman (tazmert)"},
    {"Hum. timing", "Hum. timing", "Agaman (akud)"}, {"Vélocité", "Velocity", "Tazmert"}, {"Synthé d'écoute", "Preview synth", "Asinti n tesliwt"},
    {"Volume", "Volume", "Ableɣ"}, {"Timbre", "Tone", "Ṭṭabeɛ"}, {"Relâche", "Release", "Aserreḥ"},
    {"Harmonie", "Harmony", "Tamyezzut"}, {"Voicing & jeu", "Voicing & playing", "Voicing d wurar"}, {"Arpège · basse · sortie", "Arp · bass · output", "Arpège · abas · tuffɣa"},
    {"Toucher · synthé", "Feel · synth", "Aḥulfu · asinti"}, {"Accord joué", "Current chord", "Akurd yettwurar"}, {"(appuyez sur une touche)", "(press a key)", "(ssed ɣef tsarut)"},
    {"Cliquez un pavé pour écouter — ou jouez Do4 à Si4 sur le clavier", "Click a pad to listen — or play C4 to B4 on your keyboard", "Sit ɣef tferkit akken ad tesleḍ — neɣ urar Do4 ar Si4"},
    {"Exporter MIDI", "Export MIDI", "Sifeḍ MIDI"}, {"Fichier créé :", "File written:", "Afaylu yettwaxleq :"}, {"Échec de l'écriture du fichier", "Could not write the file", "Ur yettwaru ara ufaylu"},
    {"glisser / molette · clic droit = défaut", "drag / wheel · right-click = default", "zuɣer / tawriqt · tasiḍt tayeffust = amezwer"},
    {"suit le clavier", "follows keyboard", "yeṭṭafar anasiw"}, {"arrêt", "stopped", "yeḥbes"}, {"lecture", "playing", "yettwurar"},
};
const char* const kModeNames[3][3] = {{"Accords de la gamme", "Progression (style)", "Désactivé (notes directes)"},
                                      {"Scale chords", "Progression (style)", "Off (notes through)"},
                                      {"Ikurdiyen n teskala", "Akala (aɣanib)", "Tensa (tinuṭin)"}};
const char* const kKeyMapNames[2][3] = {{"Touches blanches = degrés", "White keys = degrees", "Tisura timellalin = idekkuṛen"},
                                        {"Note jouée (aimantée)", "Played note (snapped)", "Tanuṭ yettwurar"}};
const char* const kInvNames[4][3] = {{"Fondamental", "Root position", "Aẓar"}, {"1er renversement", "1st inversion", "Asuzzer 1"},
                                     {"2e renversement", "2nd inversion", "Asuzzer 2"}, {"3e renversement", "3rd inversion", "Asuzzer 3"}};
const char* const kStrumDirNames[3][3] = {{"Grave → aigu", "Low → high", "Azal → afellay"}, {"Aigu → grave", "High → low", "Afellay → azal"}, {"Alterné", "Alternate", "Yettemyeqlab"}};
const char* const kRoman[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
const int kWhite[7] = {60, 62, 64, 65, 67, 69, 71};

std::string documentsDir() {
#ifdef _WIN32
    wchar_t* w = _wgetenv(L"USERPROFILE");
    std::string base;
    if (w) { char buf[1024]; const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof buf, nullptr, nullptr); if (n > 0) base = buf; }
    if (base.empty()) base = "C:\\";
    return base + "\\Documents\\Arrangeur MIDI";
#else
    const char* h = std::getenv("HOME"); std::string base = h ? h : "/tmp";
    struct stat st; std::string docs = base + "/Documents";
    if (stat(docs.c_str(), &st) != 0) docs = base;
    return docs + "/Arrangeur MIDI";
#endif
}
bool makeDir(const std::string& p) {
#ifdef _WIN32
    wchar_t w[1024]; if (!MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, w, 1024)) return false;
    _wmkdir(w); return true;
#else
    mkdir(p.c_str(), 0755); return true;
#endif
}
} // namespace

class ArrangeurUI : public UI {
public:
    ArrangeurUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
        loadSharedResources();
        for (int i = 0; i < kParamCount; ++i) values_[i] = kParamDefs[i].def;
        const float r0 = 318, dy = 42;
        auto sel = [&](ParamId id, float x, int row, int lk, Kind k = Selector) { ctl_[n_++] = {id, k, x, r0 + dy * row, 220, lk}; };
        sel(pMode, 20, 0, sMode); sel(pKeyMap, 20, 1, sKeyMap); sel(pTonic, 20, 2, sTonic); sel(pScale, 20, 3, sScale);
        sel(pChordType, 20, 4, sChordType); sel(pPreset, 20, 5, sPreset);
        sel(pVoicing, 265, 0, sVoicing); sel(pInversion, 265, 1, sInversion); sel(pOctave, 265, 2, sOctave);
        sel(pVoiceLead, 265, 3, sVoiceLead, Toggle); sel(pLatch, 265, 4, sLatch, Toggle); sel(pStrumDir, 265, 5, sStrumDir);
        sel(pArpOn, 510, 0, sArp, Toggle); sel(pArpPattern, 510, 1, sPattern); sel(pArpRate, 510, 2, sRate); sel(pArpOct, 510, 3, sArpOct);
        sel(pBassMode, 510, 4, sBass);
        ctl_[n_++] = {pChChord, Selector, 510, r0 + dy * 5, 105, sChChord};
        ctl_[n_++] = {pChBass, Selector, 625, r0 + dy * 5, 105, sChBass};
        ctl_[n_++] = {pSynthOn, Toggle, 760, r0, 220, sSynth};
        const ParamId knobs[9] = {pStrum, pSwing, pGate, pHumVel, pHumTime, pVelocity, pSynthVol, pSynthTone, pSynthRelease};
        const int labels[9] = {sStrum, sSwing, sGate, sHumVel, sHumTime, sVelocity, sVol, sTone, sRelease};
        for (int i = 0; i < 9; ++i) ctl_[n_++] = {knobs[i], Knob, 797.f + 73.f * (i % 3), 388.f + 78.f * (i / 3), 0, labels[i]};
    }

protected:
    void parameterChanged(uint32_t index, float value) override {
        if (index >= kParamCount) return;
        if (values_[index] != value) { values_[index] = value; repaint(); }
    }

    int lang() const { return std::max(0, std::min(2, int(values_[pLang]))); }
    const char* tr(int k) const { return kStr[k][lang()]; }
    int I(ParamId p) const { return int(std::lround(values_[p])); }
    std::string noteName(int pc) const { return lang() == 1 ? am::noteNameEn(pc) : am::noteNameFr(pc); }
    std::string chordName(int root, int q) const {
        if (root < 0) return "";
        std::string s = noteName(am::pcOf(root)), suf = am::qualitySuffix(q);
        return s + (lang() == 1 || suf.empty() ? "" : " ") + suf;
    }

    am::Settings settings() const {
        am::Settings s;
        s.mode = I(pMode); s.keyMap = I(pKeyMap); s.tonic = I(pTonic); s.scale = I(pScale); s.chordType = I(pChordType); s.preset = I(pPreset);
        s.voicing = I(pVoicing); s.inversion = I(pInversion); s.octave = I(pOctave); s.voiceLead = I(pVoiceLead); s.latch = false;
        s.strumMs = values_[pStrum]; s.strumDir = I(pStrumDir); s.arpOn = I(pArpOn); s.arpPattern = I(pArpPattern); s.arpRate = I(pArpRate);
        s.arpOctaves = I(pArpOct); s.swing = values_[pSwing] / 100.f; s.gate = values_[pGate] / 100.f; s.bassMode = I(pBassMode);
        s.humVel = values_[pHumVel] / 100.f; s.humTimeMs = values_[pHumTime]; s.velocity = float(I(pVelocity));
        s.chChord = I(pChChord) - 1; s.chBass = I(pChBass) - 1;
        return s;
    }

    // Pad i: which key to send and which chord it plays
    struct Pad { int key; am::Chord chord; std::string roman; bool valid; };
    Pad pad(int i) const {
        Pad p; p.valid = true;
        const am::Scale& sc = am::kScales[std::max(0, std::min(am::kNumScales - 1, I(pScale)))];
        if (I(pMode) == am::mProgression) {
            const am::Preset& pr = am::kPresets[std::max(0, std::min(am::kNumPresets - 1, I(pPreset)))];
            p.key = kWhite[i];
            if (i >= pr.n) { p.valid = false; return p; }
            p.chord = am::qualityChord(60 + I(pTonic) + pr.c[i].off, pr.c[i].q);
            // roman numeral i of "I–V–vi–IV"
            std::string r = pr.roman; int k = 0; size_t a = 0;
            while (k < i) { size_t b = r.find("–", a); if (b == std::string::npos) { a = r.size(); break; } a = b + 3; ++k; }
            size_t b = r.find("–", a); p.roman = r.substr(a, b == std::string::npos ? std::string::npos : b - a);
            return p;
        }
        if (I(pMode) == am::mThru) { p.key = kWhite[i]; p.chord.add(p.key); p.chord.root = p.key; p.roman = ""; return p; }
        if (i >= sc.n) { p.valid = false; p.key = kWhite[i]; return p; }
        p.chord = am::diatonicChord(sc, I(pTonic), i, am::ChordType(I(pChordType)));
        p.key = I(pKeyMap) == am::kmWhiteKeys ? kWhite[i] : 60 + I(pTonic) + sc.iv[i];
        const int q = am::identifyQuality(p.chord);
        std::string r = kRoman[i];
        if (q == am::qMin || q == am::qMin7 || q == am::qDim || q == am::qHalfDim || q == am::qMin9 || q == 201 || q == 202)
            for (auto& ch : r) ch = char(std::tolower(ch));
        if (q == am::qDim || q == am::qHalfDim || q == 202) r += "°";
        p.roman = r;
        return p;
    }

    void label(float x, float y, const char* s, float size, Color c, int align = ALIGN_LEFT | ALIGN_MIDDLE) {
        if (s == nullptr || s[0] == '\0') return;
        fontSize(size); fillColor(c); textAlign(align); text(x, y, s, nullptr);
    }
    void panel(float x, float y, float w, float h) {
        beginPath(); roundedRect(x, y, w, h, 10); fillColor(Color(28, 31, 40)); fill();
        strokeColor(Color(48, 53, 66)); strokeWidth(1); stroke();
    }

    void onNanoDisplay() override {
        const float W = getWidth(), H = getHeight();
        const Color ink(233, 236, 242), muted(150, 158, 175), mint(143, 211, 193), amber(255, 180, 84), dark(13, 36, 32);
        beginPath(); rect(0, 0, W, H); fillColor(Color(17, 19, 25)); fill();

        // header
        label(20, 26, "Arrangeur MIDI", 24, ink);
        label(200, 28, "v0.1 · MIT", 12, muted);
        char buf[256];
        std::snprintf(buf, sizeof buf, "%.1f bpm · %s", double(values_[oBpm]), values_[oPlaying] > 0.5f ? tr(sPlaying) : tr(sStopped));
        label(W - 200, 27, buf, 12, values_[oPlaying] > 0.5f ? mint : muted, ALIGN_RIGHT | ALIGN_MIDDLE);
        const char* langs[3] = {"FR", "EN", "KAB"};
        for (int i = 0; i < 3; ++i) {
            const float x = W - 170 + i * 52;
            beginPath(); roundedRect(x, 13, 46, 26, 6); fillColor(i == lang() ? mint : Color(36, 40, 52)); fill();
            label(x + 23, 26, langs[i], 13, i == lang() ? dark : ink, ALIGN_CENTER | ALIGN_MIDDLE);
        }

        // current chord + keyboard
        panel(16, 50, W - 32, 140);
        label(32, 72, tr(sChordNow), 13, muted);
        const int root = int(values_[oRoot]);
        if (root >= 0) label(32, 112, chordName(root, int(values_[oQuality])).c_str(), 36, ink);
        else label(32, 110, tr(sNoChord), 15, muted);
        const am::Scale& sc = am::kScales[std::max(0, std::min(am::kNumScales - 1, I(pScale)))];
        std::string ctx = noteName(I(pTonic)) + " " + (lang() == 0 ? sc.fr : lang() == 1 ? sc.en : sc.kab);
        if (I(pMode) == am::mProgression) { const am::Preset& pr = am::kPresets[I(pPreset)]; ctx = noteName(I(pTonic)) + " · " + (lang() == 0 ? pr.fr : lang() == 1 ? pr.en : pr.kab) + " · " + pr.roman; }
        label(32, 150, ctx.c_str(), 14, mint);
        // keyboard 24..83 (5 octaves) with output notes lit
        const uint32_t mask[3] = {uint32_t(values_[oMask0]), uint32_t(values_[oMask1]), uint32_t(values_[oMask2])};
        auto lit = [&](int n) { const int k = n - 24; return k >= 0 && k < 60 && (mask[k / 20] >> (k % 20) & 1u); };
        const float kx = 300, ky = 62, kw = W - 32 - 300, kh = 112; const int nWhite = 35; const float ww = kw / nWhite;
        auto inScale = [&](int n) { const int rel = am::pcOf(n - I(pTonic)); for (int i = 0; i < sc.n; ++i) if (sc.iv[i] == rel) return true; return false; };
        int wi = 0;
        for (int n = 24; n < 84; ++n) {
            static const bool black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
            if (black[n % 12]) continue;
            beginPath(); rect(kx + wi * ww, ky, ww - 1, kh); fillColor(lit(n) ? mint : Color(222, 226, 233)); fill();
            if (I(pMode) == am::mScale && inScale(n) && !lit(n)) { beginPath(); circle(kx + wi * ww + ww / 2, ky + kh - 9, 2.5f); fillColor(Color(120, 170, 160)); fill(); }
            if (n % 12 == 0) { std::snprintf(buf, sizeof buf, "%s%d", lang() == 1 ? "C" : "Do", n / 12 - 1); label(kx + wi * ww + 2, ky + kh + 9, buf, 9, muted); }
            ++wi;
        }
        wi = 0;
        for (int n = 24; n < 84; ++n) {
            static const bool black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
            if (!black[n % 12]) { ++wi; continue; }
            beginPath(); rect(kx + wi * ww - ww * 0.3f, ky, ww * 0.6f, kh * 0.6f); fillColor(lit(n) ? Color(90, 180, 160) : Color(30, 33, 42)); fill();
        }

        // pads
        label(20, 206, tr(sPads), 12, muted);
        const float px = 20, py = 216, pw = (W - 40 - 6 * 10) / 7.f, ph = 70;
        for (int i = 0; i < 7; ++i) {
            const Pad p = pad(i);
            const float x = px + i * (pw + 10);
            const bool down = padDown_ == i;
            beginPath(); roundedRect(x, py, pw, ph, 9);
            fillColor(!p.valid ? Color(24, 26, 33) : down ? mint : Color(36, 40, 52)); fill();
            if (!p.valid) continue;
            const std::string name = I(pMode) == am::mThru ? noteName(am::pcOf(p.key)) : chordName(p.chord.root, am::identifyQuality(p.chord));
            label(x + pw / 2, py + 26, name.c_str(), 18, down ? dark : ink, ALIGN_CENTER | ALIGN_MIDDLE);
            label(x + pw / 2, py + 48, p.roman.c_str(), 13, down ? dark : amber, ALIGN_CENTER | ALIGN_MIDDLE);
            std::snprintf(buf, sizeof buf, "%s%d", noteName(am::pcOf(p.key)).c_str(), p.key / 12 - 1);
            label(x + pw / 2, py + 62, buf, 10, down ? dark : muted, ALIGN_CENTER | ALIGN_MIDDLE);
        }

        // control panels
        panel(12, 296, 240, 270); panel(257, 296, 240, 270); panel(502, 296, 240, 270); panel(747, 296, W - 759, 270);
        label(24, 306, tr(sHarmony), 11, mint); label(269, 306, tr(sVoicingHdr), 11, mint);
        label(514, 306, tr(sArpHdr), 11, mint); label(759, 306, tr(sFeelHdr), 11, mint);
        for (int i = 0; i < n_; ++i) drawControl(ctl_[i], ink, muted, mint);

        // export bar
        beginPath(); roundedRect(16, H - 44, 170, 32, 8); fillColor(exportFlash_ > 0 ? amber : mint); fill();
        label(101, H - 28, tr(sExport), 14, dark, ALIGN_CENTER | ALIGN_MIDDLE);
        if (!exportMsg_.empty()) label(198, H - 28, exportMsg_.c_str(), 11, exportOk_ ? ink : amber);
        else label(198, H - 28, tr(sHelp), 11, muted);
    }

    std::string valueText(ParamId id) const {
        const int v = I(id), L = lang();
        char buf[64];
        switch (id) {
            case pMode: return kModeNames[L][std::max(0, std::min(2, v))];
            case pKeyMap: return kKeyMapNames[std::max(0, std::min(1, v))][L];
            case pTonic: return noteName(v);
            case pScale: { const am::Scale& s = am::kScales[std::max(0, std::min(am::kNumScales - 1, v))]; return L == 0 ? s.fr : L == 1 ? s.en : s.kab; }
            case pChordType: return (L == 0 ? am::kChordTypeFr : L == 1 ? am::kChordTypeEn : am::kChordTypeKab)[std::max(0, std::min(am::kNumChordTypes - 1, v))];
            case pPreset: { const am::Preset& p = am::kPresets[std::max(0, std::min(am::kNumPresets - 1, v))]; return std::string(L == 0 ? p.fr : L == 1 ? p.en : p.kab); }
            case pVoicing: return (L == 0 ? am::kVoicingFr : L == 1 ? am::kVoicingEn : am::kVoicingKab)[std::max(0, std::min(3, v))];
            case pInversion: return kInvNames[std::max(0, std::min(3, v))][L];
            case pOctave: std::snprintf(buf, sizeof buf, "%+d", v); return v == 0 ? "0" : buf;
            case pStrumDir: return kStrumDirNames[std::max(0, std::min(2, v))][L];
            case pArpPattern: return (L == 0 ? am::kArpFr : L == 1 ? am::kArpEn : am::kArpKab)[std::max(0, std::min(am::kNumArpPatterns - 1, v))];
            case pArpRate: return am::kRateNames[std::max(0, std::min(am::kNumRates - 1, v))];
            case pArpOct: std::snprintf(buf, sizeof buf, "%d", v); return buf;
            case pBassMode: return (L == 0 ? am::kBassFr : L == 1 ? am::kBassEn : am::kBassKab)[std::max(0, std::min(am::kNumBassModes - 1, v))];
            case pChChord: case pChBass: std::snprintf(buf, sizeof buf, "ch %d", v); return buf;
            default: return "";
        }
    }

    void drawControl(const Control& c, Color ink, Color muted, Color mint) {
        const float v = values_[c.id];
        const ParamDef& d = kParamDefs[c.id];
        char buf[64];
        if (c.kind == Knob) {
            const float r = 19, cx = c.x, cy = c.y, t = (v - d.mn) / (d.mx - d.mn);
            const float a0 = 0.75f * float(M_PI), a1 = a0 + 1.5f * float(M_PI) * t;
            beginPath(); arc(cx, cy, r, a0, 2.25f * float(M_PI), CW); strokeColor(Color(48, 53, 66)); strokeWidth(5); stroke();
            beginPath(); arc(cx, cy, r, a0, std::max(a0 + 0.001f, a1), CW); strokeColor(mint); strokeWidth(5); stroke();
            if (c.id == pVelocity) { if (v < 0.5f) std::snprintf(buf, sizeof buf, "%s", "auto"); else std::snprintf(buf, sizeof buf, "%.0f", double(v)); }
            else if (c.id == pSynthRelease) std::snprintf(buf, sizeof buf, "%.2fs", double(v) / 100.0);
            else std::snprintf(buf, sizeof buf, "%.0f%s", double(v), d.unit);
            label(cx, cy, buf, 10, ink, ALIGN_CENTER | ALIGN_MIDDLE);
            label(cx, cy + 31, tr(c.labelKey), 11, muted, ALIGN_CENTER | ALIGN_MIDDLE);
            if (c.id == pVelocity && v < 0.5f) label(cx, cy + 43, tr(sFollow), 9, muted, ALIGN_CENTER | ALIGN_MIDDLE);
            return;
        }
        label(c.x, c.y, tr(c.labelKey), 11, muted);
        const float y = c.y + 6, h = 24;
        const bool on = c.kind == Toggle && v > 0.5f;
        beginPath(); roundedRect(c.x, y, c.w, h, 6); fillColor(on ? mint : Color(36, 40, 52)); fill();
        const std::string s = c.kind == Toggle ? (on ? "ON" : "OFF") : valueText(c.id);
        label(c.x + c.w / 2, y + h / 2, s.c_str(), c.w < 150 ? 11 : 12, on ? Color(13, 36, 32) : ink, ALIGN_CENTER | ALIGN_MIDDLE);
        if (c.kind == Selector) { label(c.x + 9, y + h / 2, "‹", 15, muted, ALIGN_CENTER | ALIGN_MIDDLE); label(c.x + c.w - 9, y + h / 2, "›", 15, muted, ALIGN_CENTER | ALIGN_MIDDLE); }
    }

    int hit(double px, double py) const {
        for (int i = 0; i < n_; ++i) {
            const Control& c = ctl_[i];
            if (c.kind == Knob) { if (std::hypot(px - c.x, py - c.y) < 24) return i; }
            else if (px >= c.x && px <= c.x + c.w && py >= c.y + 4 && py <= c.y + 32) return i;
        }
        return -1;
    }
    void set(const Control& c, float v) {
        const ParamDef& d = kParamDefs[c.id];
        v = std::max(d.mn, std::min(d.mx, v));
        if (d.integer) v = std::round(v);
        values_[c.id] = v; setParameterValue(c.id, v); repaint();
    }

    void doExport() {
        const am::Settings s = settings();
        const auto ev = am::renderProgression(s, 1, 480, 2);
        const am::Preset& pr = am::kPresets[std::max(0, std::min(am::kNumPresets - 1, s.preset))];
        const auto smf = am::buildSmf(ev, 480, 120, std::string("Arrangeur MIDI - ") + pr.en);
        const std::string dir = documentsDir();
        makeDir(dir);
        std::time_t t = std::time(nullptr); char ts[32]; std::strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", std::localtime(&t));
        std::string name = std::string(pr.en);
        for (auto& ch : name) if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '-';
        const std::string path = dir + (dir.find('\\') != std::string::npos ? "\\" : "/") + name + "-" + am::noteNameEn(s.tonic) + "-" + ts + ".mid";
        exportOk_ = am::writeFile(path, smf);
        exportMsg_ = exportOk_ ? std::string(tr(sExported)) + " " + path : std::string(tr(sExportFail)) + " (" + path + ")";
        exportFlash_ = 20;
        repaint();
    }

    void uiIdle() override { if (exportFlash_ > 0 && --exportFlash_ == 0) repaint(); }

    bool onMouse(const MouseEvent& ev) override {
        const double x = ev.pos.getX(), y = ev.pos.getY();
        const float W = getWidth(), H = getHeight();
        if (!ev.press) {
            if (drag_ >= 0) editParameter(ctl_[drag_].id, false);
            drag_ = -1;
            if (padDown_ >= 0) { sendNote(0, uint8_t(padKey_), 0); padDown_ = -1; repaint(); }
            return false;
        }
        for (int i = 0; i < 3; ++i) { const float bx = W - 170 + i * 52; if (x >= bx && x <= bx + 46 && y >= 13 && y <= 39) { values_[pLang] = float(i); setParameterValue(pLang, float(i)); repaint(); return true; } }
        if (x >= 16 && x <= 186 && y >= H - 44 && y <= H - 12) { doExport(); return true; }
        // pads
        const float pw = (W - 40 - 6 * 10) / 7.f;
        if (y >= 216 && y <= 286 && ev.button == 1) for (int i = 0; i < 7; ++i) {
            const float px = 20 + i * (pw + 10);
            if (x >= px && x <= px + pw) { const Pad p = pad(i); if (!p.valid) return true; padDown_ = i; padKey_ = p.key; sendNote(0, uint8_t(p.key), 100); repaint(); return true; }
        }
        const int h = hit(x, y); if (h < 0) return false;
        const Control& c = ctl_[h];
        const ParamDef& d = kParamDefs[c.id];
        editParameter(c.id, true);
        if (ev.button == 3) { set(c, d.def); editParameter(c.id, false); return true; }
        if (c.kind == Knob) { drag_ = h; dragY_ = y; dragV_ = values_[c.id]; return true; }
        if (c.kind == Toggle) set(c, values_[c.id] > 0.5f ? 0.f : 1.f);
        else { float v = values_[c.id] + (x < c.x + c.w * 0.3f ? -1 : 1); if (v > d.mx) v = d.mn; if (v < d.mn) v = d.mx; set(c, v); }
        editParameter(c.id, false);
        return true;
    }
    bool onMotion(const MotionEvent& ev) override {
        if (drag_ < 0) return false;
        const Control& c = ctl_[drag_]; const ParamDef& d = kParamDefs[c.id];
        set(c, dragV_ + float(dragY_ - ev.pos.getY()) / 200.f * (d.mx - d.mn));
        return true;
    }
    bool onScroll(const ScrollEvent& ev) override {
        const int h = hit(ev.pos.getX(), ev.pos.getY()); if (h < 0) return false;
        const Control& c = ctl_[h]; const ParamDef& d = kParamDefs[c.id];
        const float step = c.kind == Knob ? (d.integer ? 1.f : (d.mx - d.mn) / 50.f) : 1.f;
        editParameter(c.id, true); set(c, values_[c.id] + (ev.delta.getY() > 0 ? step : -step)); editParameter(c.id, false);
        return true;
    }

private:
    float values_[kParamCount] = {};
    Control ctl_[32]; int n_ = 0;
    int drag_ = -1; double dragY_ = 0; float dragV_ = 0;
    int padDown_ = -1, padKey_ = 60;
    std::string exportMsg_; bool exportOk_ = false; int exportFlash_ = 0;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangeurUI)
};

UI* createUI() { return new ArrangeurUI(); }

END_NAMESPACE_DISTRHO
