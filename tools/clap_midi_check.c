/* clap_midi_check.c — minimal CLAP host used to verify the built plugin end to end: loads it, lists its
 * audio/note ports, plays notes into it with a running transport (120 bpm) and checks the MIDI notes it
 * outputs (chord, arpeggio on the tempo grid, bass channel) and the preview-synth audio.
 * Build (Linux): gcc -Idpf/distrho/src tools/clap_midi_check.c -o clap_midi_check -ldl -lm */
#include <clap/entry.h>
#include <clap/plugin-factory.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/params.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const void* host_get_ext(const clap_host_t* h, const char* id) { (void)h; (void)id; return NULL; }
static void noop(const clap_host_t* h) { (void)h; }
static const clap_host_t host = { CLAP_VERSION_INIT, NULL, "clap_midi_check", "arrangeur-midi", "", "0.1", host_get_ext, noop, noop, noop };

/* input events for one block */
static union { clap_event_header_t h; clap_event_note_t n; clap_event_param_value_t p; } in_ev[16];
static uint32_t n_in = 0;
static uint32_t ev_size(const clap_input_events_t* l) { (void)l; return n_in; }
static const clap_event_header_t* ev_get(const clap_input_events_t* l, uint32_t i) { (void)l; return &in_ev[i].h; }
/* captured output notes */
typedef struct { long t; int on, ch, key, vel; } Out;
static Out outs[4096]; static int n_out = 0; static long block_t = 0;
static bool ev_push(const clap_output_events_t* l, const clap_event_header_t* e) {
    (void)l;
    if (n_out >= 4096) return true;
    if (e->type == CLAP_EVENT_NOTE_ON || e->type == CLAP_EVENT_NOTE_OFF) {
        const clap_event_note_t* n = (const clap_event_note_t*)e;
        outs[n_out++] = (Out){block_t + e->time, e->type == CLAP_EVENT_NOTE_ON, n->channel, n->key, (int)lround(n->velocity * 127)};
    } else if (e->type == CLAP_EVENT_MIDI) {
        const clap_event_midi_t* m = (const clap_event_midi_t*)e;
        const int st = m->data[0] & 0xF0;
        if (st == 0x90 || st == 0x80) outs[n_out++] = (Out){block_t + e->time, st == 0x90 && m->data[2] > 0, m->data[0] & 15, m->data[1], m->data[2]};
    }
    return true;
}
static void note(int type, uint32_t time, int key) {
    clap_event_note_t* n = &in_ev[n_in++].n; memset(n, 0, sizeof *n);
    n->header.size = sizeof *n; n->header.time = time; n->header.space_id = CLAP_CORE_EVENT_SPACE_ID; n->header.type = (uint16_t)type;
    n->note_id = -1; n->port_index = 0; n->channel = 0; n->key = (int16_t)key; n->velocity = 0.8;
}
static void param(const clap_plugin_params_t* pp, const clap_plugin_t* p, const char* name, double v) {
    for (uint32_t i = 0; i < pp->count(p); ++i) { clap_param_info_t info; pp->get_info(p, i, &info);
        if (!strcmp(info.name, name)) { clap_event_param_value_t* e = &in_ev[n_in++].p; memset(e, 0, sizeof *e);
            e->header.size = sizeof *e; e->header.type = CLAP_EVENT_PARAM_VALUE; e->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            e->param_id = info.id; e->cookie = info.cookie; e->note_id = -1; e->port_index = -1; e->channel = -1; e->key = -1; e->value = v; return; } }
    printf("param %s not found\n", name);
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    void* so = dlopen(argv[1], RTLD_NOW); if (!so) { printf("dlopen: %s\n", dlerror()); return 1; }
    const clap_plugin_entry_t* entry = dlsym(so, "clap_entry");
    entry->init(argv[1]);
    const clap_plugin_factory_t* f = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t* d = f->get_plugin_descriptor(f, 0);
    printf("plugin: %s (%s) v%s; features:", d->name, d->id, d->version);
    for (const char* const* ft = d->features; *ft; ++ft) printf(" %s", *ft);
    printf("\n");
    const clap_plugin_t* p = f->create_plugin(f, &host, d->id);
    p->init(p);
    const clap_plugin_audio_ports_t* ap = p->get_extension(p, CLAP_EXT_AUDIO_PORTS);
    printf("audio in %u, audio out %u\n", ap->count(p, true), ap->count(p, false));
    const clap_plugin_note_ports_t* np = p->get_extension(p, CLAP_EXT_NOTE_PORTS);
    printf("note ports: in %u, out %u\n", np->count(p, true), np->count(p, false));
    const clap_plugin_params_t* pp = p->get_extension(p, CLAP_EXT_PARAMS);
    printf("parameters: %u\n", pp->count(p));
    p->activate(p, 48000, 32, 512);
    p->start_processing(p);
    static float ol[512], orr[512]; float* out[2] = {ol, orr};
    clap_audio_buffer_t aout[1] = {{out, NULL, 2, 0, 0}};
    clap_input_events_t ie = {NULL, ev_size, ev_get}; clap_output_events_t oe = {NULL, ev_push};
    clap_event_transport_t tr; memset(&tr, 0, sizeof tr);
    double peak = 0; int nan = 0, fails = 0;
    /* phase A (0-1 s): chord mode, key C4 at 0.1 s, release at 0.6 s; bass on quarter notes
       phase B (1-2 s): arpeggiator 1/16, key D4 at 1.0 s (on the beat grid), release at 1.9 s */
    for (int b = 0; b < 2 * 48000 / 512 + 20; ++b) {
        block_t = (long)b * 512; n_in = 0;
        if (b == 0) { param(pp, p, "Bass line", 2); }
        if (block_t <= 4800 && 4800 < block_t + 512) note(CLAP_EVENT_NOTE_ON, 4800 - block_t, 60);
        if (block_t <= 28800 && 28800 < block_t + 512) note(CLAP_EVENT_NOTE_OFF, 28800 - block_t, 60);
        if (block_t <= 47000 && 47000 < block_t + 512) { param(pp, p, "Arpeggiator", 1); param(pp, p, "Bass line", 0); }
        if (block_t <= 48000 && 48000 < block_t + 512) note(CLAP_EVENT_NOTE_ON, 48000 - block_t, 62);
        if (block_t <= 91200 && 91200 < block_t + 512) note(CLAP_EVENT_NOTE_OFF, 91200 - block_t, 62);
        tr.header.size = sizeof tr; tr.header.type = CLAP_EVENT_TRANSPORT;
        tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_IS_PLAYING | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
        tr.tempo = 120; tr.tsig_num = 4; tr.tsig_denom = 4;
        tr.song_pos_beats = (clap_beattime)llround(block_t / 24000.0 * CLAP_BEATTIME_FACTOR);
        tr.bar_start = (clap_beattime)llround(floor(block_t / 96000.0) * 4 * CLAP_BEATTIME_FACTOR);
        tr.bar_number = (int32_t)(block_t / 96000);
        clap_process_t pr; memset(&pr, 0, sizeof pr);
        pr.frames_count = 512; pr.audio_outputs = aout; pr.audio_outputs_count = 1;
        pr.in_events = &ie; pr.out_events = &oe; pr.steady_time = block_t; pr.transport = &tr;
        p->process(p, &pr);
        for (int i = 0; i < 512; ++i) { if (isnan(ol[i])) nan++; if (fabs(ol[i]) > peak) peak = fabs(ol[i]); }
    }
    int chordOn = 0, bassOn = 0, arpOn = 0, offs = 0, onGrid = 1;
    for (int i = 0; i < n_out; ++i) {
        Out* o = &outs[i];
        if (!o->on) { offs++; continue; }
        if (o->t < 48000) { if (o->ch == 0) chordOn++; else if (o->ch == 1) bassOn++; }
        else { arpOn++; if ((o->t - 48000) % 6000 != 0) onGrid = 0; }
    }
    printf("phase A: %d chord note-ons (ch1) at", chordOn);
    for (int i = 0; i < n_out && outs[i].t < 48000; ++i) if (outs[i].on && outs[i].ch == 0) printf(" %d@%ld", outs[i].key, outs[i].t);
    printf("; %d bass note-ons (ch2)\n", bassOn);
    for (int i = 0; i < n_out; ++i) if (outs[i].t >= 48000 && outs[i].on) printf(" %d@%ld", outs[i].key, outs[i].t);
    printf("\n");
    printf("phase B: %d arpeggio notes, all on the 1/16 grid: %s\n", arpOn, onGrid ? "yes" : "no");
    printf("note-offs: %d (note-ons %d)\n", offs, chordOn + bassOn + arpOn);
    printf("preview synth: peak %.3f, NaN %d\n", peak, nan);
    if (chordOn != 3) fails++;
    if (bassOn != 1) fails++;   /* quarter note at 0.5 s only */
    if (arpOn != 8 || !onGrid) fails++;  /* 0.9 s of 1/16 at 120 bpm */
    if (offs != chordOn + bassOn + arpOn) fails++;
    if (peak < 0.02 || nan) fails++;
    p->stop_processing(p); p->deactivate(p); p->destroy(p); entry->deinit();
    printf("%s\n", fails ? "FAILED" : "OK");
    return fails ? 1 : 0;
}
