#pragma once

#include "PixelLabsIds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace PixelLabs {

inline const char* waveformName(int idx) {
    static const char* kNames[kNumWaveforms] = {
        "Pulse 50%", "Pulse 25%", "Pulse 12.5%", "NES Triangle", "GB Wavetable", "LFSR Noise",
        "PWM Sweep", "Chip Saw", "16-Bit FM", "FM E.Piano", "FM Slap Bass", "FM Bell",
        "Soft Strings", "Pipe Organ", "Flute", "Reed", "Lute", "Bowed Viol", "Choir", "Drum Kit"
    };
    return kNames[std::clamp(idx, 0, kNumWaveforms - 1)];
}

inline const char* harmonyName(int idx) {
    static const char* kNames[kNumHarmonyModes] = {
        "Single", "5th (+7)", "Octave (+12)", "Organum", "Arp Major", "Arp Minor", "Arp Octave", "Coin Jump"
    };
    return kNames[std::clamp(idx, 0, kNumHarmonyModes - 1)];
}

inline const char* reverbTypeName(int idx) {
    static const char* kNames[kNumReverbTypes] = { "Room", "Hall", "Church", "Cave" };
    return kNames[std::clamp(idx, 0, kNumReverbTypes - 1)];
}

inline const char* delayTypeName(int idx) {
    static const char* kNames[kNumDelayTypes] = { "Ping-Pong", "Tape Echo", "16-Bit Echo", "Slapback", "Canyon" };
    return kNames[std::clamp(idx, 0, kNumDelayTypes - 1)];
}

inline const char* touchName(int idx) {
    static const char* kNames[kNumTouchCurves] = { "Off (Fixed)", "Light", "Normal", "Heavy" };
    return kNames[std::clamp(idx, 0, kNumTouchCurves - 1)];
}

inline const char* noteName(int midiNote) {
    static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return kNames[((midiNote % 12) + 12) % 12];
}

// MIDI 60 = C4
inline int noteOctave(int midiNote) { return midiNote / 12 - 1; }

// Display text for a parameter value (shared by the host strings and the editor).
inline void formatParamValue(int id, double v, char* buf, size_t n);

} // namespace PixelLabs

#include "../presets/FactoryPresets.h"

namespace PixelLabs {

inline void formatParamValue(int id, double v, char* buf, size_t n) {
    v = std::clamp(v, 0.0, 1.0);
    switch (id) {
        case kParamWaveform:   std::snprintf(buf, n, "%s", waveformName(normToList(v, kNumWaveforms))); break;
        case kParamHarmony:    std::snprintf(buf, n, "%s", harmonyName(normToList(v, kNumHarmonyModes))); break;
        case kParamReverbType: std::snprintf(buf, n, "%s", reverbTypeName(normToList(v, kNumReverbTypes))); break;
        case kParamDelayType:  std::snprintf(buf, n, "%s", delayTypeName(normToList(v, kNumDelayTypes))); break;
        case kParamTouch:      std::snprintf(buf, n, "%s", touchName(normToList(v, kNumTouchCurves))); break;
        case kParamPreset:     std::snprintf(buf, n, "%s", factoryPreset(normToList(v, kNumFactoryPresets)).name); break;
        case kParamSustain:    std::snprintf(buf, n, "%s", v >= 0.5 ? "On" : "Off"); break;
        case kParamPitchBend:  std::snprintf(buf, n, "%+.2f st", (v - 0.5) * 4.0); break;
        case kParamWidth:      std::snprintf(buf, n, "%.0f%%", v * 200.0); break;
        case kParamSweep: {
            if (std::fabs(v - 0.5) < 0.01) { std::snprintf(buf, n, "Off"); break; }
            const double a = (v - 0.5) / 0.5;
            std::snprintf(buf, n, "%+.1f st", (a >= 0 ? 1.0 : -1.0) * a * a * 48.0);
            break;
        }
        case kParamSweepTime:  std::snprintf(buf, n, "%.2f s", 0.01 + v * v * 1.5); break;
        case kParamArpRate:    std::snprintf(buf, n, "%.1f Hz", 4.0 * std::pow(16.0, v)); break;
        default:               std::snprintf(buf, n, "%.0f%%", v * 100.0); break;
    }
}

} // namespace PixelLabs
