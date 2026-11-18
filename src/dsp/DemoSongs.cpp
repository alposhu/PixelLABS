#include "DemoSongs.h"
#include "../plugin/PixelLabsIds.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace PixelLabs {
namespace {

// =============================================================================
// Song book format
// =============================================================================
//
// A song is a list of parts played in `form` order. Each part has
//   chords: "Dm7:4 Bbmaj7:4 ..."  chord name : length in beats
//   melody: "d5:4 e5:2 r:2 _:4 |"  note+octave : length in steps (16th notes in 4/4),
//           r = rest, _ = hold the previous note longer, | = bar line (checked).
//           A length can be left out to repeat the previous one.
// The accompaniment (bass, chords, drones, echoes, canons) is generated from the
// chords by the song's style, so every part stays in harmony with its melody.

enum Style {
    // Retro: tracks are melody, bass, arpeggio, pad, echo of the melody
    kStChipDrive = 0, kStChipMarch, kStChipBallad, kStChipGallop, kStChipWaltz,
    // Medieval: melody, lute/bass, harmony or canon, drone, second canon
    kStEstampie, kStChant, kStLute, kStJig, kStRound, kStProcession,
    // Ambient piano: melody, bass, broken chords, held chord tone, high sparkle
    kStPiano, kStDrift, kStMusicBox, kStPulse, kStWaltzAmb
};

struct PartDef { const char* id; const char* chords; const char* melody; };

struct SongDef {
    const char* name;
    int group;
    int bpm;          // beats per minute
    int beatSteps;    // sequencer steps per beat: 4 = 16th notes, 6 for 6/8 dotted-quarter beats
    int barBeats;     // beats per bar
    int style;
    const char* key;  // tonic and mode, e.g. "D dorian"
    int canonBars;    // rounds: bars between canon entries (0 = none)
    const char* form; // part ids in play order, e.g. "I A A B A"
    float vel[kDemoTracks];
    PartDef parts[4];
};

#include "DemoSongBook.inc"

constexpr int kNumClassicSongs = 4;
constexpr int kNumBookSongs = static_cast<int>(sizeof(kSongBook) / sizeof(kSongBook[0]));
static_assert(kNumClassicSongs + kNumBookSongs == kNumDemoSongs, "kNumDemoSongs must match the song book");

const char* const kClassicNames[kNumClassicSongs] = { "Pixel Sunrise", "Greensleeves", "Neon Drive", "Pocket Adventure" };
const int kClassicGroups[kNumClassicSongs] = { kDemoRetro, kDemoMedieval, kDemoRetro, kDemoRetro };

// =============================================================================
// The original four songs
// =============================================================================

using NoteList = std::initializer_list<std::pair<int, int>>;

void putNotes(std::vector<int8_t>& track, NoteList notes) {
    for (const auto& n : notes) {
        for (int i = 0; i < n.second; ++i) {
            if (n.first < 0) track.push_back(-1);
            else track.push_back(static_cast<int8_t>(i == 0 ? n.first : -2));
        }
    }
}

void buildClassicSongs(std::vector<DemoSong>& songs) {
    // 0: "Pixel Sunrise" (original), 128 BPM 80s console theme in D minor: Dm Bb C A
    {
        DemoSong& s = songs[0];
        s.stepSec = 60.0 / 128.0 / 4.0;
        s.numSteps = 256; // 16 bars of 16th notes
        static const int kBassRoot[4] = { 50, 46, 48, 45 };          // D3 Bb2 C3 A2
        static const int kArp[4][4] = { {62,65,69,74}, {58,62,65,70}, {60,64,67,72}, {57,61,64,69} };
        static const int kArpOrder[8] = { 0, 1, 2, 3, 1, 2, 3, 2 };
        static const int kPad[4] = { 65, 62, 64, 61 };                // chord thirds
        auto& lead = s.tracks[0];
        auto& bass = s.tracks[1];
        auto& arp = s.tracks[2];
        auto& pad = s.tracks[3];
        auto& thump = s.tracks[4];
        for (int bar = 0; bar < 16; ++bar) {
            const int ch = bar % 4;
            for (int i = 0; i < 16; ++i) {
                // Octave-bouncing eighth-note bass
                if (i & 1) bass.push_back(-2);
                else bass.push_back(static_cast<int8_t>(kBassRoot[ch] + (((i >> 1) & 1) ? 12 : 0)));
                arp.push_back(static_cast<int8_t>(kArp[ch][kArpOrder[i & 7]]));
                thump.push_back((i == 0 || i == 8) ? 38 : -1);
            }
            putNotes(pad, { {kPad[ch], 16} });
        }
        putNotes(lead, { {-1,64} }); // 4-bar intro
        // Phrase A
        const NoteList a1 = { {74,4},{76,2},{77,4},{76,2},{74,2},{72,2} };
        const NoteList a2 = { {70,6},{74,2},{77,6},{79,2} };
        const NoteList a3 = { {79,4},{77,2},{76,4},{72,2},{76,4} };
        putNotes(lead, a1); putNotes(lead, a2); putNotes(lead, a3);
        putNotes(lead, { {73,6},{76,2},{81,8} });
        // Phrase B
        putNotes(lead, { {81,4},{79,2},{77,4},{76,2},{77,2},{79,2} });
        putNotes(lead, { {77,6},{74,2},{70,4},{74,4} });
        putNotes(lead, { {76,2},{77,2},{79,4},{84,4},{79,4} });
        putNotes(lead, { {81,12},{-1,4} });
        // Phrase A again, resolving home to D
        putNotes(lead, a1); putNotes(lead, a2); putNotes(lead, a3);
        putNotes(lead, { {73,4},{69,4},{74,8} });
        lead.resize(s.numSteps, -1);
        s.velocity[0] = 0.90f; s.velocity[1] = 0.74f; s.velocity[2] = 0.30f;
        s.velocity[3] = 0.26f; s.velocity[4] = 0.50f;
    }

    // 1: "Greensleeves" (traditional, 16th century), 6/8 in 16th steps
    {
        DemoSong& s = songs[1];
        s.stepSec = 0.140;
        auto& lead = s.tracks[0];
        const NoteList m1 = { {72,4},{74,2},{76,3},{77,1},{76,2} };
        const NoteList m2 = { {74,4},{71,2},{67,3},{69,1},{71,2} };
        const NoteList m3 = { {72,4},{69,2},{69,3},{68,1},{69,2} };
        const NoteList m7 = { {72,3},{71,1},{69,2},{68,3},{66,1},{68,2} };
        const NoteList m9 = { {79,6},{79,3},{78,1},{76,2} };
        putNotes(lead, m1); putNotes(lead, m2); putNotes(lead, m3);
        putNotes(lead, { {71,4},{68,2},{64,4},{69,2} });
        putNotes(lead, m1); putNotes(lead, m2); putNotes(lead, m7);
        putNotes(lead, { {69,6},{69,4},{-1,2} });
        putNotes(lead, m9); putNotes(lead, m2); putNotes(lead, m3);
        putNotes(lead, { {71,4},{68,2},{64,6} });
        putNotes(lead, m9); putNotes(lead, m2); putNotes(lead, m7);
        putNotes(lead, { {69,6},{69,6} });
        s.numSteps = static_cast<int>(lead.size());

        // Lute-style accompaniment: root, fifth, octave on each half bar
        enum { Am, G, C, E };
        static const int kRoots[4][3] = { {45,52,57}, {43,50,55}, {48,55,60}, {40,47,52} };
        static const int kHalfBars[32] = {
            Am,Am, G,G, Am,Am, E,E,  Am,Am, G,G, Am,E, Am,Am,
            C,C,   G,G, Am,Am, E,E,  C,C,   G,G, Am,E, Am,Am
        };
        auto& bass = s.tracks[1];
        for (int h = 0; h < 32; ++h) {
            const int* r = kRoots[kHalfBars[h]];
            putNotes(bass, { {r[0],2},{r[1],2},{r[2],2} });
        }
        bass.resize(s.numSteps, -1);
        for (int t = 2; t < kDemoTracks; ++t) s.tracks[t].assign(s.numSteps, -1);
        s.velocity[0] = 0.90f; s.velocity[1] = 0.55f;
    }

    // 2: "Neon Drive" (original), 110 BPM synth groove over Am F C G
    {
        DemoSong& s = songs[2];
        s.stepSec = 60.0 / 110.0 / 4.0;
        s.numSteps = 256;
        static const int kBassRoot[4] = { 45, 41, 48, 43 };
        static const int kArp[4][4] = { {57,60,64,69}, {57,60,65,69}, {55,60,64,67}, {55,59,62,67} };
        static const int kArpOrder[8] = { 0, 1, 2, 3, 2, 1, 2, 3 };
        auto& lead = s.tracks[0];
        auto& bass = s.tracks[1];
        auto& arp = s.tracks[2];
        for (int bar = 0; bar < 16; ++bar) {
            const int ch = bar % 4;
            for (int i = 0; i < 16; ++i) {
                if (i & 1) bass.push_back(-2);
                else bass.push_back(static_cast<int8_t>(kBassRoot[ch] + ((i == 4 || i == 12) ? 12 : 0)));
                arp.push_back(static_cast<int8_t>(kArp[ch][kArpOrder[i & 7]]));
            }
        }
        putNotes(lead, { {-1,64} });
        const NoteList a1 = { {76,6},{74,2},{72,8} };
        const NoteList a2 = { {69,6},{72,2},{77,8} };
        const NoteList a3 = { {76,4},{79,4},{76,4},{74,4} };
        const NoteList a4 = { {74,8},{71,4},{74,4} };
        putNotes(lead, a1); putNotes(lead, a2); putNotes(lead, a3); putNotes(lead, a4);
        putNotes(lead, { {81,6},{79,2},{76,8} });
        putNotes(lead, { {77,6},{76,2},{72,8} });
        putNotes(lead, { {79,4},{76,4},{72,4},{76,4} });
        putNotes(lead, { {74,12},{-1,4} });
        putNotes(lead, a1); putNotes(lead, a2); putNotes(lead, a3); putNotes(lead, a4);
        lead.resize(s.numSteps, -1);
        s.tracks[3].assign(s.numSteps, -1);
        s.tracks[4].assign(s.numSteps, -1);
        s.velocity[0] = 0.92f; s.velocity[1] = 0.78f; s.velocity[2] = 0.36f;
    }

    // 3: "Pocket Adventure" (original), 140 BPM handheld-style overworld tune in C
    {
        DemoSong& s = songs[3];
        s.stepSec = 60.0 / 140.0 / 4.0;
        auto& lead = s.tracks[0];
        auto& bass = s.tracks[1];
        // chord per beat: 0 C, 1 G, 2 Am, 3 F
        static const int kRoot[4] = { 48, 43, 45, 41 };
        static const int kBeats[32] = {
            0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,1,1,
            0,0,0,0, 1,1,1,1, 3,3,3,3, 1,1,0,0
        };
        for (int rep = 0; rep < 2; ++rep) {
            putNotes(lead, { {76,2},{79,2},{84,4},{83,2},{79,2},{76,4} });
            putNotes(lead, { {74,2},{79,2},{83,4},{81,2},{79,2},{74,4} });
            putNotes(lead, { {72,2},{76,2},{81,4},{79,2},{76,2},{72,4} });
            putNotes(lead, { {77,4},{81,4},{79,4},{83,4} });
            putNotes(lead, { {84,6},{79,2},{76,4},{79,4} });
            putNotes(lead, { {83,6},{79,2},{74,4},{79,4} });
            putNotes(lead, { {81,2},{79,2},{77,2},{76,2},{77,4},{81,4} });
            putNotes(lead, { {79,4},{83,4},{84,8} });
            for (int b = 0; b < 32; ++b) {
                const int r = kRoot[kBeats[b]];
                putNotes(bass, { {r,2},{r + 7,2} });
            }
        }
        s.numSteps = static_cast<int>(lead.size());
        bass.resize(s.numSteps, -1);
        for (int t = 2; t < kDemoTracks; ++t) s.tracks[t].assign(s.numSteps, -1);
        s.velocity[0] = 0.92f; s.velocity[1] = 0.70f;
    }
}

// =============================================================================
// Parsing
// =============================================================================

std::vector<std::string> g_issues;

void issue(const SongDef& d, const char* fmt, const char* a = "", int n = 0) {
    char msg[256];
    char detail[160];
    std::snprintf(detail, sizeof(detail), fmt, a, n);
    std::snprintf(msg, sizeof(msg), "%s: %s", d.name, detail);
    g_issues.push_back(msg);
}

struct Chord {
    int root = 0;
    int n = 3;
    int iv[6] = { 0, 4, 7, 0, 0, 0 };
};

struct Quality { const char* name; int n; int iv[6]; };
const Quality kQualities[] = {
    { "",      3, { 0, 4, 7 } },
    { "m",     3, { 0, 3, 7 } },
    { "5",     2, { 0, 7 } },
    { "6",     4, { 0, 4, 7, 9 } },
    { "m6",    4, { 0, 3, 7, 9 } },
    { "7",     4, { 0, 4, 7, 10 } },
    { "m7",    4, { 0, 3, 7, 10 } },
    { "maj7",  4, { 0, 4, 7, 11 } },
    { "9",     5, { 0, 4, 7, 10, 14 } },
    { "m9",    5, { 0, 3, 7, 10, 14 } },
    { "maj9",  5, { 0, 4, 7, 11, 14 } },
    { "add9",  4, { 0, 4, 7, 14 } },
    { "madd9", 4, { 0, 3, 7, 14 } },
    { "sus2",  3, { 0, 2, 7 } },
    { "sus4",  3, { 0, 5, 7 } },
    { "7sus4", 4, { 0, 5, 7, 10 } },
    { "dim",   3, { 0, 3, 6 } },
    { "m7b5",  4, { 0, 3, 6, 10 } },
};

int letterPc(char c) {
    switch (c) {
        case 'c': case 'C': return 0;
        case 'd': case 'D': return 2;
        case 'e': case 'E': return 4;
        case 'f': case 'F': return 5;
        case 'g': case 'G': return 7;
        case 'a': case 'A': return 9;
        case 'b': case 'B': return 11;
        default: return -1;
    }
}

// Splits on spaces; '|' is returned as its own token.
std::vector<std::string> tokens(const char* s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = s ? s : ""; ; ++p) {
        const char ch = *p;
        if (ch == '\0' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '|') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            if (ch == '|') out.push_back("|");
            if (ch == '\0') break;
        } else {
            cur += ch;
        }
    }
    return out;
}

