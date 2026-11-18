#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace PixelLabs {

// Built-in demo songs. They play on the loaded sound, five monophonic tracks at a time.
enum DemoGroups { kDemoRetro = 0, kDemoMedieval, kDemoAmbient, kNumDemoGroups };

static constexpr int kNumDemoSongs = 34; // indices are stable: 0..3 are the original four songs
static constexpr int kDemoTracks = 5;

struct DemoSong {
    const char* name = "";
    int group = kDemoRetro;
    double stepSec = 0.12;
    int numSteps = 0;
    std::vector<int8_t> tracks[kDemoTracks]; // >=0 note on, -1 rest, -2 hold
    float velocity[kDemoTracks] = { 0.9f, 0.78f, 0.3f, 0.3f, 0.5f };
};

// Built once on first use and read-only afterwards, so every instance shares one copy.
const std::vector<DemoSong>& demoSongs();

const char* demoSongName(int index);
int demoSongGroup(int index);
const char* demoGroupName(int group);
const char* demoGroupHint(int group);   // which factory sounds suit the group
int demoSongForEra(int era);            // what PLAY DEMO starts when no song was picked
int demoGroupSongs(int group, int* out, int maxOut); // songs of a group in picker order

// Problems found while building the song book; empty when every song is well formed.
std::vector<std::string> demoSongIssues();

} // namespace PixelLabs
