#pragma once

#include "vst3/vst3_abi.h"
#include <atomic>
#include <cstdint>

namespace PixelLabs {

static constexpr Steinberg::TUID kPixelLabsProcessorUID =
    INLINE_UID_COM(0x50584C42, 0x53594E54, 0x4850524F, 0x43303031);

static constexpr Steinberg::TUID kPixelLabsControllerUID =
    INLINE_UID_COM(0x50584C42, 0x53594E54, 0x4843544C, 0x52303031);

// IDs 0..15 keep their v1 meaning so existing automation lanes stay valid.
enum ParamIds : Steinberg::Vst::ParamID {
    kParamExpression   = 0,   // Output level fader
    kParamDynamics     = 1,   // Timbre fader (PWM / FM index / brightness / vowel, per waveform)
    kParamReverb       = 2,   // FX: reverb amount
    kParamCrush        = 3,   // FX: bitcrush + decimate
    kParamChorus       = 4,   // FX: ensemble chorus
    kParamDelay        = 5,   // FX: delay amount
    kParamEnvShape     = 6,   // Contour macro: scales decay + release time (0.5 = neutral)
    kParamVibrato      = 7,   // Delayed pitch vibrato depth
    kParamWaveform     = 8,   // List: kNumWaveforms
    kParamHarmony      = 9,   // List: kNumHarmonyModes
    kParamSubMix       = 10,  // Sub oscillator (-1 oct) level
    kParamCutoff       = 11,  // Lowpass cutoff
    kParamResonance    = 12,  // Lowpass resonance
    kParamPreset       = 13,  // List: factory presets
    kParamPitchBend    = 14,  // +/- 2 semitones, centre 0.5
    kParamSustain      = 15,  // Sustain pedal CC64

    kParamPhaser       = 16,  // FX: 4-stage stereo phaser
    kParamDrive        = 17,  // FX: saturation
    kParamTape         = 18,  // FX: wow / flutter / tape tone
    kParamTremolo      = 19,  // FX: stereo tremolo
    kParamSpeaker      = 20,  // FX: handheld speaker tone
    kParamWidth        = 21,  // FX: stereo width (0.5 = natural)
    kParamReverbType   = 22,  // List: kNumReverbTypes
    kParamDelayType    = 23,  // List: kNumDelayTypes
    kParamSweep        = 24,  // Pitch sweep into the note (0.5 = off, <0.5 rise, >0.5 fall)
    kParamSweepTime    = 25,
    kParamArpRate      = 26,
    kParamGlide        = 27,
    kParamNoise        = 28,  // Noise layer level
    kParamAttack       = 29,
    kParamDecay        = 30,
    kParamSustainLevel = 31,
    kParamRelease      = 32,
    kParamFilterEnv    = 33,  // Envelope -> cutoff amount
    kParamDetune       = 34,  // Oscillator 2 / unison detune