// "Bbmaj7:4" -> chord and beats
bool parseChord(const std::string& tok, Chord& c, int& beats) {
    const size_t colon = tok.find(':');
    if (colon == std::string::npos || colon == 0) return false;
    beats = std::atoi(tok.c_str() + colon + 1);
    if (beats <= 0) return false;
    int pc = letterPc(tok[0]);
    if (pc < 0) return false;
    size_t i = 1;
    if (i < colon && tok[i] == '#') { ++pc; ++i; }
    else if (i < colon && tok[i] == 'b') { --pc; ++i; }
    const std::string q = tok.substr(i, colon - i);
    for (const Quality& k : kQualities) {
        if (q == k.name) {
            c.root = (pc + 12) % 12;
            c.n = k.n;
            for (int j = 0; j < 6; ++j) c.iv[j] = k.iv[j];
            return true;
        }
    }
    return false;
}

// "f#5:2" -> MIDI note (60 = c4); "r" -> -1 rest; "_" -> -2 hold. Returns false if malformed.
bool parseNote(const std::string& tok, int& note, int& len, int prevLen) {
    const size_t colon = tok.find(':');
    const std::string head = tok.substr(0, colon);
    len = (colon == std::string::npos) ? prevLen : std::atoi(tok.c_str() + colon + 1);
    if (len <= 0 || head.empty()) return false;
    if (head == "r") { note = -1; return true; }
    if (head == "_") { note = -2; return true; }
    int pc = letterPc(head[0]);
    if (pc < 0 || head[0] < 'a') return false;
    size_t i = 1;
    if (i < head.size() && head[i] == '#') { ++pc; ++i; }
    else if (i < head.size() && head[i] == 'b' && i + 1 < head.size()) { --pc; ++i; }
    if (i >= head.size()) return false;
    const int octave = std::atoi(head.c_str() + i);
    note = (octave + 1) * 12 + pc;
    return note >= 12 && note <= 120;
}

