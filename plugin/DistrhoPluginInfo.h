// DistrhoPluginInfo.h — static description of the Arrangeur MIDI plugin for DPF.
#pragma once

#define DISTRHO_PLUGIN_BRAND   "salimadour31-boop"
#define DISTRHO_PLUGIN_NAME    "Arrangeur MIDI"
#define DISTRHO_PLUGIN_URI     "https://github.com/salimadour31-boop/arrangeur-midi"
#define DISTRHO_PLUGIN_CLAP_ID "io.github.salimadour31-boop.arrangeur-midi"

#define DISTRHO_PLUGIN_BRAND_ID  Slmd
#define DISTRHO_PLUGIN_UNIQUE_ID ArMd

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         1   // loads as an instrument (FL Studio: Channel Rack generator)
#define DISTRHO_PLUGIN_NUM_INPUTS       0
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2   // built-in preview synth (stereo)
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 1   // generated chords / arpeggio / bass for another instrument
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1   // host tempo and position for the arpeggiator and bass
#define DISTRHO_PLUGIN_WANT_STATE       0
#define DISTRHO_UI_USE_NANOVG           1
#define DISTRHO_UI_USER_RESIZABLE       0
#define DISTRHO_UI_DEFAULT_WIDTH        1000
#define DISTRHO_UI_DEFAULT_HEIGHT       620

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Generator"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "instrument", "note-effect", "synthesizer", "stereo"
#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:InstrumentPlugin"