    // Keyboard touch: global playing settings, never part of a saved sound
    kParamVelSens      = 35,  // How much key velocity changes loudness and tone (0 = fixed)
    kParamTouch        = 36,  // List: kNumTouchCurves
    kNumParams         = 37
};

static constexpr int kNumParamsV1 = 16;
static constexpr int kNumParamsV2 = 35;

enum TouchCurves { kTouchOff = 0, kTouchLight, kTouchNormal, kTouchHeavy, kNumTouchCurves };
static constexpr float kDefaultVelSens = 0.65f; // matches the fixed v2 velocity response

// Live controller and keyboard settings: preset loads leave them alone and they never
// count as an edit to the sound.
inline bool isPerformanceParam(int id) {
    return id == kParamPreset || id == kParamPitchBend || id == kParamSustain ||
           id == kParamVelSens || id == kParamTouch;
}

enum Waveforms {
    kWavePulse50 = 0, kWavePulse25, kWavePulse12, kWaveNesTri,
    kWaveGbWave, kWaveNoise, kWavePwm, kWaveChipSaw,
    kWaveFm16, kWaveFmKeys, kWaveFmBass, kWaveBell,
    kWaveSoftStr, kWaveOrgan, kWaveFlute, kWaveReed,
    kWaveLute, kWaveBowed, kWaveChoir, kWaveDrums,
    kNumWaveforms
};

enum HarmonyModes {
    kHarmSingle = 0, kHarmFifth, kHarmOctave, kHarmOrganum,
    kHarmArpMajor, kHarmArpMinor, kHarmArpOctave, kHarmCoin,
    kNumHarmonyModes
};

enum ReverbTypes { kRevRoom = 0, kRevHall, kRevChurch, kRevCave, kNumReverbTypes };
enum DelayTypes  { kDlyPing = 0, kDlyTape, kDly16Bit, kDlySlap, kDlyCanyon, kNumDelayTypes };

enum Eras { kEraChip80 = 0, kEraChip90, kEraSynth, kEraMedieval, kEraSfx, kNumEras };

// The centre dial edits one of these at a time.
static constexpr int kNumMacros = 12;
static constexpr int kMacroParamIds[kNumMacros] = {
    kParamReverb, kParamDelay, kParamChorus, kParamPhaser, kParamTremolo, kParamTape,
    kParamDrive, kParamCrush, kParamSpeaker, kParamWidth, kParamEnvShape, kParamVibrato
};

// Browser tabs: 0 = ALL, 1..kNumEras = eras, then the user library views
enum BrowserTabs {
    kTabAll = 0,
    kTabUser = kNumEras + 1,   // MY SOUNDS
    kTabFavorites,
    kTabBanks,
    kNumBrowserTabs
};

static constexpr int kNumThemes = 8;   // 0 = AUTO (follow preset era), 1..7 fixed skins (append only)
enum Themes { kThemeAuto = 0, kThemeNes, kThemeDmg, kThemeNeon, kThemeCodex, kThemeArcade, kThemePaper, kThemeGilded };
static constexpr int kDefaultTheme = kThemeGilded; // new instances; projects keep the skin they saved
static constexpr int kNumUiScales = 4; // 100 / 125 / 150 / 200 %

inline float listToNorm(int idx, int count) {
    return (count > 1) ? static_cast<float>(idx) / static_cast<float>(count - 1) : 0.0f;
}

inline int normToList(double v, int count) {
    int idx = static_cast<int>(v * (count - 1) + 0.5);
    if (idx < 0) idx = 0;
    if (idx > count - 1) idx = count - 1;
    return idx;
}

struct StateSnapshotV1 {
    uint32_t magic;   // 'PXL1'
    uint32_t version;
    double params[kNumParamsV1];
    int32_t activeMacroIndex;
    int32_t darkTheme;
};

// v2 projects: 35 parameters, 47 factory presets
struct StateSnapshotV2 {
    uint32_t magic;   // 'PXL2'
    uint32_t version;
    double params[kNumParamsV2];
    int32_t activeMacroIndex;
    int32_t theme;
    int32_t uiScale;
    int32_t eraTab;
};
static constexpr int kNumFactoryPresetsV2 = 47;

struct StateSnapshot {
    uint32_t magic = 0x50584C33; // 'PXL3'
    uint32_t version = 3;
    double params[kNumParams];
    int32_t activeMacroIndex = 0;
    int32_t theme = 0;
    int32_t uiScale = 0;
    int32_t eraTab = 0;
    // The sound that is loaded, so a project shows the same name and "edited" state
    int32_t soundKind = 0;       // SoundKind
    uint32_t soundId = 0;        // factory slot or user sound id
    int32_t soundEra = 0;
    int32_t basePreset = 0;
    char soundName[48] = {};
    char soundCategory[16] = {};
    double refParams[kNumParams]; // the sound as last loaded or saved
};

// Lock-free bridge between the audio engine and the editor.
struct SharedUITelemetry {
    std::atomic<float> params[kNumParams];
    std::atomic<int> activeMacro{0};
    std::atomic<int> theme{kDefaultTheme};
    std::atomic<int> uiScale{0};
    std::atomic<int> eraTab{0};       // 0 = ALL, 1..kNumEras
    std::atomic<int> currentPreset{0};
    std::atomic<int> activeVoices{0};
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};

    std::atomic<uint64_t> activeNotesLow{0};  // MIDI 0..63
    std::atomic<uint64_t> activeNotesHigh{0}; // MIDI 64..127

    // Editor -> engine note queue (single producer, single consumer).
    static constexpr uint32_t kGuiQueueSize = 32;
    std::atomic<int32_t> guiQueue[kGuiQueueSize];
    std::atomic<uint32_t> guiQueueWrite{0};
    std::atomic<uint32_t> guiQueueRead{0};

    std::atomic<int> demoActive{0};
    std::atomic<int> demoSong{0};      // which built-in song the engine is playing
    std::atomic<float> tempoBpm{125.0f};
    std::atomic<int> lastVelocity{0};  // raw 0..127 of the last played note (for the touch readout)

    float scopeSamples[256] = {0.0f};
    std::atomic<uint32_t> scopeWriteIdx{0};

    SharedUITelemetry() {
        for (int i = 0; i < kNumParams; ++i) params[i].store(0.5f, std::memory_order_relaxed);
        for (uint32_t i = 0; i < kGuiQueueSize; ++i) guiQueue[i].store(0, std::memory_order_relaxed);
    }

    void pushGuiNote(int pitch, int velocity) {
        uint32_t w = guiQueueWrite.load(std::memory_order_relaxed);
        guiQueue[w % kGuiQueueSize].store((pitch & 0x7F) | ((velocity & 0x7F) << 8), std::memory_order_relaxed);
        guiQueueWrite.store(w + 1, std::memory_order_release);
    }

    bool popGuiNote(int& pitch, int& velocity) {
        uint32_t r = guiQueueRead.load(std::memory_order_relaxed);
        uint32_t w = guiQueueWrite.load(std::memory_order_acquire);
        if (r == w) return false;
        if (w - r > kGuiQueueSize) r = w - kGuiQueueSize;
        int32_t e = guiQueue[r % kGuiQueueSize].load(std::memory_order_relaxed);
        pitch = e & 0x7F;
        velocity = (e >> 8) & 0x7F;
        guiQueueRead.store(r + 1, std::memory_order_relaxed);
        return true;
    }
};

} // namespace PixelLabs