struct Key {
    int tonic = 0;
    bool pcs[12] = {};
};

Key parseKey(const SongDef& d) {
    Key k;
    struct Mode { const char* name; int steps[7]; };
    static const Mode kModes[] = {
        { "ionian",     { 0, 2, 4, 5, 7, 9, 11 } },
        { "dorian",     { 0, 2, 3, 5, 7, 9, 10 } },
        { "phrygian",   { 0, 1, 3, 5, 7, 8, 10 } },
        { "lydian",     { 0, 2, 4, 6, 7, 9, 11 } },
        { "mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
        { "aeolian",    { 0, 2, 3, 5, 7, 8, 10 } },
    };
    const std::vector<std::string> t = tokens(d.key);
    int pc = t.empty() ? -1 : letterPc(t[0][0]);
    if (pc < 0) { issue(d, "bad key '%s'", d.key); pc = 0; }
    if (t.size() > 0 && t[0].size() > 1) pc += (t[0][1] == '#') ? 1 : (t[0][1] == 'b' ? -1 : 0);
    k.tonic = (pc + 12) % 12;
    const Mode* mode = &kModes[0];
    bool found = false;
    for (const Mode& m : kModes) {
        if (t.size() > 1 && t[1] == m.name) { mode = &m; found = true; }
    }
    if (!found) issue(d, "unknown mode in key '%s'", d.key);
    for (int s : mode->steps) k.pcs[(k.tonic + s) % 12] = true;
    return k;
}

// Whole-song timeline: melody plus the chord and part under every step.
struct Timeline {
    std::vector<int8_t> melody;
    std::vector<Chord> chords;
    std::vector<int> chordAt, chordStart, chordEnd;
    std::vector<int> partStart, partEnd;
};

void appendPart(const SongDef& d, const PartDef& part, Timeline& tl) {
    const int barSteps = d.beatSteps * d.barBeats;
    const int start = static_cast<int>(tl.melody.size());

    // Chords
    int len = 0;
    for (const std::string& tok : tokens(part.chords)) {
        if (tok == "|") continue;
        Chord c;
        int beats = 0;
        if (!parseChord(tok, c, beats)) { issue(d, "bad chord '%s'", tok.c_str()); continue; }
        const int steps = beats * d.beatSteps;
        const int idx = static_cast<int>(tl.chords.size());
        tl.chords.push_back(c);
        for (int i = 0; i < steps; ++i) {
            tl.chordAt.push_back(idx);
            tl.chordStart.push_back(start + len);
            tl.chordEnd.push_back(start + len + steps);
        }
        len += steps;
    }
    if (len == 0) { issue(d, "part %s has no chords", part.id); return; }
    if (len % barSteps != 0) issue(d, "part %s is not a whole number of bars (%d steps)", part.id, len);

    // Melody
    std::vector<int8_t> mel;
    int prevLen = 4;
    for (const std::string& tok : tokens(part.melody)) {
        if (tok == "|") {
            if (static_cast<int>(mel.size()) % barSteps != 0) {
                char what[64];
                std::snprintf(what, sizeof(what), "%s", part.id);
                issue(d, "bar line off the beat in part %s at step %d", what, static_cast<int>(mel.size()));
            }
            continue;
        }
        int note = 0, n = 0;
        if (!parseNote(tok, note, n, prevLen)) { issue(d, "bad note '%s'", tok.c_str()); continue; }
        prevLen = n;
        for (int i = 0; i < n; ++i) {
            if (note == -2) mel.push_back(-2);
            else if (note == -1) mel.push_back(-1);
            else mel.push_back(static_cast<int8_t>(i == 0 ? note : -2));
        }
    }
    if (static_cast<int>(mel.size()) != len) {
        char what[64];
        std::snprintf(what, sizeof(what), "%s", part.id);
        issue(d, "melody of part %s is %d steps off its chords", what, static_cast<int>(mel.size()) - len);
    }
    mel.resize(len, -1);
    if (!mel.empty() && mel[0] == -2) mel[0] = -1; // a hold cannot start a part
    tl.melody.insert(tl.melody.end(), mel.begin(), mel.end());
    for (int i = 0; i < len; ++i) {
        tl.partStart.push_back(start);
        tl.partEnd.push_back(start + len);
    }
}

// =============================================================================
// Accompaniment
// =============================================================================

// Writes a note held for len steps; ends an older note that was held past it.
void put(std::vector<int8_t>& t, int s, int note, int len) {
    const int n = static_cast<int>(t.size());
    if (s < 0 || s >= n || len <= 0 || note < 0 || note > 127) return;
    t[s] = static_cast<int8_t>(note);
    const int e = std::min(n, s + len);
    for (int i = s + 1; i < e; ++i) t[i] = -2;
    if (e < n && t[e] == -2) t[e] = -1;
}

void shiftCopy(const std::vector<int8_t>& src, std::vector<int8_t>& dst, int delay) {
    const int n = static_cast<int>(src.size());
    dst.assign(n, -1);
    for (int i = delay; i < n; ++i) dst[i] = src[i - delay];
    if (delay < n && dst[delay] == -2) dst[delay] = -1;
}

// Lowest note at or above lo with pitch class pc.
int place(int pc, int lo) {
    pc = ((pc % 12) + 12) % 12;
    return lo + ((pc - lo) % 12 + 12) % 12;
}

int third(const Chord& c) { return c.n > 1 ? c.iv[1] : 7; }
int fifth(const Chord& c) { return c.n > 2 ? c.iv[2] : (c.n > 1 ? c.iv[1] : 7); }
int seventh(const Chord& c) {
    for (int i = 0; i < c.n; ++i) if (c.iv[i] == 10 || c.iv[i] == 11) return c.iv[i];
    return -1;
}
bool hasNinth(const Chord& c) {
    for (int i = 0; i < c.n; ++i) if (c.iv[i] == 14) return true;
    return false;
}
// The chord's colour note: 7th or 6th, else the 9th, else the octave.
int colour(const Chord& c) {
    for (int i = 3; i < c.n; ++i) if (c.iv[i] < 12) return c.iv[i];
    if (hasNinth(c)) return 14;
    return 12;
}
// Chip arpeggio tones: root, third, fifth, then the 7th or the octave
int tone(const Chord& c, int k) {
    switch (k & 3) {
        case 0:  return 0;
        case 1:  return third(c);
        case 2:  return fifth(c);
        default: { const int s = seventh(c); return s >= 0 ? s : 12; }
    }
}
bool chordHas(const Chord& c, int pc) {
    for (int i = 0; i < c.n; ++i) if ((c.root + c.iv[i]) % 12 == ((pc % 12) + 12) % 12) return true;
    return false;
}

// A third (or failing that a fourth) below the melody note, preferring chord tones, then the mode.
int harmonyBelow(int m, const Chord& c, const Key& k) {
    for (int d : { 3, 4 }) if (chordHas(c, m - d)) return m - d;
    for (int d : { 3, 4 }) if (k.pcs[(m - d + 120) % 12]) return m - d;
    return m - 5;
}

// Parallel fourth below, else fifth, as in early organum; stays inside the mode.
int organumBelow(int m, const Key& k) {
    if (k.pcs[(m - 5 + 120) % 12]) return m - 5;
    if (k.pcs[(m - 7 + 120) % 12]) return m - 7;
    return m - 3;
}

// The pitch each step of a track leaves sounding (-1 = silent).
std::vector<int> sounding(const std::vector<int8_t>& t) {
    std::vector<int> out(t.size(), -1);
    int cur = -1;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] >= 0) cur = t[i];
        else if (t[i] == -1) cur = -1;
        out[i] = cur;
    }
    return out;
}

