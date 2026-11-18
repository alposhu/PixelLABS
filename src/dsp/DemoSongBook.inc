// Demo song book: 30 original songs (5 retro, 10 medieval, 15 ambient).
// Included by DemoSongs.cpp; the format is described there. Song order is part of
// the song index, so add new songs at the end of their group's run only if
// kNumDemoSongs and the picker order are updated with them.

const SongDef kSongBook[] = {

// ============================================================================
// RETRO
// ============================================================================

{ "Crystal Caverns", kDemoRetro, 112, 4, 4, kStChipBallad, "A aeolian", 0, "I A B A",
  { 0.90f, 0.70f, 0.30f, 0.24f, 0.32f },
  { { "I", "Am:4 F:4",
           "r:16 | r:16" },
    { "A", "Am:4 F:4 G:4 Em:4 Am:4 F:4 Dm:4 E:4",
           "e5:4 a5:4 c6:4 b5:2 a5:2 | c6:6 a5:2 f5:8 | g5:4 b5:4 d6:6 b5:2 | b5:8 g5:4 e5:4 |"
           "a5:4 c6:2 e6:6 c6:4 | d6:4 c6:4 a5:4 f5:4 | f5:4 a5:4 d6:4 c6:4 | b5:8 g#5:4 e5:4" },
    { "B", "F:4 G:4 Em:4 Am:4 Dm:4 E:4 Am:8",
           "a5:6 c6:2 f6:8 | d6:4 b5:4 g5:8 | e6:6 d6:2 b5:4 g5:4 | a5:12 r:4 |"
           "f5:4 a5:4 d6:4 f6:4 | e6:4 d6:2 c6:2 b5:4 g#5:4 | c6:4 b5:4 a5:8 | e5:4 a5:12" } } },

{ "Boss Rush", kDemoRetro, 156, 4, 4, kStChipGallop, "D aeolian", 0, "I A A B A",
  { 0.92f, 0.74f, 0.30f, 0.22f, 0.30f },
  { { "I", "Dm:4 Dm:4",
           "r:16 | r:16" },
    { "A", "Dm:4 Bb:4 C:4 A:4 Dm:4 Bb:4 Gm:4 A:4",
           "d5:2 d5:2 f5:2 a5:2 d6:4 c6:2 a5:2 | bb5:4 a5:2 f5:2 d5:4 f5:4 | e5:2 g5:2 c6:2 e6:2 g6:4 e6:4 | c#6:4 e6:4 a5:4 r:4 |"
           "d5:2 d5:2 f5:2 a5:2 d6:4 c6:2 a5:2 | d6:4 c6:2 bb5:2 f5:4 bb5:4 | g5:2 bb5:2 d6:2 g6:2 f6:4 d6:4 | e6:4 c#6:4 a5:8" },
    { "B", "Gm:4 Dm:4 Bb:4 A:4 Gm:4 Dm:4 E:4 A:4",
           "bb5:6 a5:2 g5:4 d5:4 | f5:6 e5:2 d5:4 a4:4 | d5:2 f5:2 bb5:2 d6:2 f6:8 | e6:2 c#6:2 a5:2 e5:2 c#5:8 |"
           "g5:2 bb5:2 d6:2 g6:2 bb6:8 | a6:4 f6:4 d6:8 | g#5:4 b5:4 e6:4 d6:4 | c#6:8 a5:4 e5:4" } } },

{ "Starfield Run", kDemoRetro, 140, 4, 4, kStChipDrive, "E aeolian", 0, "I A A B A",
  { 0.90f, 0.76f, 0.34f, 0.24f, 0.30f },
  { { "I", "Em:4 Em:4",
           "r:16 | r:16" },
    { "A", "Em:4 C:4 D:4 B:4 Em:4 C:4 Am:4 B:4",
           "b5:4 e6:4 g6:2 f#6:2 e6:4 | e6:6 d6:2 c6:8 | d6:4 f#6:4 a6:4 f#6:4 | d#6:8 b5:8 |"
           "e6:2 g6:2 b6:4 a6:2 g6:2 e6:4 | g6:4 e6:4 c6:4 e6:4 | a5:4 c6:4 e6:4 c6:4 | b5:4 d#6:4 f#6:8" },
    { "B", "C:4 D:4 Bm:4 Em:4 C:4 D:4 B:8",
           "g5:6 e5:2 c5:8 | a5:6 f#5:2 d5:8 | f#5:4 d5:4 b4:8 | e5:4 g5:4 b5:8 |"
           "c6:4 b5:4 g5:4 e5:4 | d6:4 a5:4 f#5:4 d5:4 | d#5:8 f#5:8 | b5:16" } } },

{ "Snowfall Village", kDemoRetro, 132, 4, 3, kStChipWaltz, "F ionian", 0, "A A B A",
  { 0.88f, 0.66f, 0.30f, 0.26f, 0.28f },
  { { "A", "F:3 C:3 Dm:3 Bb:3 F:3 Gm:3 C:3 F:3",
           "a5:6 c6:2 a5:2 f5:2 | g5:8 e5:4 | f5:4 a5:4 d6:4 | d6:6 c6:2 bb5:4 |"
           "a5:4 c6:4 f6:4 | g6:4 f6:2 e6:2 d6:4 | e6:6 g6:2 e6:4 | f6:12" },
    { "B", "Bb:3 F:3 C:3 Dm:3 Bb:3 F:3 C:3 C:3",
           "f6:4 d6:4 bb5:4 | c6:6 a5:2 f5:4 | e5:4 g5:4 c6:4 | a5:8 f5:4 |"
           "bb5:4 d6:4 f6:4 | a6:6 g6:2 f6:4 | e6:4 d6:4 c6:4 | g5:12" } } },

{ "Title Screen Dreams", kDemoRetro, 128, 4, 4, kStChipMarch, "C ionian", 0, "A B A B",
  { 0.90f, 0.72f, 0.34f, 0.0f, 0.32f },
  { { "A", "C:4 G:4 Am:4 F:4 C:4 G:4 F:4 G:4",
           "e5:4 g5:4 c6:6 e6:2 | d6:4 b5:4 g5:8 | c6:4 e6:4 a6:6 g6:2 | f6:4 c6:4 a5:8 |"
           "g5:4 c6:4 e6:4 g6:4 | f6:4 d6:4 b5:4 g5:4 | a5:4 c6:4 f6:4 a6:4 | g6:12 r:4" },
    { "B", "Am:4 Em:4 F:4 C:4 Dm:4 G:4 C:8",
           "e6:6 d6:2 c6:4 a5:4 | b5:6 c6:2 b5:4 g5:4 | a5:4 c6:4 f6:4 e6:4 | g6:8 e6:8 |"
           "f6:4 e6:2 d6:2 a5:4 d6:4 | b5:4 d6:4 g6:4 f6:4 | e6:8 g6:8 | c7:16" } } },

// ============================================================================
// MEDIEVAL
// ============================================================================

{ "Lantern Estampie", kDemoMedieval, 150, 4, 3, kStEstampie, "D dorian", 0, "A A B B A",
  { 0.90f, 0.55f, 0.34f, 0.24f, 0.0f },
  { { "A", "Dm:3 C:3 Dm:3 Am:3 Dm:3 C:3 G:3 Dm:3",
           "d5:4 e5:2 f5:2 a5:4 | g5:4 e5:4 c5:4 | f5:2 e5:2 d5:4 a4:4 | c5:6 b4:2 a4:4 |"
           "d5:2 f5:2 a5:4 d6:4 | c6:4 g5:4 e5:4 | b5:4 a5:2 g5:2 d5:4 | d5:12" },
    { "B", "F:3 C:3 Dm:3 Am:3 F:3 G:3 Am:3 Dm:3",
           "a5:4 c6:4 a5:4 | g5:6 f5:2 e5:4 | f5:4 a5:4 d6:4 | e6:6 d6:2 c6:4 |"
           "c6:4 a5:2 g5:2 f5:4 | g5:4 b5:4 d6:4 | c6:4 b5:2 a5:2 e5:4 | d5:12" } } },

{ "Pilgrim's Road", kDemoMedieval, 72, 4, 4, kStChant, "E phrygian", 0, "A A B A",
  { 0.85f, 0.32f, 0.45f, 0.30f, 0.0f },
  { { "A", "Em:4 Em:4 Am:4 Em:4",
           "e4:8 g4:4 a4:4 | b4:8 a4:4 g4:4 | a4:4 c5:4 b4:4 a4:4 | g4:4 f4:4 e4:8" },
    { "B", "Am:4 G:4 Dm:4 Em:4",
           "c5:8 d5:4 e5:4 | d5:4 c5:4 b4:8 | a4:4 c5:4 d5:4 c5:4 | b4:4 a4:4 g4:2 f4:2 e4:4" } } },

{ "Saltarello of the Mill", kDemoMedieval, 100, 6, 2, kStJig, "A aeolian", 0, "A A B B A",
  { 0.90f, 0.55f, 0.34f, 0.22f, 0.0f },
  { { "A", "Am:2 G:2 Am:2 E:2 Am:2 G:2 F:1 E:1 Am:2",
           "e5:2 a5:2 b5:2 c6:4 a5:2 | b5:2 a5:2 g5:2 d5:4 g5:2 | a5:2 c6:2 e6:2 d6:2 c6:2 b5:2 | g#5:6 e5:6 |"
           "e5:2 a5:2 b5:2 c6:4 a5:2 | d6:4 b5:2 g5:4 b5:2 | a5:2 g5:2 f5:2 e5:2 g#5:2 b5:2 | a5:12" },
    { "B", "C:2 G:2 Am:2 E:2 F:2 C:2 Dm:1 E:1 Am:2",
           "g5:2 c6:2 e6:2 g6:4 e6:2 | d6:2 b5:2 g5:2 d6:6 | c6:2 b5:2 a5:2 e6:6 | e6:2 d6:2 b5:2 g#5:6 |"
           "f5:2 a5:2 c6:2 f6:6 | e6:2 d6:2 c6:2 g5:6 | f5:2 e5:2 d5:2 e5:2 g#5:2 b5:2 | a5:12" } } },

{ "Abbey Round", kDemoMedieval, 84, 4, 4, kStRound, "G ionian", 2, "A A A E",
  { 0.80f, 0.50f, 0.62f, 0.22f, 0.55f },
  { { "A", "G:4 D:4 G:4 D:4 G:4 D:4",
           "g4:8 b4:8 | a4:8 f#4:8 | d5:4 b4:4 g4:4 b4:4 | a4:4 d5:4 f#5:4 d5:4 |"
           "g5:2 f#5:2 g5:2 d5:2 b4:4 d5:4 | f#5:2 e5:2 d5:2 a4:2 f#4:4 a4:4" },
    { "E", "G:4 D:4 G:4 D:4",
           "r:16 | r:16 | r:16 | r:16" } } },

{ "The Falconer's Ductia", kDemoMedieval, 140, 4, 4, kStEstampie, "G mixolydian", 0, "A A B A",
  { 0.90f, 0.55f, 0.34f, 0.24f, 0.0f },
  { { "A", "G:4 F:4 G:4 C:4 G:4 F:4 C:2 Dm:2 G:4",
           "g5:4 b5:2 a5:2 g5:4 d5:4 | f5:4 a5:2 g5:2 f5:4 c5:4 | d5:2 g5:2 b5:2 d6:2 b5:4 g5:4 | e5:4 g5:4 c6:8 |"
           "b5:4 d6:2 c6:2 b5:4 g5:4 | a5:4 f5:2 g5:2 a5:4 c6:4 | e5:4 g5:4 f5:4 a5:4 | g5:12 r:4" },
    { "B", "C:4 G:4 F:4 G:4 C:4 Am:4 F:4 G:4",
           "c6:4 e6:4 g6:4 e6:4 | d6:4 b5:4 g5:8 | a5:4 c6:4 f6:4 c6:4 | b5:8 d6:8 |"
           "e6:2 d6:2 c6:2 b5:2 c6:4 g5:4 | a5:4 c6:4 e6:4 c6:4 | f5:4 a5:4 c6:4 a5:4 | g5:12 r:4" } } },

{ "Lament of the Tower", kDemoMedieval, 76, 4, 3, kStLute, "A aeolian", 0, "A B A",
  { 0.88f, 0.55f, 0.32f, 0.22f, 0.0f },
  { { "A", "Am:3 G:3 F:3 E:3 Am:3 Dm:3 E:3 Am:3",
           "a4:6 c5:2 b4:2 a4:2 | b4:6 d5:2 c5:2 b4:2 | c5:4 a4:4 f4:4 | g#4:8 e4:4 |"
           "e5:6 d5:2 c5:2 b4:2 | a4:4 d5:4 f5:4 | e5:4 d5:2 c5:2 b4:4 | a4:12" },
    { "B", "F:3 C:3 G:3 Am:3 F:3 Dm:3 E:3 E:3",
           "f5:6 e5:2 d5:2 c5:2 | e5:6 g5:2 e5:2 c5:2 | d5:4 b4:4 g4:4 | a4:6 c5:2 e5:4 |"
           "a5:6 g5:2 f5:2 e5:2 | f5:4 d5:4 a4:4 | b4:6 c5:2 b4:2 a4:2 | g#4:12" } } },

{ "Rose of the Marches", kDemoMedieval, 104, 4, 4, kStProcession, "F ionian", 0, "A B A",
  { 0.88f, 0.50f, 0.34f, 0.22f, 0.0f },
  { { "A", "F:4 C:4 Dm:4 Bb:4 F:4 Gm:4 C:4 F:4",
           "f5:4 g5:4 a5:8 | g5:4 e5:4 c5:8 | d5:4 f5:4 a5:4 g5:4 | f5:8 d5:8 |"
           "c6:4 a5:4 f5:4 a5:4 | bb5:4 a5:4 g5:8 | e5:4 f5:2 g5:2 c5:8 | f5:16" },
    { "B", "Bb:4 F:4 C:4 F:4 Dm:4 Am:4 Gm:4 C:4",
           "d6:8 c6:4 bb5:4 | a5:8 f5:8 | g5:4 a5:4 g5:4 e5:4 | f5:8 c5:8 |"
           "d5:4 f5:4 a5:8 | c6:8 a5:4 e5:4 | d5:4 g5:4 bb5:8 | g5:4 e5:4 c5:8" } } },

{ "Jongleur's Jig", kDemoMedieval, 116, 6, 2, kStJig, "D mixolydian", 0, "A A B B A A",
  { 0.90f, 0.55f, 0.34f, 0.22f, 0.0f },
  { { "A", "D:2 C:2 D:2 Am:2 D:2 C:2 G:1 Am:1 D:2",
           "a5:2 f#5:2 a5:2 d6:4 a5:2 | g5:2 e5:2 g5:2 c6:4 g5:2 | f#5:2 g5:2 a5:2 d5:4 f#5:2 | e5:6 a4:6 |"
           "a5:2 f#5:2 a5:2 d6:4 e6:2 | c6:2 b5:2 a5:2 g5:6 | b5:2 a5:2 g5:2 a5:2 c6:2 e6:2 | d6:12" },
    { "B", "G:2 D:2 C:2 G:2 Em:2 D:2 C:1 Am:1 D:2",
           "b5:2 d6:2 b5:2 g5:6 | a5:2 f#5:2 d5:2 a5:6 | g5:2 e5:2 c5:2 e5:4 g5:2 | d6:6 b5:6 |"
           "e6:2 d6:2 b5:2 g5:6 | f#5:2 e5:2 d5:2 a5:6 | e5:2 g5:2 c6:2 c6:2 a5:2 e5:2 | d5:6 f#5:2 a5:2 d6:2" } } },

{ "King's Procession", kDemoMedieval, 92, 4, 4, kStProcession, "C ionian", 0, "A A B A",
  { 0.90f, 0.58f, 0.34f, 0.22f, 0.0f },
  { { "A", "C:4 G:4 C:4 F:4 C:4 Am:4 G:4 C:4",
           "c5:4 c5:2 d5:2 e5:4 c5:4 | d5:4 d5:2 e5:2 f5:4 d5:4 | e5:4 g5:4 c6:6 b5:2 | a5:8 f5:8 |"
           "g5:4 e5:2 f5:2 g5:4 c6:4 | a5:4 e5:4 c5:8 | d5:4 g5:4 b5:4 d6:4 | c6:12 r:4" },
    { "B", "F:4 C:4 Dm:4 G:4 Am:4 F:4 G:4 C:4",
           "c6:4 a5:4 f5:4 a5:4 | g5:8 e5:8 | f5:4 e5:2 d5:2 a5:8 | g5:8 b4:8 |"
           "c5:4 e5:4 a5:6 g5:2 | f5:4 a5:4 c6:8 | b5:4 a5:2 g5:2 d5:8 | c5:12 r:4" } } },

{ "Harvest Round", kDemoMedieval, 120, 4, 3, kStRound, "D dorian", 2, "A A A A E",
  { 0.85f, 0.50f, 0.60f, 0.22f, 0.55f },
  { { "A", "Dm:3 C:3 Dm:3 C:3 Dm:3 C:3",
           "d5:12 | e5:12 | f5:4 a5:4 d5:4 | c5:4 e5:4 g5:4 |"
           "a5:2 g5:2 f5:2 e5:2 d5:4 | c5:2 e5:2 g5:2 e5:2 c5:4" },
    { "E", "Dm:3 C:3 Dm:3 C:3",
           "r:12 | r:12 | r:12 | r:12" } } },

// ============================================================================
// AMBIENT
// ============================================================================

{ "First Light", kDemoAmbient, 72, 4, 4, kStPiano, "C ionian", 0, "I A B A",
  { 0.70f, 0.45f, 0.30f, 0.18f, 0.24f },
  { { "I", "Fmaj7:4 Cadd9:4 Fmaj7:4 Cadd9:4",
           "r:16 | r:16 | r:16 | r:16" },
    { "A", "Fmaj7:4 Cadd9:4 Fmaj7:4 Cadd9:4 Am7:4 Em7:4 Fmaj7:4 G:4",
           "r:4 a5:4 c6:4 e6:4 | d6:12 r:4 | r:4 a5:4 c6:4 g6:4 | e6:12 r:4 |"
           "r:4 c6:4 e6:4 g6:4 | b5:12 r:4 | a5:4 g5:4 f5:4 e5:4 | d5:16" },
    { "B", "Dm7:4 Fmaj7:4 Am7:4 G:4 Dm7:4 Fmaj7:4 Cmaj7:8",
           "r:8 f5:4 a5:4 | c6:8 e6:8 | g6:12 e6:4 | d6:16 |"
           "r:8 c6:4 a5:4 | g5:8 a5:8 | e5:16 | r:16" } } },

{ "Moss and Stone", kDemoAmbient, 64, 4, 4, kStDrift, "D dorian", 0, "A B A B",
  { 0.66f, 0.42f, 0.26f, 0.20f, 0.22f },
  { { "A", "Dm9:8 Bbmaj7:8 Fmaj7:8 Cadd9:8",
           "r:8 a5:8 | e6:12 r:4 | d6:8 c6:8 | a5:16 | r:8 c6:8 | e6:8 g6:8 | d6:16 | r:16" },
    { "B", "Gm7:8 Bbmaj7:8 Dm9:8 C:8",
           "r:8 bb5:8 | d6:8 f6:8 | a6:16 | f6:16 | e6:8 d6:8 | a5:16 | g5:16 | e5:16" } } },

{ "Cartographer", kDemoAmbient, 76, 4, 4, kStPiano, "G ionian", 0, "I A B A",
  { 0.70f, 0.45f, 0.30f, 0.18f, 0.24f },
  { { "I", "Gmaj7:4 Em7:4",
           "r:16 | r:16" },
    { "A", "Gmaj7:4 Em7:4 Cmaj7:4 Dsus4:2 D:2 Gmaj7:4 Em7:4 Am7:4 D:4",
           "b5:6 a5:2 b5:4 d6:4 | g5:12 r:4 | e5:4 g5:4 b5:4 c6:4 | a5:8 f#5:8 |"
           "d6:6 c6:2 b5:8 | e6:8 d6:8 | c6:4 b5:4 a5:8 | f#5:16" },
    { "B", "Cmaj7:4 Bm7:4 Em7:4 G:4 Cmaj7:4 Am7:4 D:8",
           "g6:12 e6:4 | d6:12 b5:4 | g5:8 b5:8 | d6:16 |"
           "e6:4 d6:4 c6:4 b5:4 | c6:8 e6:8 | f#6:8 e6:8 | d6:16" } } },

{ "Rain on the Lake", kDemoAmbient, 80, 4, 4, kStMusicBox, "A aeolian", 0, "A B A B",
  { 0.68f, 0.45f, 0.30f, 0.0f, 0.0f },
  { { "A", "Am9:4 Fmaj7:4 Cmaj7:4 G6:4 Am9:4 Fmaj7:4 Dm7:4 E7sus4:4",
           "c6:8 b5:8 | a5:12 r:4 | g5:4 e5:4 g5:4 b5:4 | e6:8 d6:8 |"
           "c6:8 e6:8 | f6:8 e6:8 | d6:8 c6:8 | b5:16" },
    { "B", "Fmaj7:4 G6:4 Em7:4 Am9:4 Fmaj7:4 G6:4 Am9:8",
           "a5:8 c6:8 | b5:8 d6:8 | g5:8 b5:8 | c6:12 r:4 |"
           "e6:8 c6:8 | d6:8 b5:8 | a5:16 | r:16" } } },

{ "Little Hearth", kDemoAmbient, 84, 4, 3, kStWaltzAmb, "F ionian", 0, "A A B A",
  { 0.70f, 0.45f, 0.28f, 0.24f, 0.22f },
  { { "A", "Fmaj7:3 Dm7:3 Bbmaj7:3 C:3 Fmaj7:3 Am7:3 Bbmaj7:3 Csus4:3",
           "a5:8 g5:2 a5:2 | f5:12 | d6:6 c6:2 a5:4 | g5:12 |"
           "c6:8 e6:4 | g6:6 e6:6 | f6:4 d6:4 a5:4 | f5:12" },
    { "B", "Bbmaj7:3 C:3 Am7:3 Dm7:3 Gm7:3 C:3 Fmaj7:6",
           "f5:4 a5:4 d6:4 | e6:8 g5:4 | c6:12 | a5:6 f5:6 |"
           "bb5:6 d6:6 | e6:6 g6:6 | a5:12 | c6:12" } } },

{ "Slow River", kDemoAmbient, 70, 4, 4, kStPulse, "E ionian", 0, "A B A",
  { 0.68f, 0.45f, 0.26f, 0.20f, 0.22f },
  { { "A", "Emaj7:8 C#m7:8 Amaj7:8 Bsus4:4 B:4",
           "r:8 g#5:8 | b5:8 d#6:8 | e6:12 c#6:4 | b5:16 | c#6:8 e6:8 | g#6:16 | e6:16 | d#6:16" },
    { "B", "Amaj7:8 G#m7:8 C#m7:8 Bsus4:4 B:4",
           "e6:8 c#6:8 | a5:16 | b5:8 d#6:8 | f#6:16 | e6:8 g#6:8 | e6:16 | f#6:16 | d#6:16" } } },

{ "Pale Birch", kDemoAmbient, 68, 4, 4, kStPiano, "D ionian", 0, "I A B A",
  { 0.70f, 0.45f, 0.30f, 0.18f, 0.24f },
  { { "I", "Dmaj7:4 Bm7:4",
           "r:16 | r:16" },
    { "A", "Dmaj7:4 Bm7:4 Gmaj7:4 Asus4:4 Dmaj7:4 F#m7:4 Gmaj7:4 A:4",
           "f#5:8 a5:8 | d6:12 c#6:4 | b5:8 a5:8 | d6:16 |"
           "f#6:8 e6:8 | c#6:16 | d6:8 b5:8 | a5:16" },
    { "B", "Gmaj7:4 A:4 F#m7:4 Bm7:4 Em7:4 Gmaj7:4 Asus4:4 A:4",
           "g5:8 b5:8 | c#6:8 e6:8 | a5:12 r:4 | d6:8 f#6:8 |"
           "g6:8 e6:8 | f#6:12 r:4 | e6:16 | c#6:16" } } },

{ "Starlit Pasture", kDemoAmbient, 60, 4, 4, kStDrift, "Bb ionian", 0, "A B A B",
  { 0.66f, 0.42f, 0.26f, 0.20f, 0.22f },
  { { "A", "Bbmaj7:8 Gm9:8 Ebmaj7:8 F:8",
           "r:8 d5:8 | f5:8 a5:8 | bb5:16 | a5:16 | g5:8 bb5:8 | d6:16 | c6:16 | a5:16" },
    { "B", "Cm7:8 Ebmaj7:8 Bbmaj7:8 F:8",
           "eb5:8 g5:8 | bb5:16 | d6:16 | bb5:16 | a5:8 f5:8 | d5:16 | c5:16 | r:16" } } },

{ "Clay and Cloud", kDemoAmbient, 78, 4, 4, kStMusicBox, "A ionian", 0, "A B A B",
  { 0.68f, 0.45f, 0.30f, 0.0f, 0.0f },
  { { "A", "Amaj7:4 E:4 F#m7:4 Dmaj7:4 Amaj7:4 C#m7:4 Dmaj7:4 E:4",
           "c#6:8 e6:8 | b5:12 r:4 | a5:8 c#6:8 | f#6:8 e6:8 |"
           "e6:4 c#6:4 a5:8 | g#5:8 b5:8 | a5:8 f#5:8 | g#5:16" },
    { "B", "Dmaj7:4 E:4 C#m7:4 F#m7:4 Bm7:4 Dmaj7:4 E:8",
           "f#5:8 a5:8 | b5:8 g#5:8 | e6:12 r:4 | c#6:16 |"
           "d6:8 f#6:8 | e6:8 c#6:8 | b5:16 | r:16" } } },

{ "Low Tide", kDemoAmbient, 66, 4, 4, kStPulse, "E aeolian", 0, "A B A",
  { 0.68f, 0.45f, 0.26f, 0.20f, 0.22f },
  { { "A", "Em9:8 Cmaj7:8 G:8 Dsus2:8",
           "r:8 b5:8 | f#6:16 | e6:8 g6:8 | e6:16 | d6:16 | b5:16 | a5:16 | e5:16" },
    { "B", "Am7:8 Cmaj7:8 Em9:8 D:8",
           "c6:8 e6:8 | g6:16 | e6:16 | b5:16 | g5:8 b5:8 | f#5:16 | a5:16 | f#5:16" } } },

{ "Distant Village", kDemoAmbient, 90, 4, 3, kStWaltzAmb, "C ionian", 0, "A A B A",
  { 0.70f, 0.45f, 0.28f, 0.24f, 0.22f },
  { { "A", "C:3 Em7:3 Fmaj7:3 C:3 Am7:3 Dm7:3 Fmaj7:3 G:3",
           "e5:6 g5:6 | b5:8 g5:4 | a5:4 c6:4 e6:4 | g5:12 |"
           "c6:6 e6:6 | f6:4 e6:4 d6:4 | c6:12 | b5:6 d6:6" },
    { "B", "Fmaj7:3 G:3 Em7:3 Am7:3 Dm7:3 G:3 C:6",
           "a5:8 c6:4 | d6:8 b5:4 | g5:8 e5:4 | a5:12 |"
           "f5:6 a5:6 | g5:4 b5:4 d6:4 | e6:12 | c6:12" } } },

{ "Snowmelt", kDemoAmbient, 70, 4, 4, kStPiano, "Eb ionian", 0, "I A B A",
  { 0.70f, 0.45f, 0.30f, 0.18f, 0.24f },
  { { "I", "Ebmaj7:4 Abmaj7:4",
           "r:16 | r:16" },
    { "A", "Ebmaj7:4 Cm7:4 Abmaj7:4 Bb:4 Ebmaj7:4 Gm7:4 Abmaj7:4 Bbsus4:4",
           "g5:8 bb5:8 | eb6:12 d6:4 | c6:8 bb5:8 | f5:16 |"
           "bb5:8 d6:8 | f6:8 d6:8 | eb6:8 c6:8 | eb6:16" },
    { "B", "Abmaj7:4 Bb:4 Gm7:4 Cm7:4 Fm7:4 Abmaj7:4 Bbsus4:4 Bb:4",
           "c6:8 eb6:8 | d6:8 f6:8 | bb5:12 r:4 | g5:16 |"
           "ab5:8 c6:8 | eb6:8 g6:8 | f6:16 | d6:16" } } },

{ "Long Afternoon", kDemoAmbient, 62, 4, 4, kStDrift, "G ionian", 0, "A B A B",
  { 0.66f, 0.42f, 0.26f, 0.20f, 0.22f },
  { { "A", "Cmaj7:8 Gadd9:8 Cmaj7:8 D:8",
           "r:8 e5:8 | g5:8 b5:8 | a5:16 | d6:16 | e6:8 d6:8 | b5:16 | a5:16 | f#5:16" },
    { "B", "Em7:8 Cmaj7:8 Am7:8 D:8",
           "g5:8 b5:8 | d6:16 | e6:16 | g6:16 | c6:8 e6:8 | a5:16 | f#5:16 | d5:16" } } },

{ "Saplings", kDemoAmbient, 84, 4, 4, kStMusicBox, "F ionian", 0, "A B A B",
  { 0.70f, 0.45f, 0.30f, 0.0f, 0.0f },
  { { "A", "F:4 C:4 Dm7:4 Bbmaj7:4 F:4 Am7:4 Bbmaj7:4 C:4",
           "a5:4 c6:4 f6:4 c6:4 | e6:8 g5:8 | f5:4 a5:4 c6:8 | d6:12 r:4 |"
           "c6:4 a5:4 f5:4 a5:4 | e5:8 g5:8 | f5:4 a5:4 d6:8 | c6:16" },
    { "B", "Bbmaj7:4 C:4 Am7:4 Dm7:4 Gm7:4 C:4 F:8",
           "d6:8 f6:8 | e6:8 c6:8 | g5:8 c6:8 | a5:16 |"
           "bb5:8 d6:8 | g6:8 e6:8 | f6:16 | r:16" } } },

{ "Quiet Harbor", kDemoAmbient, 66, 4, 4, kStPiano, "B aeolian", 0, "I A B A",
  { 0.70f, 0.45f, 0.30f, 0.18f, 0.24f },
  { { "I", "Bm7:4 Gmaj7:4",
           "r:16 | r:16" },
    { "A", "Bm7:4 Gmaj7:4 Dmaj7:4 A:4 Bm7:4 Gmaj7:4 Em7:4 F#m7:4",
           "d6:8 c#6:8 | b5:16 | a5:8 f#5:8 | e5:16 |"
           "f#5:8 a5:8 | d6:8 f#6:8 | g6:8 e6:8 | c#6:16" },
    { "B", "Gmaj7:4 A:4 F#m7:4 Bm7:4 Em7:4 Gmaj7:4 A:8",
           "b5:8 d6:8 | e6:12 c#6:4 | a5:16 | f#5:8 d5:8 |"
           "e5:8 g5:8 | b5:8 a5:8 | c#6:16 | e6:16" } } },

};
