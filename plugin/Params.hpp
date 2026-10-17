// Params.hpp — parameter indices shared by the DSP and the UI.
#pragma once

enum ParamId {
    pMode = 0, pKeyMap, pTonic, pScale, pChordType, pPreset, pVoicing, pInversion, pOctave, pVoiceLead, pLatch,
    pStrum, pStrumDir, pArpOn, pArpPattern, pArpRate, pArpOct, pSwing, pGate, pBassMode, pHumVel, pHumTime,
    pVelocity, pChChord, pChBass, pSynthOn, pSynthVol, pSynthTone, pSynthRelease, pLang,
    // outputs (DSP -> UI)
    oRoot, oQuality, oMask0, oMask1, oMask2, oBpm, oPlaying,
    kParamCount
};

struct ParamDef { const char* symbol; const char* name; float mn, mx, def; bool integer; const char* unit; };
static const ParamDef kParamDefs[kParamCount] = {
    {"mode", "Mode", 0, 2, 0, true, ""},
    {"keymap", "Key map", 0, 1, 0, true, ""},
    {"tonic", "Tonic", 0, 11, 0, true, ""},
    {"scale", "Scale", 0, 18, 0, true, ""},
    {"chord_type", "Chord type", 0, 6, 0, true, ""},
    {"preset", "Progression preset", 0, 11, 0, true, ""},
    {"voicing", "Voicing", 0, 3, 0, true, ""},
    {"inversion", "Inversion", 0, 3, 0, true, ""},
    {"octave", "Octave", -2, 2, 0, true, ""},
    {"voice_lead", "Voice leading", 0, 1, 1, true, ""},
    {"latch", "Latch", 0, 1, 0, true, ""},
    {"strum", "Strum", 0, 300, 0, false, "ms"},
    {"strum_dir", "Strum direction", 0, 2, 0, true, ""},
    {"arp_on", "Arpeggiator", 0, 1, 0, true, ""},
    {"arp_pattern", "Arp pattern", 0, 6, 0, true, ""},
    {"arp_rate", "Arp rate", 0, 7, 5, true, ""},
    {"arp_octaves", "Arp octaves", 1, 4, 1, true, ""},
    {"swing", "Swing", 0, 100, 0, false, "%"},
    {"gate", "Gate", 5, 100, 80, false, "%"},
    {"bass_mode", "Bass line", 0, 5, 0, true, ""},
    {"hum_vel", "Humanize velocity", 0, 100, 0, false, "%"},
    {"hum_time", "Humanize timing", 0, 40, 0, false, "ms"},
    {"velocity", "Velocity", 0, 127, 0, true, ""},
    {"ch_chord", "Chord channel", 1, 16, 1, true, ""},
    {"ch_bass", "Bass channel", 1, 16, 2, true, ""},
    {"synth_on", "Preview synth", 0, 1, 1, true, ""},
    {"synth_vol", "Synth volume", 0, 100, 70, false, "%"},
    {"synth_tone", "Synth tone", 0, 100, 50, false, "%"},
    {"synth_release", "Synth release", 2, 300, 30, false, "cs"},
    {"lang", "Language", 0, 2, 0, true, ""},
    {"out_root", "Chord root", -1, 127, -1, true, ""},
    {"out_quality", "Chord quality", -1, 300, -1, true, ""},
    {"out_mask0", "Notes 24-43", 0, 1048575, 0, true, ""},
    {"out_mask1", "Notes 44-63", 0, 1048575, 0, true, ""},
    {"out_mask2", "Notes 64-83", 0, 1048575, 0, true, ""},
    {"out_bpm", "Tempo", 0, 999, 120, false, "bpm"},
    {"out_playing", "Host playing", 0, 1, 0, true, ""},
};