// The engine gives one voice to each pitch, and a note-off releases every voice of that
// pitch. Two tracks on the same pitch would cut or re-strike each other, so a clashing
// accompaniment note moves an octave (the echo track only moves down, as an octave echo).
void avoidUnisons(DemoSong& s, bool echoTrack) {
    const int n = s.numSteps;
    std::vector<std::vector<int>> fixedMaps;
    fixedMaps.push_back(sounding(s.tracks[0]));
    static const int kOrder[4] = { 1, 3, 2, 4 };
    for (int t : kOrder) {
        auto& tr = s.tracks[t];
        const bool echo = echoTrack && t == 4;
        for (int i = 0; i < n; ++i) {
            if (tr[i] < 0) continue;
            int len = 1;
            while (i + len < n && tr[i + len] == -2) ++len;
            // Also guard the step before and after: a note-off landing on our start or end would cut us
            auto clashes = [&](int p) {
                for (const auto& m : fixedMaps) {
                    for (int k = std::max(0, i - 1); k <= std::min(n - 1, i + len); ++k) {
                        if (m[k] == p) return true;
                    }
                }
                return false;
            };
            const int p = tr[i];
            if (clashes(p)) {
                int alt = -1;
                if (p - 12 >= 36 && !clashes(p - 12)) alt = p - 12;
                else if (!echo && p + 12 <= 100 && !clashes(p + 12)) alt = p + 12;
                if (alt >= 0) {
                    tr[i] = static_cast<int8_t>(alt);
                } else {
                    for (int k = i; k < i + len; ++k) tr[k] = -1;
                }
            }
            i += len - 1;
        }
        fixedMaps.push_back(sounding(tr));
    }
}

template <typename F>
void followMelody(const std::vector<int8_t>& mel, std::vector<int8_t>& out, F voiceFor) {
    const int n = static_cast<int>(mel.size());
    for (int i = 0; i < n; ++i) {
        if (mel[i] < 0) continue;
        int len = 1;
        while (i + len < n && mel[i + len] == -2) ++len;
        put(out, i, voiceFor(i, mel[i]), len);
    }
}

void generate(const SongDef& d, const Timeline& tl, const Key& key, DemoSong& s) {
    const int n = s.numSteps;
    const int S = d.beatSteps;
    const int bar = d.beatSteps * d.barBeats;
    auto& mel = s.tracks[0];
    auto& bass = s.tracks[1];
    auto& t2 = s.tracks[2];
    auto& t3 = s.tracks[3];
    auto& t4 = s.tracks[4];
    for (int t = 1; t < kDemoTracks; ++t) s.tracks[t].assign(n, -1);

    auto chordOf = [&](int i) -> const Chord& { return tl.chords[tl.chordAt[i]]; };
    auto rel = [&](int i) { return i - tl.partStart[i]; };
    auto chordStarts = [&](int i) { return tl.chordStart[i] == i; };
    auto partStarts = [&](int i) { return tl.partStart[i] == i; };
    auto toBarEnd = [&](int i) { return std::min(tl.chordEnd[i], i + bar - rel(i) % bar) - i; };

    auto thirdsBelow = [&]() {
        followMelody(mel, t2, [&](int i, int m) { return harmonyBelow(m, chordOf(i), key); });
    };
    auto drone = [&](std::vector<int8_t>& t, int pc, int lo) {
        for (int i = 0; i < n; ++i) if (partStarts(i)) put(t, i, place(pc, lo), tl.partEnd[i] - i);
    };
    // A high chord tone now and then (ambient styles)
    auto sparkle = [&](int i, int r, int everyBars, int atStep, int lenSteps) {
        const int barIdx = rel(i) / bar;
        if (r == atStep && barIdx % everyBars == everyBars - 1) {
            const Chord& c = chordOf(i);
            put(t4, i, place(c.root + ((barIdx / everyBars) % 2 ? fifth(c) : 0), 84), lenSteps);
        }
    };

    for (int i = 0; i < n; ++i) {
        const Chord& c = chordOf(i);
        const int r = rel(i) % bar;
        const int b = r % S;
        switch (d.style) {
            case kStChipDrive: {
                static const int kOrd[8] = { 0, 1, 2, 3, 1, 2, 3, 2 };
                if (r % 2 == 0) put(bass, i, place(c.root, 36) + (((r / 2) & 1) ? 12 : 0), 2);
                put(t2, i, place(c.root, 57) + tone(c, kOrd[r % 8]), 1);
                if (chordStarts(i)) put(t3, i, place(c.root + third(c), 64), tl.chordEnd[i] - i);
                break;
            }
            case kStChipMarch: {
                if (b == 0) {
                    int note = place(c.root, 40) + (((r / S) % 2) ? fifth(c) : 0);
                    if (note > 55) note -= 12;
                    put(bass, i, note, S - 1);
                } else if (b == S / 2) {
                    put(t2, i, place(c.root, 60) + (((r / S) % 2) ? fifth(c) : third(c)), S / 2);
                }
                break;
            }
            case kStChipBallad: {
                static const int kOrd[8] = { 0, 2, 1, 2, 3, 2, 1, 2 };
                const int half = (d.barBeats % 2 == 0) ? bar / 2 : bar;
                if (r % half == 0) put(bass, i, place(c.root, 36) + ((r == 0 || chordStarts(i)) ? 0 : fifth(c)), half);
                if (r % 2 == 0) put(t2, i, place(c.root, 55) + tone(c, kOrd[(r / 2) % 8]), 2);
                if (chordStarts(i)) put(t3, i, place(c.root + third(c), 64), tl.chordEnd[i] - i);
                break;
            }
            case kStChipGallop: {
                if (b == 0) put(bass, i, place(c.root, 36), 2);
                else if (b == 2) put(bass, i, place(c.root, 36), 1);
                else if (b == 3) put(bass, i, place(c.root, 36) + 12, 1);
                put(t2, i, place(c.root, 62) + tone(c, r % 4), 1);
                if (chordStarts(i)) put(t3, i, place(c.root + fifth(c), 67), tl.chordEnd[i] - i);
                break;
            }
            case kStChipWaltz: {
                if (b == 0) {
                    if (r == 0) put(bass, i, place(c.root, 40), S);
                    else {
                        put(t2, i, place(c.root + third(c), 60), S - 1);
                        put(t3, i, place(c.root + fifth(c), 64), S - 1);
                    }
                }
                break;
            }
            case kStEstampie:
            case kStRound: {
                if (r % 2 == 0) {
                    const int offs[4] = { 0, fifth(c), 12, fifth(c) };
                    put(bass, i, place(c.root, 43) + offs[(r / 2) % 4], 2);
                }
                break;
            }
            case kStLute: {
                if (r % 2 == 0) {
                    const int offs[6] = { 0, fifth(c), 12, 12 + third(c), 12, fifth(c) };
                    put(bass, i, place(c.root, 43) + offs[(r / 2) % 6], 2);
                }
                break;
            }
            case kStJig: {
                if (r % 2 == 0) {
                    const int offs[3] = { 0, fifth(c), 12 };
                    put(bass, i, place(c.root, 43) + offs[(b / 2) % 3], 2);
                }
                break;
            }
            case kStProcession: {
                if (b == 0) put(bass, i, place(c.root, 38), S - 1);
                break;
            }
            case kStChant:
                break;
            case kStPiano: {
                if (r == 0 || chordStarts(i)) put(bass, i, place(c.root, 36), toBarEnd(i));
                if (r % 2 == 0) {
                    const int sev = seventh(c);
                    const int top = sev >= 0 ? sev : 12;
                    const int tenth = hasNinth(c) ? 14 : 12 + third(c);
                    int off = -1;
                    if (d.barBeats == 3) {
                        const int fig[6] = { -1, fifth(c), top, 12 + third(c), top, fifth(c) };
                        off = fig[(r / 2) % 6];
                    } else {
                        const int fig[8] = { -1, fifth(c), top, 12 + third(c), 12 + fifth(c), tenth, 12, fifth(c) };
                        off = fig[(r / 2) % 8];
                    }
                    if (off >= 0) put(t2, i, place(c.root, 48) + off, 2);
                }
                if (chordStarts(i)) {
                    const int sev = seventh(c);
                    put(t3, i, place(c.root + (sev >= 0 ? sev : third(c)), 62), tl.chordEnd[i] - i);
                }
                sparkle(i, r, 2, d.barBeats == 3 ? S : 2 * S, 2 * S);
                break;
            }
            case kStDrift: {
                const bool restrike = chordStarts(i) || (r == 0 && (i - tl.chordStart[i]) % (2 * bar) == 0);
                if (restrike) {
                    const int len = std::min(tl.chordEnd[i], i + 2 * bar) - i;
                    put(bass, i, place(c.root, 36), len);
                    put(t2, i, place(c.root + third(c), 55), len);
                    put(t3, i, place(c.root + colour(c), 62), len);
                }
                sparkle(i, r, 4, S, 3 * S);
                break;
            }
            case kStMusicBox: {
                const int half = (d.barBeats % 2 == 0) ? bar / 2 : bar;
                if (r % half == 0) put(bass, i, place(c.root, 43) + ((r == 0 || chordStarts(i)) ? 0 : fifth(c)), half);
                if (r % 2 == 0) {
                    const int fig[8] = { 0, fifth(c), 12, 12 + third(c), 12 + fifth(c), 12 + third(c), 12, fifth(c) };
                    put(t2, i, place(c.root, 60) + fig[(r / 2) % 8], 2);
                }
                break;
            }
            case kStPulse: {
                if (r == 0 || chordStarts(i)) put(bass, i, place(c.root, 36), toBarEnd(i));
                if (r % 2 == 0) put(t2, i, place(c.root + fifth(c), 60), 2);
                if (chordStarts(i)) put(t3, i, place(c.root + third(c), 64), tl.chordEnd[i] - i);
                sparkle(i, r, 4, S, 3 * S);
                break;
            }
            case kStWaltzAmb: {
                if (b == 0) {
                    if (r == 0) put(bass, i, place(c.root, 40), S);
                    else {
                        const int sev = seventh(c);
                        put(t2, i, place(c.root + third(c), 55), S);
                        put(t3, i, place(c.root + (sev >= 0 ? sev : fifth(c)), 60), S);
                    }
                }
                sparkle(i, r, 4, 0, bar);
                break;
            }
            default:
                break;
        }
    }

    // Voices that follow the melody or the key
    switch (d.style) {
        case kStChipDrive: case kStChipMarch: case kStChipGallop:
            shiftCopy(mel, t4, 3);
            break;
        case kStChipBallad:
            shiftCopy(mel, t4, 4);
            break;
        case kStChipWaltz:
            shiftCopy(mel, t4, 2);
            break;
        case kStEstampie: case kStLute: case kStJig:
            thirdsBelow();
            drone(t3, key.tonic, 31); // under the lute
            break;
        case kStProcession:
            thirdsBelow();
            drone(t3, key.tonic, 50); // between the bass and the tune
            break;
        case kStChant:
            followMelody(mel, t2, [&](int, int m) { return organumBelow(m, key); });
            drone(bass, key.tonic, 36);
            drone(t3, key.tonic + 7, 43);
            break;
        case kStRound: {
            const int delay = std::max(1, d.canonBars) * bar;
            shiftCopy(mel, t2, delay);
            shiftCopy(mel, t4, 2 * delay);
            drone(t3, key.tonic, 31);
            break;
        }
        default:
            break;
    }
    avoidUnisons(s, d.style <= kStChipWaltz);
}

void buildBookSong(const SongDef& d, DemoSong& s) {
    s.name = d.name;
    s.group = d.group;
    s.stepSec = 60.0 / std::max(20, d.bpm) / std::max(1, d.beatSteps);
    for (int t = 0; t < kDemoTracks; ++t) s.velocity[t] = d.vel[t];

    const Key key = parseKey(d);
    Timeline tl;
    for (const std::string& id : tokens(d.form)) {
        const PartDef* part = nullptr;
        for (const PartDef& p : d.parts) {
            if (p.id && id == p.id) part = &p;
        }
        if (!part) { issue(d, "form names a missing part '%s'", id.c_str()); continue; }
        appendPart(d, *part, tl);
    }
    s.numSteps = static_cast<int>(tl.melody.size());
    if (s.numSteps == 0) {
        issue(d, "song is empty%s");
        s.numSteps = 16;
        for (auto& t : s.tracks) t.assign(16, -1);
        return;
    }
    s.tracks[0] = tl.melody;
    generate(d, tl, key, s);
}

std::vector<DemoSong> buildAll() {
    g_issues.clear();
    std::vector<DemoSong> songs(kNumDemoSongs);
    buildClassicSongs(songs);
    for (int i = 0; i < kNumClassicSongs; ++i) {
        songs[i].name = kClassicNames[i];
        songs[i].group = kClassicGroups[i];
    }
    for (int i = 0; i < kNumBookSongs; ++i) buildBookSong(kSongBook[i], songs[kNumClassicSongs + i]);
    for (DemoSong& s : songs) {
        for (auto& t : s.tracks) t.resize(s.numSteps, -1);
    }
    return songs;
}

} // namespace

const std::vector<DemoSong>& demoSongs() {
    static const std::vector<DemoSong> songs = buildAll();
    return songs;
}

std::vector<std::string> demoSongIssues() {
    demoSongs();
    return g_issues;
}

const char* demoSongName(int index) {
    index = std::clamp(index, 0, kNumDemoSongs - 1);
    return index < kNumClassicSongs ? kClassicNames[index] : kSongBook[index - kNumClassicSongs].name;
}

int demoSongGroup(int index) {
    index = std::clamp(index, 0, kNumDemoSongs - 1);
    return index < kNumClassicSongs ? kClassicGroups[index] : kSongBook[index - kNumClassicSongs].group;
}

const char* demoGroupName(int group) {
    static const char* kNames[kNumDemoGroups] = { "RETRO", "MEDIEVAL", "AMBIENT" };
    return kNames[std::clamp(group, 0, kNumDemoGroups - 1)];
}

const char* demoGroupHint(int group) {
    static const char* kHints[kNumDemoGroups] = {
        "Try a chip lead: Cascade Lead, Pulse Hero Lead",
        "Try Minstrel Lute, Gothic Harp or Tavern Recorder",
        "Try Felt Room Keys or Crystal Cave Keys"
    };
    return kHints[std::clamp(group, 0, kNumDemoGroups - 1)];
}

int demoSongForEra(int era) {
    switch (era) {
        case kEraChip90:   return 3;
        case kEraSynth:    return 2;
        case kEraMedieval: return 1;
        default:           return 0;
    }
}

int demoGroupSongs(int group, int* out, int maxOut) {
    static const int kClassicOrder[kNumClassicSongs] = { 0, 3, 2, 1 };
    int count = 0;
    for (int idx : kClassicOrder) {
        if (kClassicGroups[idx] == group && count < maxOut) out[count++] = idx;
    }
    for (int i = 0; i < kNumBookSongs; ++i) {
        if (kSongBook[i].group == group && count < maxOut) out[count++] = kNumClassicSongs + i;
    }
    return count;
}

} // namespace PixelLabs
