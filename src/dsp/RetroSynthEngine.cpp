#include "RetroSynthEngine.h"
#include "../presets/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <xmmintrin.h>

namespace PixelLabs {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi    = 3.14159265358979323846;
constexpr float kInv2Pi = 0.15915494309189535f;


struct Tables {
    float sine[2050];
    float gb[4][32]; // 4-bit, 32-step handheld wavetables
    Tables() {
        for (int i = 0; i < 2050; ++i) sine[i] = static_cast<float>(std::sin(kTwoPi * i / 2048.0));
        for (int t = 0; t < 4; ++t) {
            double raw[32];
            double peak = 1e-9;
            for (int i = 0; i < 32; ++i) {
                double x = kTwoPi * (i + 0.5) / 32.0;
                double y = 0.0;
                switch (t) {
                    case 0: y = std::sin(x); break;                                              // soft
                    case 1: y = std::sin(x) + 0.5 * std::sin(2 * x) + 0.33 * std::sin(3 * x); break; // wave bass
                    case 2: y = std::sin(x) + 0.6 * std::sin(3 * x) + 0.3 * std::sin(5 * x); break;  // hollow
                    default: y = 1.0 - 2.0 * (i + 0.5) / 32.0; break;                            // ramp
                }
                raw[i] = y;
                peak = std::max(peak, std::fabs(y));
            }
            for (int i = 0; i < 32; ++i) {
                double q = std::floor((raw[i] / peak * 0.5 + 0.5) * 15.0 + 0.5); // 0..15
                gb[t][i] = static_cast<float>(q / 7.5 - 1.0);
            }
        }
    }
};
const Tables gT;

inline float fsin(double ph) {
    ph -= std::floor(ph);
    double x = ph * 2048.0;
    int i = static_cast<int>(x);
    float f = static_cast<float>(x - i);
    i &= 2047;
    return gT.sine[i] + (gT.sine[i + 1] - gT.sine[i]) * f;
}

inline float polyBlep(double t, double dt) {
    if (dt <= 0.0) return 0.0f;
    if (t < dt) {
        t /= dt;
        return static_cast<float>(t + t - t * t - 1.0);
    } else if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return static_cast<float>(t * t + t + t + 1.0);
    }
    return 0.0f;
}

inline float pulseBlep(double phase, double dt, double pw) {
    float val = (phase < pw) ? 1.0f : -1.0f;
    val += polyBlep(phase, dt);
    double p2 = phase - pw;
    if (p2 < 0.0) p2 += 1.0;
    val -= polyBlep(p2, dt);
    return val;
}

inline float sawBlep(double phase, double dt) {
    float val = static_cast<float>(2.0 * phase - 1.0);
    val -= polyBlep(phase, dt);
    return val;
}

inline float nesTriRaw(double phase, float smoothness) {
    // 16-step (4-bit) console triangle blended toward a pure triangle
    double tri = (phase < 0.5) ? (4.0 * phase - 1.0) : (3.0 - 4.0 * phase);
    double stepped = std::floor((tri + 1.0) * 7.5 + 0.5) / 7.5 - 1.0;
    return static_cast<float>(stepped * (1.0 - 0.55 * smoothness) + tri * (0.55 * smoothness));
}

// Stepped waves are box-filtered over four sub-samples to tame aliasing on high notes.
inline float nesTri(double phase, double dt, float smoothness) {
    float acc = 0.0f;
    for (int k = 0; k < 4; ++k) {
        double p = phase + dt * (k * 0.25 - 0.375);
        if (p < 0.0) p += 1.0; else if (p >= 1.0) p -= 1.0;
        acc += nesTriRaw(p, smoothness);
    }
    return acc * 0.25f;
}

inline float tableWave(const float* table, double phase, double dt) {
    float acc = 0.0f;
    for (int k = 0; k < 4; ++k) {
        double p = phase + dt * (k * 0.25 - 0.375);
        if (p < 0.0) p += 1.0; else if (p >= 1.0) p -= 1.0;
        acc += table[static_cast<int>(p * 32.0) & 31];
    }
    return acc * 0.25f;
}

inline float whiteNoise(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(s)) * (1.0f / 2147483648.0f);
}

inline void lfsrStep(uint32_t& l, bool shortMode) {
    uint32_t bit = (l ^ (l >> 1)) & 1u;
    l = (l >> 1) | (bit << 14);
    if (shortMode) l = (l & ~0x40u) | (bit << 6);
}

inline float softClip(float x) {
    const float t = 0.65f;
    float ax = std::fabs(x);
    if (ax <= t) return x;
    float y = t + (1.0f - t) * std::tanh((ax - t) / (1.0f - t));
    return x < 0.0f ? -y : y;
}

inline float onePoleCoef(double hz, double sampleRate) {
    return static_cast<float>(1.0 - std::exp(-kTwoPi * hz / sampleRate));
}

// Loudness trim per waveform so presets sit at a similar level.
const float kWaveGain[kNumWaveforms] = {
    1.00f, 1.05f, 1.20f, 1.35f,   // pulse 50 / 25 / 12, tri
    1.30f, 0.85f, 1.00f, 1.00f,   // wave, noise, pwm, saw
    1.10f, 1.25f, 1.20f, 1.25f,   // fm, keys, fm bass, bell
    1.60f, 1.00f, 1.45f, 1.30f,   // soft strings, organ, flute, reed
    1.35f, 1.50f, 2.60f, 1.50f    // lute, bowed, choir, drums
};

// Drum synthesis models. Every kit maps one drum to each note name (C kick, D snare...).
enum DrumModel {
    kDmKick = 0,  // sine with a pitch drop, plus a noise click
    kDmRim,       // short triangle tick
    kDmSnare,     // sine body plus noise
    kDmClap,      // three noise bursts and a tail
    kDmTom,       // pitched sine (with optional noise)
    kDmHat,       // bright hiss
    kDmCowbell,   // two detuned squares
    kDmMetal,     // six inharmonic squares, highpassed (808 hats, cymbals, jingles)
    kDmNes,       // 4-bit triangle body plus an LFSR noise channel (aux = noise clock, colour >= 0.5 = metallic mode)
    kDmBell,      // inharmonic partials (finger cymbals, bells)
    kDmShaker     // hiss that swells in over tauA and dies over tauB
};

struct DrumSpec {
    int model;
    float tauA, tauB, tauP;  // decay of the tone, the noise and the pitch drop (s)
    float baseHz, rangeHz;   // pitch at rest, extra pitch at the hit
    float gainT, gainN;      // tone and noise levels
    float color;             // noise colour: 0 white, >0 brighter, <0 darker
    float aux;               // model specific
};

const DrumSpec kDrumKits[kNumDrumKits][12] = {
    { // CHIP: the original kit
        { kDmKick,    0.160f, 0.004f, 0.035f,   48.0f, 170.0f, 1.25f, 0.50f, 0.0f, 0.0f }, // C  kick
        { kDmRim,     0.014f, 0.004f, 0.010f, 1700.0f,   0.0f, 0.90f, 0.00f, 0.0f, 0.0f }, // C# rim
        { kDmSnare,   0.070f, 0.110f, 0.020f,  185.0f,  90.0f, 0.60f, 0.80f, 0.0f, 0.0f }, // D  snare
        { kDmClap,    0.100f, 0.100f, 0.010f,  300.0f,   0.0f, 0.90f, 1.10f, 0.0f, 0.0f }, // D# clap
        { kDmSnare,   0.050f, 0.060f, 0.015f,  240.0f,  60.0f, 0.60f, 0.80f, 0.0f, 0.0f }, // E  tight snare
        { kDmTom,     0.220f, 0.004f, 0.050f,   85.0f,  75.0f, 1.10f, 0.00f, 0.0f, 0.0f }, // F  low tom
        { kDmHat,     0.010f, 0.028f, 0.010f,  300.0f,   0.0f, 0.00f, 1.10f, 0.0f, 0.0f }, // F# closed hat
        { kDmTom,     0.200f, 0.004f, 0.050f,  125.0f, 110.0f, 1.10f, 0.00f, 0.0f, 0.0f }, // G  mid tom
        { kDmHat,     0.010f, 0.200f, 0.010f,  300.0f,   0.0f, 0.00f, 1.10f, 0.0f, 0.0f }, // G# open hat
        { kDmTom,     0.180f, 0.004f, 0.045f,  175.0f, 150.0f, 1.10f, 0.00f, 0.0f, 0.0f }, // A  high tom
        { kDmHat,     0.010f, 0.550f, 0.010f,  300.0f,   0.0f, 0.00f, 1.10f, 0.0f, 0.0f }, // A# crash
        { kDmCowbell, 0.090f, 0.004f, 0.010f,  540.0f,   0.0f, 0.80f, 0.00f, 0.0f, 0.0f }  // B  cowbell
    },
    { // 808 BOOM: long sub kick, metallic hats and cymbal
        { kDmKick,    0.550f, 0.003f, 0.060f,   46.0f,  75.0f, 1.35f, 0.15f, 0.0f, 0.0f },
        { kDmRim,     0.012f, 0.004f, 0.008f, 1650.0f,   0.0f, 0.90f, 0.00f, 0.0f, 0.0f },
        { kDmSnare,   0.100f, 0.130f, 0.015f,  180.0f,  40.0f, 0.55f, 0.85f, 0.6f, 0.0f },
        { kDmClap,    0.160f, 0.100f, 0.010f,  300.0f,   0.0f, 0.90f, 1.10f, 0.0f, 0.0f },
        { kDmSnare,   0.050f, 0.070f, 0.012f,  240.0f,  40.0f, 0.50f, 0.80f, 0.8f, 0.0f },
        { kDmTom,     0.420f, 0.004f, 0.080f,   75.0f,  25.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmMetal,   0.010f, 0.045f, 0.010f,  205.0f,   0.0f, 1.90f, 0.40f, 0.0f, 0.0f },
        { kDmTom,     0.360f, 0.004f, 0.080f,  105.0f,  30.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmMetal,   0.010f, 0.320f, 0.010f,  205.0f,   0.0f, 1.90f, 0.40f, 0.0f, 0.0f },
        { kDmTom,     0.300f, 0.004f, 0.070f,  150.0f,  40.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmMetal,   0.010f, 1.100f, 0.010f,  260.0f,   0.0f, 1.30f, 0.70f, 0.0f, 0.0f },
        { kDmCowbell, 0.120f, 0.004f, 0.010f,  540.0f,   0.0f, 0.80f, 0.00f, 0.0f, 0.0f }
    },
    { // NES NOISE: triangle drums and the console's noise channel
        { kDmNes,     0.120f, 0.004f, 0.025f,   55.0f, 300.0f, 1.50f, 0.00f, 0.0f,  8000.0f },
        { kDmNes,     0.004f, 0.012f, 0.010f,    0.0f,   0.0f, 0.00f, 0.80f, 1.0f, 12000.0f },
        { kDmNes,     0.050f, 0.100f, 0.020f,  180.0f, 120.0f, 0.60f, 0.70f, 0.0f,  6000.0f },
        { kDmNes,     0.004f, 0.080f, 0.010f,    0.0f,   0.0f, 0.00f, 0.80f, 1.0f,  9000.0f },
        { kDmNes,     0.030f, 0.060f, 0.015f,  220.0f, 100.0f, 0.40f, 0.80f, 0.0f, 14000.0f },
        { kDmNes,     0.200f, 0.004f, 0.080f,   90.0f,  60.0f, 1.15f, 0.00f, 0.0f,  8000.0f },
        { kDmNes,     0.004f, 0.025f, 0.010f,    0.0f,   0.0f, 0.00f, 0.90f, 0.0f, 40000.0f },
        { kDmNes,     0.180f, 0.004f, 0.070f,  130.0f,  80.0f, 1.15f, 0.00f, 0.0f,  8000.0f },
        { kDmNes,     0.004f, 0.180f, 0.010f,    0.0f,   0.0f, 0.00f, 0.90f, 0.0f, 40000.0f },
        { kDmNes,     0.160f, 0.004f, 0.060f,  180.0f, 110.0f, 1.15f, 0.00f, 0.0f,  8000.0f },
        { kDmNes,     0.004f, 0.600f, 0.010f,    0.0f,   0.0f, 0.00f, 0.70f, 0.0f, 25000.0f },
        { kDmCowbell, 0.070f, 0.004f, 0.010f,  880.0f,   0.0f, 0.80f, 0.00f, 0.0f,     0.0f }
    },
    { // ELECTRO: punchy clicky kick, bright snare, metallic hats
        { kDmKick,    0.300f, 0.008f, 0.030f,   52.0f, 220.0f, 1.10f, 0.70f, 1.0f, 0.0f },
        { kDmRim,     0.010f, 0.004f, 0.008f, 1900.0f,   0.0f, 0.90f, 0.00f, 0.0f, 0.0f },
        { kDmSnare,   0.080f, 0.160f, 0.015f,  200.0f,  60.0f, 0.55f, 0.95f, 0.7f, 0.0f },
        { kDmClap,    0.120f, 0.100f, 0.010f,  300.0f,   0.0f, 0.90f, 1.10f, 0.0f, 0.0f },
        { kDmSnare,   0.040f, 0.090f, 0.010f,  260.0f,  50.0f, 0.45f, 0.90f, 1.0f, 0.0f },
        { kDmTom,     0.250f, 0.004f, 0.040f,   95.0f,  90.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmMetal,   0.010f, 0.035f, 0.010f,  300.0f,   0.0f, 1.80f, 0.50f, 0.0f, 0.0f },
        { kDmTom,     0.220f, 0.004f, 0.040f,  135.0f, 110.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmMetal,   0.010f, 0.260f, 0.010f,  300.0f,   0.0f, 1.80f, 0.50f, 0.0f, 0.0f },
        { kDmTom,     0.200f, 0.004f, 0.035f,  185.0f, 140.0f, 1.10f, 0.00f, 0.0f, 0.0f },
        { kDmHat,     0.010f, 0.900f, 0.010f,  300.0f,   0.0f, 0.00f, 1.00f, 0.0f, 0.0f },
        { kDmCowbell, 0.080f, 0.004f, 0.010f,  600.0f,   0.0f, 0.80f, 0.00f, 0.0f, 0.0f }
    },
    { // DUSTY BREAK: soft, dark sampled-break feel
        { kDmKick,    0.220f, 0.012f, 0.040f,   50.0f, 120.0f, 1.30f, 0.30f, -0.6f, 0.0f },
        { kDmTom,     0.020f, 0.004f, 0.004f,  900.0f, 200.0f, 0.80f, 0.00f,  0.0f, 0.0f },
        { kDmSnare,   0.090f, 0.150f, 0.015f,  190.0f,  30.0f, 0.70f, 0.70f, -0.2f, 0.0f },
        { kDmClap,    0.100f, 0.100f, 0.010f,  300.0f,   0.0f, 1.40f, 1.80f,  0.0f, 0.0f },
        { kDmSnare,   0.040f, 0.060f, 0.010f,  220.0f,  30.0f, 0.60f, 0.80f,  0.0f, 0.0f },
        { kDmTom,     0.250f, 0.004f, 0.050f,   80.0f,  40.0f, 1.10f, 0.00f,  0.0f, 0.0f },
        { kDmHat,     0.010f, 0.030f, 0.010f,  300.0f,   0.0f, 0.00f, 1.40f,  0.0f, 0.0f },
        { kDmTom,     0.220f, 0.004f, 0.050f,  115.0f,  50.0f, 1.10f, 0.00f,  0.0f, 0.0f },
        { kDmHat,     0.010f, 0.150f, 0.010f,  300.0f,   0.0f, 0.00f, 1.40f,  0.0f, 0.0f },
        { kDmTom,     0.200f, 0.004f, 0.045f,  160.0f,  60.0f, 1.10f, 0.00f,  0.0f, 0.0f },
        { kDmMetal,   0.010f, 0.800f, 0.010f,  330.0f,   0.0f, 1.60f, 0.80f,  0.0f, 0.0f },
        { kDmShaker,  0.020f, 0.090f, 0.010f,    0.0f,   0.0f, 0.00f, 2.20f,  0.0f, 0.0f }
    },
    { // TAVERN: frame drum, tabor, darbuka, tambourine, finger cymbals, hand bell
        { kDmSnare,   0.300f, 0.080f, 0.050f,   70.0f,  30.0f, 1.20f, 0.35f, -0.7f, 0.0f }, // frame drum
        { kDmTom,     0.035f, 0.004f, 0.004f, 1250.0f, 200.0f, 0.55f, 0.00f,  0.0f, 0.0f }, // wood block
        { kDmSnare,   0.100f, 0.200f, 0.015f,  165.0f,  40.0f, 0.75f, 0.55f,  0.2f, 0.0f }, // tabor
        { kDmClap,    0.100f, 0.100f, 0.010f,  300.0f,   0.0f, 0.90f, 1.10f,  0.0f, 0.0f }, // hand clap
        { kDmSnare,   0.040f, 0.020f, 0.006f,  620.0f, 300.0f, 0.90f, 0.40f,  1.0f, 0.0f }, // darbuka tek
        { kDmKick,    0.280f, 0.010f, 0.030f,   95.0f,  40.0f, 0.95f, 0.20f, -0.6f, 0.0f }, // darbuka doum
        { kDmMetal,   0.010f, 0.070f, 0.010f, 2900.0f,   0.0f, 0.80f, 0.70f,  0.0f, 0.0f }, // tambourine shake
        { kDmTom,     0.200f, 0.004f, 0.040f,  140.0f,  50.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // low tabor
        { kDmMetal,   0.010f, 0.300f, 0.010f, 2900.0f,   0.0f, 0.80f, 0.70f,  0.0f, 0.0f }, // tambourine hit
        { kDmTom,     0.140f, 0.004f, 0.030f,  210.0f,  80.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // nakers
        { kDmBell,    1.600f, 0.500f, 0.010f, 1420.0f,   0.0f, 0.45f, 0.00f,  0.0f, 0.0f }, // finger cymbals
        { kDmBell,    1.400f, 0.400f, 0.010f,  880.0f,   0.0f, 0.38f, 0.00f,  0.0f, 0.0f }  // hand bell
    },
    { // WAR DRUMS: huge low drums, timpani, gong
        { kDmSnare,   0.900f, 0.120f, 0.120f,   42.0f,  40.0f, 0.55f, 0.30f, -0.8f, 0.0f }, // great drum
        { kDmTom,     0.030f, 0.004f, 0.004f, 1500.0f, 200.0f, 0.90f, 0.00f,  0.0f, 0.0f }, // rim click
        { kDmSnare,   0.120f, 0.250f, 0.015f,  210.0f,  50.0f, 0.60f, 0.70f,  0.3f, 0.0f }, // field snare
        { kDmClap,    0.200f, 0.100f, 0.010f,  300.0f,   0.0f, 0.90f, 1.10f,  0.0f, 0.0f }, // massed clap
        { kDmSnare,   0.350f, 0.050f, 0.040f,  120.0f,  40.0f, 0.85f, 0.30f, -0.5f, 0.0f }, // high war drum
        { kDmTom,     1.200f, 0.004f, 0.200f,   65.0f,   8.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // low timpani
        { kDmMetal,   0.010f, 0.120f, 0.010f, 1800.0f,   0.0f, 0.80f, 0.60f,  0.0f, 0.0f }, // chains
        { kDmTom,     1.000f, 0.004f, 0.200f,   87.0f,   8.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // mid timpani
        { kDmMetal,   0.010f, 1.800f, 0.010f,  120.0f,   0.0f, 1.00f, 0.50f,  0.0f, 0.0f }, // gong
        { kDmTom,     0.900f, 0.004f, 0.200f,  110.0f,   8.0f, 0.80f, 0.00f,  0.0f, 0.0f }, // high timpani
        { kDmHat,     0.010f, 1.400f, 0.010f,  300.0f,   0.0f, 0.00f, 0.90f,  0.0f, 0.0f }, // crash
        { kDmBell,    0.600f, 0.200f, 0.010f, 1100.0f,   0.0f, 0.55f, 0.00f,  0.0f, 0.0f }  // anvil
    },
    { // HAND DRUMS: congas, bongos, djembe, shakers, claves
        { kDmTom,     0.320f, 0.004f, 0.015f,  170.0f,  40.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // low conga
        { kDmTom,     0.040f, 0.004f, 0.002f, 2500.0f,   0.0f, 0.90f, 0.00f,  0.0f, 0.0f }, // claves
        { kDmSnare,   0.060f, 0.035f, 0.008f,  340.0f,  60.0f, 0.80f, 0.60f,  0.8f, 0.0f }, // conga slap
        { kDmSnare,   0.010f, 0.025f, 0.004f, 1800.0f,   0.0f, 0.30f, 1.00f,  1.0f, 0.0f }, // finger snap
        { kDmTom,     0.250f, 0.004f, 0.012f,  250.0f,  40.0f, 1.10f, 0.00f,  0.0f, 0.0f }, // open conga
        { kDmKick,    0.300f, 0.010f, 0.030f,   75.0f,  60.0f, 1.00f, 0.30f, -0.6f, 0.0f }, // djembe bass
        { kDmShaker,  0.025f, 0.080f, 0.010f,    0.0f,   0.0f, 0.00f, 1.30f,  0.0f, 0.0f }, // shaker
        { kDmTom,     0.120f, 0.004f, 0.008f,  390.0f,  80.0f, 1.00f, 0.00f,  0.0f, 0.0f }, // low bongo
        { kDmShaker,  0.040f, 0.220f, 0.010f,    0.0f,   0.0f, 0.00f, 1.30f,  0.0f, 0.0f }, // long shake
        { kDmTom,     0.100f, 0.004f, 0.008f,  520.0f, 100.0f, 0.85f, 0.00f,  0.0f, 0.0f }, // high bongo
        { kDmMetal,   0.010f, 0.180f, 0.010f, 3500.0f,   0.0f, 0.60f, 0.80f,  0.0f, 0.0f }, // cabasa
        { kDmBell,    0.350f, 0.120f, 0.010f,  960.0f,   0.0f, 0.50f, 0.00f,  0.0f, 0.0f }  // agogo
    }
};


} // namespace

RetroSynthEngine::RetroSynthEngine() : songs_(demoSongs()) {
    for (int i = 0; i < kNumParams; ++i) { params_[i] = 0.5f; sm_[i] = 0.5f; }
    // Preset loads skip the live controls, so give them real defaults here
    // (a 0.5 sustain value would read as a held pedal).
    params_[kParamPitchBend] = 0.5f;
    params_[kParamSustain] = 0.0f;
    params_[kParamVelSens] = kDefaultVelSens;
    params_[kParamTouch] = listToNorm(kTouchNormal, kNumTouchCurves);
    applyPreset(0);
    for (int i = 0; i < kNumParams; ++i) sm_[i] = params_[i];
    prepare(48000.0, 512);
}

void RetroSynthEngine::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = (sampleRate > 8000.0) ? sampleRate : 48000.0;

    int chorusLen = static_cast<int>(sampleRate_ * 0.060) + 64;
    chorusBufL_.assign(chorusLen, 0.0f);
    chorusBufR_.assign(chorusLen, 0.0f);
    chorusWriteIdx_ = 0;

    int tapeLen = static_cast<int>(sampleRate_ * 0.040) + 64;
    tapeBufL_.assign(tapeLen, 0.0f);
    tapeBufR_.assign(tapeLen, 0.0f);
    tapeWriteIdx_ = 0;

    int delayLen = static_cast<int>(sampleRate_ * 2.0) + 64;
    delayBufL_.assign(delayLen, 0.0f);
    delayBufR_.assign(delayLen, 0.0f);
    delayWriteIdx_ = 0;

    const double scale = sampleRate_ / 44100.0 * 1.65; // room for the largest reverb type
    static const int kCombTunings[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static const int kAllpassTunings[4] = { 556, 441, 341, 225 };
    for (int i = 0; i < 8; ++i) {
        combsL_[i].init(static_cast<int>(kCombTunings[i] * scale) + 8);
        combsR_[i].init(static_cast<int>((kCombTunings[i] + 23) * scale) + 8);
    }
    for (int i = 0; i < 4; ++i) {
        allpassL_[i].init(static_cast<int>(kAllpassTunings[i] * scale) + 8);
        allpassR_[i].init(static_cast<int>((kAllpassTunings[i] + 19) * scale) + 8);
    }
    preDelayBuf_.assign(static_cast<int>(sampleRate_ * 0.060) + 16, 0.0f);
    preDelayIdx_ = 0;
    reverbTypeApplied_ = -1;

    ksSize_ = 1024;
    while (ksSize_ < static_cast<int>(sampleRate_ / 20.0) + 16) ksSize_ <<= 1;
    ksBuf_.assign(static_cast<size_t>(ksSize_) * kMaxVoices * 2, 0.0f);

    // Handheld speaker: 2-pole highpass at 420 Hz into a slightly resonant 2-pole lowpass at 4.2 kHz
    auto svf = [&](double hz, float k, float* a) {
        float g = static_cast<float>(std::tan(kPi * hz / sampleRate_));
        a[0] = 1.0f / (1.0f + g * (g + k));
        a[1] = g * a[0];
        a[2] = g * a[1];
    };
    svf(420.0, 1.2f, spkHpA_);
    svf(4200.0, 0.9f, spkLpA_);

    smoothCoef_ = static_cast<float>(1.0 - std::exp(-(kCtrlInterval / sampleRate_) / 0.008));
    ctrlCount_ = 0;
}

void RetroSynthEngine::reset() {
    allNotesOff();
    demoActive_ = false;
    demoSampleCounter_ = 0.0;
    demoStep_ = 0;
    for (int& p : demoPrev_) p = -1;

    std::fill(chorusBufL_.begin(), chorusBufL_.end(), 0.0f);
    std::fill(chorusBufR_.begin(), chorusBufR_.end(), 0.0f);
    std::fill(tapeBufL_.begin(), tapeBufL_.end(), 0.0f);
    std::fill(tapeBufR_.begin(), tapeBufR_.end(), 0.0f);
    std::fill(delayBufL_.begin(), delayBufL_.end(), 0.0f);
    std::fill(delayBufR_.begin(), delayBufR_.end(), 0.0f);
    std::fill(preDelayBuf_.begin(), preDelayBuf_.end(), 0.0f);
    for (int i = 0; i < 8; ++i) { combsL_[i].clear(); combsR_[i].clear(); }
    for (int i = 0; i < 4; ++i) { allpassL_[i].clear(); allpassR_[i].clear(); }
    for (int ch = 0; ch < 2; ++ch) {
        dcX_[ch] = dcY_[ch] = 0.0f;
        phFb_[ch] = 0.0f;
        for (int i = 0; i < 4; ++i) phZ_[ch][i] = 0.0f;
        spkHp_[ch][0] = spkHp_[ch][1] = spkLp_[ch][0] = spkLp_[ch][1] = 0.0f;
    }
    delayLpL_ = delayLpR_ = tapeLpL_ = tapeLpR_ = 0.0f;
    crushHoldL_ = crushHoldR_ = 0.0f;
    ctrlCount_ = 0;
}

void RetroSynthEngine::setParameterNormalized(int paramId, float normValue) {
    if (paramId < 0 || paramId >= kNumParams) return;
    normValue = std::clamp(normValue, 0.0f, 1.0f);
    float prev = params_[paramId];
    params_[paramId] = normValue;

    // Pedal up: let go of everything the pedal was holding
    if (paramId == kParamSustain && prev >= 0.5f && normValue < 0.5f) {
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].active && voices_[i].sustained) {
                voices_[i].sustained = false;
                voices_[i].releasing = true;
                voices_[i].envStage = 4;
            }
        }
    }
}

float RetroSynthEngine::getParameterNormalized(int paramId) const {
    if (paramId < 0 || paramId >= kNumParams) return 0.0f;
    return params_[paramId];
}

void RetroSynthEngine::applyPreset(int presetIndex) {
    const FactoryPreset& p = factoryPreset(presetIndex);
    for (int i = 0; i < kNumParams; ++i) {
        if (isPerformanceParam(i) && i != kParamPreset) continue; // live controller / keyboard state
        params_[i] = p.values[i];
    }
    currentEra_ = p.era;
}

void RetroSynthEngine::setEra(int era) {
    currentEra_ = std::clamp(era, 0, kNumEras - 1);
}

void RetroSynthEngine::setHostTempo(double bpm) {
    if (bpm >= 30.0 && bpm <= 400.0) hostTempo_ = bpm;
}

void RetroSynthEngine::setupDrum(Voice& v) {
    v.drumType = ((v.midiNote % 12) + 12) % 12;
    // The drum oscillator has no harmony, so HARM picks its kit
    const int kit = normToList(params_[kParamHarmony], kNumDrumKits);
    const DrumSpec& d = kDrumKits[kit][v.drumType];
    const double decayScale = std::clamp(static_cast<double>(contourScale_), 0.25, 4.0);
    auto coef = [&](float tau, double scale) {
        return static_cast<float>(std::exp(-1.0 / (tau * scale * sampleRate_)));
    };
    v.dAmpA = 1.0f;
    v.dAmpB = 1.0f;
    v.dPitch = 1.0f;
    v.dCoefA = coef(d.tauA, decayScale);
    v.dCoefB = coef(d.tauB, decayScale);
    v.dCoefP = coef(d.tauP, 1.0);
    v.dBaseHz = d.baseHz;
    v.dRangeHz = d.rangeHz;
    v.drumModel = d.model;
    v.dGainT = d.gainT;
    v.dGainN = d.gainN;
    v.dColor = d.color;
    v.dAux = d.aux;
    v.dHp = 0.0f;
    for (double& ph : v.dPh) ph = 0.0;
    v.dClock = 0.0;
    v.dLfsr = 1u;
    v.phase1 = 0.0;
    v.phase2 = 0.0;
    v.lpA = 0.0f;
}

void RetroSynthEngine::exciteString(Voice& v, int voiceIndex, double freq1, double freq2) {
    const int mask = ksSize_ - 1;
    const float bright = std::clamp(0.20f + 0.60f * params_[kParamDynamics] + 0.25f * v.velocity, 0.0f, 1.0f);
    const float lpCoef = 0.08f + 0.90f * bright * bright;
    const double freqs[2] = { freq1, freq2 };
    for (int line = 0; line < 2; ++line) {
        float* buf = ksLine(voiceIndex, line);
        std::fill(buf, buf + ksSize_, 0.0f);
        const int len = std::clamp(static_cast<int>(sampleRate_ / std::max(20.0, freqs[line])) + 1, 4, ksSize_ - 4);
        const int base = v.ksWrite - len + ksSize_;
        const int pick = std::max(1, static_cast<int>(len * 0.17));
        float lp = 0.0f;
        for (int i = 0; i < len; ++i) {
            lp += lpCoef * (whiteNoise(v.rng) - lp);
            buf[(base + i) & mask] = lp;
        }
        // Plucking point comb, then normalise the burst
        for (int i = len - 1; i >= pick; --i) buf[(base + i) & mask] -= 0.7f * buf[(base + i - pick) & mask];
        float peak = 1e-6f;
        for (int i = 0; i < len; ++i) peak = std::max(peak, std::fabs(buf[(base + i) & mask]));
        const float norm = 1.0f / peak;
        for (int i = 0; i < len; ++i) buf[(base + i) & mask] *= norm;
    }
    v.ksPrev1 = v.ksPrev2 = 0.0f;
}

void RetroSynthEngine::noteOn(int pitch, float velocity, int noteId, bool applyTouch) {
    if (pitch < 0 || pitch > 127) return;
    if (velocity <= 0.0001f) {
        noteOff(pitch, noteId);
        return;
    }

    // Same pitch retriggers its voice; otherwise a free voice; otherwise steal the quietest release, then the oldest
    int chosen = -1;
    for (int i = 0; i < kMaxVoices; ++i) {
        if (voices_[i].active && voices_[i].midiNote == pitch) { chosen = i; break; }
    }
    if (chosen < 0) {
        for (int i = 0; i < kMaxVoices; ++i) {
            if (!voices_[i].active) { chosen = i; break; }
        }
    }
    if (chosen < 0) {
        float quietest = 2.0f;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].releasing && voices_[i].envLevel < quietest) {
                quietest = voices_[i].envLevel;
                chosen = i;
            }
        }
    }
    if (chosen < 0) {
        uint32_t maxAge = 0;
        chosen = 0;
        for (int i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].age >= maxAge) { maxAge = voices_[i].age; chosen = i; }
        }
    }

    for (int i = 0; i < kMaxVoices; ++i) {
        if (voices_[i].active) voices_[i].age++;
    }

    const int wave = normToList(params_[kParamWaveform], kNumWaveforms);
    Voice& v = voices_[chosen];
    if (!v.active) {
        v.phase1 = 0.0;
        v.phase2 = 0.25;
        v.phase3 = 0.50;
        v.subPhase = 0.0;
        v.ic1eq = v.ic2eq = 0.0f;
        v.fmPrev = 0.0f;
        v.lpA = v.lpB = 0.0f;
        v.envLevel = 0.0f;
        for (auto& z : v.fz) z[0] = z[1] = 0.0f;
        v.lfsr = 0x7FFFu;
        v.noisePhase = 0.0;
    }
    v.active = true;
    v.releasing = false;
    v.sustained = false;
    v.midiNote = pitch;
    v.noteId = noteId;
    {
        // Touch: the curve reshapes how hard you have to play, Velocity Sens sets how much it matters.
        // Normal curve at the default 65% reproduces the original response exactly.
        const float raw = std::clamp(velocity, 0.0f, 1.0f);
        float curved = raw;
        float sens = std::clamp(params_[kParamVelSens], 0.0f, 1.0f);
        if (applyTouch) {
            switch (normToList(params_[kParamTouch], kNumTouchCurves)) {
                case kTouchOff:   sens = 0.0f; break;
                case kTouchLight: curved = std::pow(raw, 0.55f); break;
                case kTouchHeavy: curved = std::pow(raw, 1.8f); break;
                default: break;
            }
        } else {
            sens = kDefaultVelSens;
        }
        const float toneSens = std::min(1.0f, sens / kDefaultVelSens);
        v.ampVel = 1.0f - sens * (1.0f - curved);
        v.velocity = std::clamp(1.0f - toneSens * (1.0f - curved), 0.05f, 1.0f);
    }
    v.age = 0;
    v.noteTime = 0.0;
    v.arpTimer = 0.0;
    v.arpStep = 0;
    v.envStage = 1;
    v.fmEnvFast = 1.0f;
    v.fmEnvSlow = 1.0f;
    v.rng = v.rng * 1664525u + 1013904223u + static_cast<uint32_t>(pitch * 7919 + chosen);
    v.driftSemis = whiteNoise(v.rng) * 0.035f;
    v.pwmPhase = 0.5 + 0.5 * whiteNoise(v.rng);
    v.pan = std::clamp(0.5f + (pitch - 64) * 0.008f + ((chosen & 1) ? 0.06f : -0.06f), 0.15f, 0.85f);

    const float glide = params_[kParamGlide];
    v.glideSemis = (glide > 0.01f && lastNote_ >= 0 && wave != kWaveDrums)
        ? static_cast<float>(lastNote_ - pitch) : 0.0f;
    lastNote_ = pitch;

    const float sw = (params_[kParamSweep] - 0.5f) * 2.0f;
    v.sweepStart = (std::fabs(sw) < 0.02f || wave == kWaveDrums) ? 0.0f : (sw > 0.0f ? 1.0f : -1.0f) * sw * sw * 48.0f;

    if (wave == kWaveDrums) {
        setupDrum(v);
        v.envLevel = 1.0f;
        v.envStage = 2;
    } else {
        v.drumType = -1;
    }

    if (wave == kWaveLute) {
        const int harm = normToList(params_[kParamHarmony], kNumHarmonyModes);
        const double f1 = 440.0 * std::pow(2.0, (pitch + v.sweepStart + v.glideSemis - 69.0) / 12.0);
        double ratio = 1.003;
        if (harm == kHarmFifth) ratio = 1.4983;
        else if (harm == kHarmOctave) ratio = 2.0;
        else if (harm == kHarmOrganum) ratio = 0.7492;
        exciteString(v, chosen, f1, f1 * ratio);
    }

    ctrlCount_ = 0; // run a control update before the next sample so the voice starts in tune
}

void RetroSynthEngine::noteOff(int pitch, int noteId) {
    bool pedalDown = (params_[kParamSustain] >= 0.5f);
    for (int i = 0; i < kMaxVoices; ++i) {
        Voice& v = voices_[i];
        if (!v.active || v.releasing || v.drumType >= 0) continue; // drums are one-shots
        if (v.midiNote == pitch || (noteId >= 0 && v.noteId == noteId)) {
            if (pedalDown) {
                v.sustained = true;
            } else {
                v.releasing = true;
                v.envStage = 4;
            }
        }
    }
}

void RetroSynthEngine::allNotesOff() {
    for (int i = 0; i < kMaxVoices; ++i) {
        voices_[i].active = false;
        voices_[i].releasing = false;
        voices_[i].sustained = false;
        voices_[i].envStage = 0;
        voices_[i].envLevel = 0.0f;
        voices_[i].drumType = -1;
    }
    lastNote_ = -1;
}

void RetroSynthEngine::releaseDemoNotes() {
    for (int i = 0; i < kMaxVoices; ++i) {
        Voice& v = voices_[i];
        if (v.active && v.noteId >= 9901 && v.noteId < 9901 + kDemoTracks) {
            v.releasing = true;
            v.sustained = false;
            v.envStage = 4;
            if (v.drumType >= 0) v.active = false;
        }
    }
    for (int& p : demoPrev_) p = -1;
}

void RetroSynthEngine::updateActiveNoteMask(SharedUITelemetry* telemetry) {
    if (!telemetry) return;
    uint64_t low = 0;
    uint64_t high = 0;
    int count = 0;
    for (int i = 0; i < kMaxVoices; ++i) {
        const Voice& v = voices_[i];
        if (!v.active) continue;
        if (v.envLevel > 0.01f) ++count;
        const bool held = (v.drumType >= 0) ? (v.noteTime < 0.12) : !v.releasing;
        if (held) {
            int n = v.midiNote;
            if (n >= 0 && n < 64) low |= (1ULL << n);
            else if (n >= 64 && n < 128) high |= (1ULL << (n - 64));
        }
    }
    telemetry->activeNotesLow.store(low, std::memory_order_relaxed);
    telemetry->activeNotesHigh.store(high, std::memory_order_relaxed);
    telemetry->activeVoices.store(count, std::memory_order_relaxed);
}

void RetroSynthEngine::applyReverbType(int type) {
    // Line-length scale, pre-delay (seconds)
    static const float kScale[kNumReverbTypes]    = { 0.55f, 1.00f, 1.55f, 1.30f };
    static const float kPreDelay[kNumReverbTypes] = { 0.004f, 0.015f, 0.036f, 0.024f };
    static const int kCombTunings[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static const int kAllpassTunings[4] = { 556, 441, 341, 225 };
    type = std::clamp(type, 0, kNumReverbTypes - 1);
    const double scale = sampleRate_ / 44100.0 * kScale[type];
    for (int i = 0; i < 8; ++i) {
        combsL_[i].setLen(static_cast<int>(kCombTunings[i] * scale));
        combsR_[i].setLen(static_cast<int>((kCombTunings[i] + 23) * scale));
    }
    for (int i = 0; i < 4; ++i) {
        allpassL_[i].setLen(static_cast<int>(kAllpassTunings[i] * scale));
        allpassR_[i].setLen(static_cast<int>((kAllpassTunings[i] + 19) * scale));
    }
    preDelayLen_ = std::clamp(static_cast<int>(kPreDelay[type] * sampleRate_), 16, static_cast<int>(preDelayBuf_.size()));
    if (preDelayIdx_ >= preDelayLen_) preDelayIdx_ = 0;
    reverbTypeApplied_ = type;
}

void RetroSynthEngine::updateVoiceControl(Voice& v, int voiceIndex) {
    const double invSr = 1.0 / sampleRate_;
    const double dtc = kCtrlInterval * invSr;
    const float dyn = sm_[kParamDynamics];

    if (v.drumType >= 0) {
        // Drums run their own pitch and amp envelopes per sample; only the filter is set here
        const float g = static_cast<float>(std::tan(kPi * std::min(90.0 * std::pow(200.0, static_cast<double>(std::clamp(sm_[kParamCutoff], 0.03f, 0.995f))), sampleRate_ * 0.44) * invSr));
        v.fk = 2.0f - 1.85f * std::clamp(sm_[kParamResonance], 0.0f, 0.95f);
        v.fa1 = 1.0f / (1.0f + g * (g + v.fk));
        v.fa2 = g * v.fa1;
        v.fa3 = g * v.fa2;
        v.noteTime += dtc;
        return;
    }

    // Tracker arpeggio
    int pitchOffset = 0;
    if (harmMode_ >= kHarmArpMajor) {
        v.arpTimer += dtc;
        while (v.arpTimer >= arpPeriod_) { v.arpTimer -= arpPeriod_; ++v.arpStep; }
        static const int kMajor[4] = { 0, 4, 7, 12 };
        static const int kMinor[4] = { 0, 3, 7, 12 };
        switch (harmMode_) {
            case kHarmArpMajor:  pitchOffset = kMajor[v.arpStep & 3]; break;
            case kHarmArpMinor:  pitchOffset = kMinor[v.arpStep & 3]; break;
            case kHarmArpOctave: pitchOffset = (v.arpStep & 1) ? 12 : 0; break;
            default:             pitchOffset = (v.arpStep >= 1) ? 5 : 0; break; // coin: one step up, then hold
        }
    }

    v.glideSemis *= glideCoef_;

    float sweep = 0.0f;
    if (v.sweepStart != 0.0f) {
        const double x = 1.0 - v.noteTime / sweepSec_;
        if (x > 0.0) sweep = v.sweepStart * static_cast<float>(x * x);
    }

    const float vibFade = std::clamp(static_cast<float>((v.noteTime - 0.10) / 0.22), 0.0f, 1.0f);
    const double note = v.midiNote + pitchOffset + v.glideSemis + sweep + bendSemis_
                      + vibLfo_ * vibDepth_ * vibFade + v.driftSemis;
    const double f1 = 440.0 * std::pow(2.0, (note - 69.0) / 12.0);

    const double dyadDetune = 1.0 + (detuneRatio_ - 1.0) * 0.3;
    double f2 = f1 * detuneRatio_;
    if (harmMode_ == kHarmFifth)        f2 = f1 * 1.498307 * dyadDetune;
    else if (harmMode_ == kHarmOctave)  f2 = f1 * 2.0 * dyadDetune;
    else if (harmMode_ == kHarmOrganum) f2 = f1 * 0.749154 * dyadDetune;
    const double f3 = (waveMode_ == kWaveBell) ? f1 * 3.5 : f1 / detuneRatio_;

    v.dt1 = std::min(0.45, f1 * invSr);
    v.dt2 = std::min(0.45, f2 * invSr);
    v.dt3 = std::min(0.45, f3 * invSr);
    v.subDt = v.dt1 * 0.5;

    // Pulse widths
    const float pwmMod = 0.14f * (dyn - 0.5f) + 0.04f * vibLfo_;
    switch (waveMode_) {
        case kWavePulse50:
            v.pwA = std::clamp(0.50f + pwmMod, 0.10f, 0.90f);
            v.pwB = std::clamp(0.50f - pwmMod, 0.10f, 0.90f);
            break;
        case kWavePulse25:
            v.pwA = std::clamp(0.25f + pwmMod * 0.8f, 0.08f, 0.92f);
            v.pwB = std::clamp(0.25f - pwmMod * 0.8f, 0.08f, 0.92f);
            break;
        case kWavePulse12:
            v.pwA = std::clamp(0.125f + pwmMod * 0.5f, 0.05f, 0.95f);
            v.pwB = std::clamp(0.125f - pwmMod * 0.5f, 0.05f, 0.95f);
            break;
        case kWavePwm: {
            v.pwmPhase += (0.2 + 9.0 * dyn * dyn * dyn) * dtc;
            if (v.pwmPhase >= 1.0) v.pwmPhase -= 1.0;
            v.pwA = 0.5f + 0.40f * fsin(v.pwmPhase);
            v.pwB = 0.5f - 0.40f * fsin(v.pwmPhase + 0.19);
            break;
        }
        default: break;
    }

    // Voice lowpass: cutoff follows the fader, Dynamics, the amp envelope and the key
    const float cutNorm = std::clamp(sm_[kParamCutoff] * 0.78f + dyn * 0.22f
                                     + sm_[kParamFilterEnv] * 0.6f * v.envLevel
                                     + (v.midiNote - 60) * 0.0025f, 0.03f, 0.995f);
    const double cutoffHz = std::min(90.0 * std::pow(200.0, static_cast<double>(cutNorm)), sampleRate_ * 0.44);
    const float g = static_cast<float>(std::tan(kPi * cutoffHz * invSr));
    v.fk = 2.0f - 1.85f * std::clamp(sm_[kParamResonance], 0.0f, 0.95f);
    v.fa1 = 1.0f / (1.0f + g * (g + v.fk));
    v.fa2 = g * v.fa1;
    v.fa3 = g * v.fa2;

    v.fmEnvFast *= fmFastCoef_;
    v.fmEnvSlow *= fmSlowCoef_;

    if (waveMode_ == kWaveSoftStr) v.lpCoef = onePoleCoef(std::min(f1 * (2.5 + 5.0 * dyn), 9000.0), sampleRate_);
    else if (waveMode_ == kWaveChoir) v.lpCoef = onePoleCoef(2400.0, sampleRate_);

    if (waveMode_ == kWaveLute) {
        const float bright = std::clamp(0.15f + 0.85f * dyn, 0.0f, 1.0f);
        v.ksBlend = 0.5f - 0.42f * bright;
        const double maxDelay = ksSize_ - 4.0;
        v.ksDelay1 = std::clamp(sampleRate_ / f1 - v.ksBlend, 2.0, maxDelay);
        v.ksDelay2 = std::clamp(sampleRate_ / f2 - v.ksBlend, 2.0, maxDelay);
        const double t60 = v.releasing ? ksT60Rel_ : ksT60Held_;
        v.ksGain1 = static_cast<float>(std::exp(-6.9078 / (std::max(20.0, f1) * t60)));
        v.ksGain2 = static_cast<float>(std::exp(-6.9078 / (std::max(20.0, f2) * t60)));
    }

    v.noteTime += dtc;
    (void)voiceIndex;
}

void RetroSynthEngine::updateControl() {
    const double invSr = 1.0 / sampleRate_;
    const double dtc = kCtrlInterval * invSr;

    for (int i = 0; i < kNumParams; ++i) sm_[i] += smoothCoef_ * (params_[i] - sm_[i]);

    waveMode_ = normToList(params_[kParamWaveform], kNumWaveforms);
    harmMode_ = normToList(params_[kParamHarmony], kNumHarmonyModes);
    const float dyn = sm_[kParamDynamics];

    // Envelope: A/D/S/R with the Contour macro scaling decay + release (and stretching slow attacks)
    contourScale_ = std::pow(4.0f, 2.0f * sm_[kParamEnvShape] - 1.0f);
    const float a = sm_[kParamAttack], d = sm_[kParamDecay], r = sm_[kParamRelease];
    const double attackSec = (0.001 + 2.0 * a * a * a) * (contourScale_ > 1.0f ? std::sqrt(contourScale_) : 1.0f);
    const double decayTau = (0.015 + 3.0 * d * d * d) * contourScale_;
    const double releaseTau = (0.012 + 2.5 * r * r * r) * contourScale_;
    attackStep_ = static_cast<float>(1.0 / (attackSec * sampleRate_));
    decayCoef_ = static_cast<float>(std::exp(-1.0 / (decayTau * sampleRate_)));
    releaseCoef_ = static_cast<float>(std::exp(-1.0 / (releaseTau * sampleRate_)));
    sustainLvl_ = (waveMode_ == kWaveLute) ? 1.0f : sm_[kParamSustainLevel] * sm_[kParamSustainLevel];
    ksT60Held_ = (0.5 + 10.0 * d * d) * contourScale_;
    ksT60Rel_ = (0.05 + 2.0 * r * r) * contourScale_;

    // Pitch motion
    vibLfoPhase_ += 5.6 * dtc;
    if (vibLfoPhase_ >= 1.0) vibLfoPhase_ -= 1.0;
    vibLfo_ = fsin(vibLfoPhase_);
    vibDepth_ = 0.6f * sm_[kParamVibrato] * sm_[kParamVibrato];
    bendSemis_ = (sm_[kParamPitchBend] - 0.5f) * 4.0f;
    arpPeriod_ = 1.0 / (4.0 * std::pow(16.0, static_cast<double>(params_[kParamArpRate])));
    const float glide = params_[kParamGlide];
    glideCoef_ = (glide > 0.01f) ? static_cast<float>(std::exp(-dtc / (0.002 + 0.4 * glide * glide))) : 0.0f;
    const float st = params_[kParamSweepTime];
    sweepSec_ = 0.01 + 1.5 * st * st;
    const float det = sm_[kParamDetune];
    detuneRatio_ = std::pow(2.0, (1.0 + 30.0 * det * det) / 1200.0);
    fmFastCoef_ = static_cast<float>(std::exp(-dtc / 0.09));
    fmSlowCoef_ = static_cast<float>(std::exp(-dtc / 1.1));
    noiseLevel_ = sm_[kParamNoise] * sm_[kParamNoise] * 0.8f;

    // Handheld wavetable: Dynamics morphs through the four tables
    if (waveMode_ == kWaveGbWave) {
        const float pos = std::clamp(dyn, 0.0f, 1.0f) * 3.0f;
        const int t0 = std::min(2, static_cast<int>(pos));
        const float f = pos - t0;
        for (int i = 0; i < 32; ++i) gbTable_[i] = gT.gb[t0][i] * (1.0f - f) + gT.gb[t0 + 1][i] * f;
    }

    // Formant bank for the voice-like waves
    if (waveMode_ == kWaveChoir || waveMode_ == kWaveReed || waveMode_ == kWaveBowed) {
        float freq[3], q[3], gain[3];
        if (waveMode_ == kWaveChoir) {
            freq[0] = 350.0f + 370.0f * dyn; freq[1] = 800.0f + 350.0f * dyn; freq[2] = 2400.0f + 250.0f * dyn;
            q[0] = 7.0f; q[1] = 9.0f; q[2] = 10.0f;
            gain[0] = 1.0f; gain[1] = 0.50f + 0.35f * dyn; gain[2] = 0.30f;
            formDry_ = 0.0f;
        } else if (waveMode_ == kWaveReed) {
            freq[0] = 1250.0f; freq[1] = 2900.0f; freq[2] = 4200.0f;
            q[0] = 4.0f; q[1] = 5.0f; q[2] = 5.0f;
            gain[0] = 1.0f; gain[1] = 0.40f + 0.50f * dyn; gain[2] = 0.25f * dyn;
            formDry_ = 0.45f;
        } else {
            freq[0] = 290.0f; freq[1] = 540.0f; freq[2] = 2500.0f;
            q[0] = 2.5f; q[1] = 3.0f; q[2] = 3.0f;
            gain[0] = 0.90f; gain[1] = 0.80f; gain[2] = 0.25f + 0.45f * dyn;
            formDry_ = 0.35f;
        }
        for (int i = 0; i < 3; ++i) {
            const float g = static_cast<float>(std::tan(kPi * std::min<double>(freq[i], sampleRate_ * 0.45) * invSr));
            const float k = 1.0f / q[i];
            formK_[i] = k;
            formA1_[i] = 1.0f / (1.0f + g * (g + k));
            formA2_[i] = g * formA1_[i];
            formA3_[i] = g * formA2_[i];
            formGain_[i] = gain[i] * k; // k normalises the band-pass peak to unity
        }
    }

    for (int vi = 0; vi < kMaxVoices; ++vi) {
        if (voices_[vi].active) updateVoiceControl(voices_[vi], vi);
    }

    // ---- Effects ----
    const float drive = sm_[kParamDrive];
    driveGain_ = 1.0f + 14.0f * drive * drive;
    driveBias_ = 0.12f * drive;
    driveBiasOut_ = std::tanh(driveBias_);
    driveNorm_ = 0.35f / std::tanh(0.35f * driveGain_);

    const float crush = sm_[kParamCrush];
    if (crush > 0.02f) {
        crushStepRate_ = std::pow(1.0 - 0.88 * crush, 2.2) * (48000.0 / sampleRate_);
        crushLevels_ = std::pow(2.0f, 12.0f - 8.5f * crush);
        crushMix_ = std::min(1.0f, crush * 1.15f);
    } else {
        crushStepRate_ = 1.0;
        crushLevels_ = 0.0f;
        crushMix_ = 0.0f;
    }

    const float phaser = sm_[kParamPhaser];
    phLfoPhase_ += (0.30 + 0.45 * phaser) * dtc;
    if (phLfoPhase_ >= 1.0) phLfoPhase_ -= 1.0;
    for (int ch = 0; ch < 2; ++ch) {
        const float lfo = 0.5f + 0.5f * fsin(phLfoPhase_ + ch * 0.25);
        const double hz = 350.0 * std::pow(2.0, 2.6 * lfo);
        const float t = static_cast<float>(std::tan(kPi * hz * invSr));
        phCoef_[ch] = (t - 1.0f) / (t + 1.0f);
    }

    const float tape = sm_[kParamTape];
    tapeLpCoef_ = onePoleCoef(16000.0 * std::pow(0.28, static_cast<double>(tape)), sampleRate_);
    tremRate_ = 3.0f + 4.5f * sm_[kParamTremolo];

    // Delay: times follow the demo song, else host tempo
    {
        const float amt = sm_[kParamDelay];
        const int type = normToList(params_[kParamDelayType], kNumDelayTypes);
        double bpm = demoActive_ ? (15.0 / songs_[demoSongIdx_].stepSec) : hostTempo_;
        bpm = std::clamp(bpm, 40.0, 300.0);
        const double beat = 60.0 / bpm;
        double tL = 0.5 * beat, tR = 0.75 * beat, lpHz = 9000.0;
        delayCross_ = true;
        delaySat_ = false;
        switch (type) {
            case kDlyTape:
                tL = 0.75 * beat; tR = tL + 0.011; delayCross_ = false; delaySat_ = true;
                delayFb_ = 0.30f + 0.42f * amt; lpHz = 2800.0; delayWet_ = 0.55f * amt;
                break;
            case kDly16Bit:
                tL = 0.375 * beat; tR = tL + 0.007; delayCross_ = false;
                delayFb_ = 0.32f + 0.40f * amt; lpHz = 4200.0; delayWet_ = 0.50f * amt;
                break;
            case kDlySlap:
                tL = 0.095; tR = 0.118; delayCross_ = false;
                delayFb_ = 0.06f; lpHz = 6500.0; delayWet_ = 0.70f * amt;
                break;
            case kDlyCanyon:
                tL = beat; tR = 1.5 * beat;
                delayFb_ = (0.35f + 0.37f * amt) * 0.75f; lpHz = 2400.0; delayWet_ = 0.50f * amt;
                break;
            default: // ping-pong: eighth / dotted eighth
                delayFb_ = (0.25f + 0.35f * amt) * 0.75f; delayWet_ = 0.50f * amt;
                break;
        }
        const int maxSamples = static_cast<int>(delayBufL_.size()) - 8;
        delaySamplesL_ = std::clamp(static_cast<int>(tL * sampleRate_), 16, maxSamples);
        delaySamplesR_ = std::clamp(static_cast<int>(tR * sampleRate_), 16, maxSamples);
        delayLpCoef_ = onePoleCoef(lpHz, sampleRate_);
    }

    // Reverb
    {
        const int type = normToList(params_[kParamReverbType], kNumReverbTypes);
        if (type != reverbTypeApplied_) applyReverbType(type);
        const float amt = sm_[kParamReverb];
        switch (type) {
            case kRevRoom:   revFeedback_ = 0.62f + 0.18f * amt;  revDamp_ = 0.35f; break;
            case kRevChurch: revFeedback_ = 0.88f + 0.085f * amt; revDamp_ = 0.15f; break;
            case kRevCave:   revFeedback_ = 0.84f + 0.10f * amt;  revDamp_ = 0.55f; break;
            default:         revFeedback_ = 0.78f + 0.14f * amt;  revDamp_ = 0.25f; break;
        }
        revWet_ = amt * 0.42f * std::sqrt(1.0f - revFeedback_ * revFeedback_);
    }
}

void RetroSynthEngine::renderBlock(float* outL, float* outR, int numSamples, SharedUITelemetry* telemetry) {
    // Flush denormals for the duration of the block
    const unsigned int savedCsr = _mm_getcsr();
    _mm_setcsr(savedCsr | 0x8040u);

    if (telemetry) {
        int pitch = 0, vel = 0;
        while (telemetry->popGuiNote(pitch, vel)) {
            if (vel > 0) {
                noteOn(pitch, vel / 127.0f, 9999);
                telemetry->lastVelocity.store(vel, std::memory_order_relaxed);
            } else {
                noteOff(pitch, 9999);
            }
        }

        const bool reqDemo = (telemetry->demoActive.load(std::memory_order_relaxed) != 0);
        // A song picked in the editor restarts playback with that song
        const uint32_t restart = telemetry->demoRestart.load(std::memory_order_acquire);
        const bool restartReq = (restart != demoRestartSeen_);
        demoRestartSeen_ = restart;
        if (reqDemo && (!demoActive_ || restartReq)) {
            if (demoActive_) releaseDemoNotes();
            demoActive_ = true;
            const int choice = telemetry->demoChoice.load(std::memory_order_relaxed);
            demoSongIdx_ = (choice >= 0 && choice < kNumDemoSongs) ? choice : demoSongForEra(currentEra_);
            demoStep_ = 0;
            demoSampleCounter_ = 0.0;
            for (int& p : demoPrev_) p = -1;
            telemetry->demoSong.store(demoSongIdx_, std::memory_order_relaxed);
        } else if (!reqDemo && demoActive_) {
            demoActive_ = false;
            demoSampleCounter_ = 0.0;
            demoStep_ = 0;
            releaseDemoNotes();
        }
        const double bpm = demoActive_ ? (15.0 / songs_[demoSongIdx_].stepSec) : hostTempo_;
        telemetry->tempoBpm.store(static_cast<float>(bpm), std::memory_order_relaxed);
    }

    const double invSr = 1.0 / sampleRate_;
    const int mask = ksSize_ - 1;
    const int chorusSize = static_cast<int>(chorusBufL_.size());
    const int tapeSize = static_cast<int>(tapeBufL_.size());
    const int delaySize = static_cast<int>(delayBufL_.size());
    const float dcR = static_cast<float>(1.0 - kTwoPi * 5.0 * invSr);
    float blockPeakL = 0.0f;
    float blockPeakR = 0.0f;

    for (int s = 0; s < numSamples; ++s) {
        // ---- Demo sequencer (sample accurate) ----
        if (demoActive_) {
            if (demoSampleCounter_ <= 0.0) {
                const DemoSong& song = songs_[demoSongIdx_];
                const int step = demoStep_ % song.numSteps;
                for (int t = 0; t < kDemoTracks; ++t) {
                    const int val = song.tracks[t][step];
                    if (val == -2) continue; // hold
                    if (demoPrev_[t] >= 0) {
                        noteOff(demoPrev_[t], 9901 + t);
                        demoPrev_[t] = -1;
                    }
                    if (val >= 0) {
                        noteOn(val, song.velocity[t], 9901 + t, false);
                        demoPrev_[t] = val;
                    }
                }
                demoStep_ = (step + 1) % song.numSteps;
                demoSampleCounter_ += song.stepSec * sampleRate_;
            }
            demoSampleCounter_ -= 1.0;
        }

        if (ctrlCount_ == 0) updateControl();
        if (++ctrlCount_ >= kCtrlInterval) ctrlCount_ = 0;

        const int wave = waveMode_;
        const float dyn = sm_[kParamDynamics];
        const float subMix = sm_[kParamSubMix];
        const float detuneBlend = std::min(1.0f, sm_[kParamDetune] * 4.0f);
        const bool dyad = (harmMode_ == kHarmFifth || harmMode_ == kHarmOctave || harmMode_ == kHarmOrganum);
        const float waveGain = kWaveGain[wave];

        float dryL = 0.0f;
        float dryR = 0.0f;

        for (int vi = 0; vi < kMaxVoices; ++vi) {
            Voice& v = voices_[vi];
            if (!v.active) continue;

            float mix = 0.0f; // pre-filter voice signal

            if (v.drumType >= 0) {
                // ---- Chip drum kit: one-shot per note name ----
                v.dAmpA *= v.dCoefA;
                v.dAmpB *= v.dCoefB;
                v.dPitch *= v.dCoefP;
                const double dt = (v.dBaseHz + v.dRangeHz * v.dPitch) * invSr;
                v.phase1 += dt; if (v.phase1 >= 1.0) v.phase1 -= 1.0;
                const float n = whiteNoise(v.rng);
                v.lpA += 0.30f * (n - v.lpA);
                const float hiss = n - v.lpA;
                // Noise colour: 0 white, >0 toward the bright hiss, <0 toward the dark lowpassed noise
                const float ns = (v.dColor == 0.0f) ? n
                               : (v.dColor > 0.0f ? n + v.dColor * (hiss - n) : n - v.dColor * (v.lpA - n));
                float d = 0.0f;
                switch (v.drumModel) {
                    case kDmKick:
                    case kDmSnare:
                    case kDmTom:
                        d = fsin(v.phase1) * v.dAmpA * v.dGainT + ns * v.dAmpB * v.dGainN;
                        break;
                    case kDmRim:
                        d = static_cast<float>(4.0 * std::fabs(v.phase1 - 0.5) - 1.0) * v.dAmpA * v.dGainT;
                        break;
                    case kDmClap: {
                        const double t = v.noteTime;
                        const bool burst = (t < 0.010) || (t > 0.014 && t < 0.024) || (t > 0.028 && t < 0.038);
                        d = hiss * (burst ? v.dGainN : (t >= 0.038 ? v.dAmpA * v.dGainT : 0.0f));
                        break;
                    }
                    case kDmHat:
                        d = hiss * v.dAmpB * v.dGainN;
                        break;
                    case kDmMetal: {
                        static const double kRatio[6] = { 1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637 };
                        float sq = 0.0f;
                        for (int k = 0; k < 6; ++k) {
                            v.dPh[k] += dt * kRatio[k];
                            if (v.dPh[k] >= 1.0) v.dPh[k] -= 1.0;
                            sq += (v.dPh[k] < 0.5) ? 1.0f : -1.0f;
                        }
                        sq *= (1.0f / 6.0f);
                        v.dHp += 0.45f * (sq - v.dHp);
                        d = ((sq - v.dHp) * v.dGainT + hiss * v.dGainN) * v.dAmpB;
                        break;
                    }
                    case kDmNes: {
                        const float tri = static_cast<float>(4.0 * std::fabs(v.phase1 - 0.5) - 1.0);
                        const float tri4 = std::floor(tri * 7.5f + 0.5f) * (1.0f / 7.5f);
                        v.dClock += v.dAux * invSr;
                        while (v.dClock >= 1.0) { v.dClock -= 1.0; lfsrStep(v.dLfsr, v.dColor >= 0.5f); }
                        const float lf = (v.dLfsr & 1u) ? 0.8f : -0.8f;
                        d = tri4 * v.dAmpA * v.dGainT + lf * v.dAmpB * v.dGainN;
                        break;
                    }
                    case kDmBell: {
                        static const double kRatio[4] = { 1.0, 2.76, 5.40, 8.93 };
                        for (int k = 0; k < 4; ++k) {
                            v.dPh[k] += dt * kRatio[k];
                            if (v.dPh[k] >= 1.0) v.dPh[k] -= 1.0;
                        }
                        const float upper = fsin(v.dPh[1]) * 0.6f + fsin(v.dPh[2]) * 0.35f + fsin(v.dPh[3]) * 0.2f;
                        d = (fsin(v.dPh[0]) * v.dAmpA + upper * v.dAmpB) * v.dGainT;
                        break;
                    }
                    case kDmShaker:
                        d = hiss * (1.0f - v.dAmpA) * v.dAmpB * v.dGainN;
                        break;
                    default: { // kDmCowbell
                        v.phase2 += dt * 1.48; if (v.phase2 >= 1.0) v.phase2 -= 1.0;
                        d = ((v.phase1 < 0.5 ? 0.5f : -0.5f) + (v.phase2 < 0.5 ? 0.5f : -0.5f)) * v.dAmpA * v.dGainT;
                        break;
                    }
                }
                mix = d * 1.6f;
                if (v.noteTime > 0.05 && v.dAmpA < 0.0005f && v.dAmpB < 0.0005f) {
                    v.active = false;
                    v.drumType = -1;
                    v.envStage = 0;
                    v.envLevel = 0.0f;
                    continue;
                }
            } else {
                // ---- Amp envelope ----
                if (v.envStage == 1) {
                    v.envLevel += attackStep_;
                    if (v.envLevel >= 1.0f) { v.envLevel = 1.0f; v.envStage = 2; }
                } else if (v.envStage == 2) {
                    v.envLevel = sustainLvl_ + (v.envLevel - sustainLvl_) * decayCoef_;
                    if (sustainLvl_ < 0.001f && v.envLevel < 0.0002f) {
                        v.active = false; v.envStage = 0; v.envLevel = 0.0f;
                        continue;
                    }
                } else if (v.envStage == 4) {
                    v.envLevel *= releaseCoef_;
                    if (v.envLevel < 0.0002f) {
                        v.active = false; v.releasing = false; v.envStage = 0; v.envLevel = 0.0f;
                        continue;
                    }
                }

                v.phase1 += v.dt1; if (v.phase1 >= 1.0) v.phase1 -= 1.0;
                v.phase2 += v.dt2; if (v.phase2 >= 1.0) v.phase2 -= 1.0;
                v.phase3 += v.dt3; if (v.phase3 >= 1.0) v.phase3 -= 1.0;
                v.subPhase += v.subDt; if (v.subPhase >= 1.0) v.subPhase -= 1.0;

                float osc1 = 0.0f, osc2 = 0.0f;
                float blend = dyad ? 0.72f : (0.25f + 0.40f * dyn) * detuneBlend;
                bool formant = false;

                switch (wave) {
                    case kWavePulse50:
                    case kWavePulse25:
                    case kWavePulse12:
                    case kWavePwm:
                        osc1 = pulseBlep(v.phase1, v.dt1, v.pwA);
                        osc2 = pulseBlep(v.phase2, v.dt2, v.pwB);
                        break;
                    case kWaveNesTri:
                        osc1 = nesTri(v.phase1, v.dt1, dyn);
                        osc2 = nesTri(v.phase2, v.dt2, dyn);
                        break;
                    case kWaveGbWave:
                        osc1 = tableWave(gbTable_, v.phase1, v.dt1);
                        osc2 = tableWave(gbTable_, v.phase2, v.dt2);
                        break;
                    case kWaveNoise: {
                        // Pitched 15-bit LFSR; high Dynamics switches to the metallic 7-bit loop
                        v.noisePhase += v.dt1 * 12.0;
                        int guard = 0;
                        while (v.noisePhase >= 1.0 && guard++ < 8) {
                            v.noisePhase -= 1.0;
                            lfsrStep(v.lfsr, dyn > 0.7f);
                            v.noiseVal = (v.lfsr & 1u) ? -1.0f : 1.0f;
                        }
                        if (v.noisePhase >= 1.0) v.noisePhase -= std::floor(v.noisePhase);
                        osc1 = v.noiseVal;
                        blend = 0.0f;
                        break;
                    }
                    case kWaveChipSaw: {
                        const float s1 = sawBlep(v.phase1, v.dt1);
                        const float s2 = sawBlep(v.phase2, v.dt2);
                        const float s3 = sawBlep(v.phase3, v.dt3);
                        osc1 = 0.55f * (s1 + s3);
                        osc2 = s2;
                        break;
                    }
                    case kWaveFm16: {
                        const float idx = (0.4f + 3.2f * dyn) * (0.4f + 0.6f * v.envLevel) * kInv2Pi;
                        const float mod = fsin(v.phase1 * 2.0 + 0.35f * kInv2Pi * v.fmPrev);
                        v.fmPrev = mod;
                        osc1 = fsin(v.phase1 + idx * mod);
                        osc2 = fsin(v.phase2 + idx * 0.8f * mod);
                        break;
                    }
                    case kWaveFmKeys: {
                        const float body = (0.25f + 2.4f * dyn * (0.4f + 0.6f * v.velocity))
                                         * (0.25f + 0.75f * v.fmEnvSlow) * kInv2Pi;
                        const double tineRatio = v.dt1 * 14.0;
                        const float tineFade = tineRatio < 0.30 ? 1.0f
                                             : std::max(0.0f, static_cast<float>((0.45 - tineRatio) / 0.15));
                        const float tineIdx = 1.6f * v.velocity * v.fmEnvFast * tineFade * kInv2Pi;
                        const float tine = fsin(v.phase1 + tineIdx * fsin(v.phase1 * 14.0));
                        osc1 = 0.80f * fsin(v.phase1 + body * fsin(v.phase1))
                             + 0.35f * tine * (0.25f + 0.75f * v.fmEnvFast);
                        osc2 = fsin(v.phase2 + body * fsin(v.phase2));
                        break;
                    }
                    case kWaveFmBass: {
                        const float idx = (0.8f + 7.0f * dyn) * (0.12f + 0.88f * v.fmEnvFast)
                                        * (0.5f + 0.5f * v.velocity) * kInv2Pi;
                        const float mod = fsin(v.phase1 + 0.35f * kInv2Pi * v.fmPrev);
                        v.fmPrev = mod;
                        osc1 = fsin(v.phase1 + idx * mod);
                        osc2 = fsin(v.phase2 + idx * 0.7f * mod);
                        break;
                    }
                    case kWaveBell: {
                        const float idx = (0.6f + 4.5f * dyn) * (0.15f + 0.85f * v.fmEnvSlow) * kInv2Pi;
                        const float mod = fsin(v.phase3); // modulator runs at 3.5x the carrier
                        osc1 = fsin(v.phase1 + idx * mod);
                        osc2 = fsin(v.phase2 + idx * 0.6f * mod);
                        break;
                    }
                    case kWaveSoftStr: {
                        const float s1 = sawBlep(v.phase1, v.dt1);
                        const float s2 = sawBlep(v.phase2, v.dt2);
                        const float s3 = sawBlep(v.phase3, v.dt3);
                        v.lpA += v.lpCoef * (0.5f * (s1 + s3) - v.lpA);
                        v.lpB += v.lpCoef * (s2 - v.lpB);
                        osc1 = v.lpA * 1.5f;
                        osc2 = v.lpB * 1.5f;
                        break;
                    }
                    case kWaveOrgan: {
                        const double p = v.phase1;
                        float o = fsin(p) * 0.60f + fsin(p * 2.0) * 0.42f
                                + fsin(p * 3.0) * (0.12f + 0.30f * dyn)
                                + fsin(p * 4.0) * (0.10f + 0.32f * dyn);
                        if (v.dt1 * 6.0 < 0.45) o += fsin(p * 6.0) * 0.26f * dyn;
                        if (v.dt1 * 8.0 < 0.45) o += fsin(p * 8.0) * 0.20f * dyn;
                        const float chiff = v.fmEnvFast * v.fmEnvFast;
                        osc1 = o * 0.62f + whiteNoise(v.rng) * chiff * 0.10f;
                        osc2 = (fsin(v.phase2) * 0.60f + fsin(v.phase2 * 2.0) * 0.40f + fsin(v.phase2 * 3.0) * 0.15f) * 0.62f;
                        break;
                    }
                    case kWaveFlute: {
                        v.lpA += 0.22f * (whiteNoise(v.rng) - v.lpA);
                        const float chiff = v.fmEnvFast * v.fmEnvFast;
                        const float breath = v.lpA * (0.10f + 0.50f * dyn) * (0.35f + 2.0f * chiff);
                        osc1 = (fsin(v.phase1 + 0.03f * breath) + (0.10f + 0.30f * dyn) * fsin(v.phase1 * 2.0)
                                + 0.07f * fsin(v.phase1 * 3.0) + breath) * 0.85f;
                        osc2 = fsin(v.phase2) * 0.8f;
                        break;
                    }
                    case kWaveReed:
                        osc1 = 0.6f * pulseBlep(v.phase1, v.dt1, 0.16 + 0.12 * dyn) + 0.5f * sawBlep(v.phase1, v.dt1);
                        osc2 = sawBlep(v.phase2, v.dt2) * 0.8f;
                        formant = true;
                        break;
                    case kWaveLute: {
                        float* b1 = ksLine(vi, 0);
                        float* b2 = ksLine(vi, 1);
                        const int w = v.ksWrite;
                        auto readLine = [&](const float* b, double delay) {
                            const double rp = w - delay + ksSize_;
                            const int i0 = static_cast<int>(rp);
                            const float fr = static_cast<float>(rp - i0);
                            return b[i0 & mask] * (1.0f - fr) + b[(i0 + 1) & mask] * fr;
                        };
                        const float y1 = readLine(b1, v.ksDelay1);
                        const float y2 = readLine(b2, v.ksDelay2);
                        b1[w] = ((1.0f - v.ksBlend) * y1 + v.ksBlend * v.ksPrev1) * v.ksGain1;
                        b2[w] = ((1.0f - v.ksBlend) * y2 + v.ksBlend * v.ksPrev2) * v.ksGain2;
                        v.ksPrev1 = y1;
                        v.ksPrev2 = y2;
                        v.ksWrite = (w + 1) & mask;
                        osc1 = y1;
                        osc2 = y2;
                        blend = dyad ? 0.72f : 0.80f * detuneBlend;
                        break;
                    }
                    case kWaveBowed: {
                        v.lpB += 0.02f * (whiteNoise(v.rng) - v.lpB); // slow bow-pressure wander
                        osc1 = sawBlep(v.phase1, v.dt1) * (1.0f + 1.6f * v.lpB * (0.4f + dyn))
                             + pulseBlep(v.phase1, v.dt1, 0.38) * 0.30f * dyn;
                        osc2 = sawBlep(v.phase2, v.dt2);
                        formant = true;
                        break;
                    }
                    case kWaveChoir: {
                        const float src = sawBlep(v.phase1, v.dt1) * 0.7f + pulseBlep(v.phase1, v.dt1, 0.32) * 0.3f;
                        v.lpA += v.lpCoef * (src - v.lpA);
                        v.lpB += v.lpCoef * (sawBlep(v.phase2, v.dt2) - v.lpB);
                        osc1 = v.lpA + whiteNoise(v.rng) * 0.02f;
                        osc2 = v.lpB;
                        blend = dyad ? 0.72f : 0.85f * detuneBlend;
                        formant = true;
                        break;
                    }
                    default: break;
                }

                mix = (osc1 + blend * osc2) * 0.45f;

                if (formant) {
                    float out = mix * formDry_;
                    for (int f = 0; f < 3; ++f) {
                        const float v3 = mix - v.fz[f][1];
                        const float v1 = formA1_[f] * v.fz[f][0] + formA2_[f] * v3;
                        const float v2 = v.fz[f][1] + formA2_[f] * v.fz[f][0] + formA3_[f] * v3;
                        v.fz[f][0] = 2.0f * v1 - v.fz[f][0];
                        v.fz[f][1] = 2.0f * v2 - v.fz[f][1];
                        out += v1 * formGain_[f];
                    }
                    mix = out;
                }

                if (subMix > 0.005f) {
                    const float sub = (wave >= kWaveSoftStr)
                        ? fsin(v.subPhase)
                        : nesTriRaw(v.subPhase, 0.5f) * 0.6f + pulseBlep(v.subPhase, v.subDt, 0.5) * 0.4f;
                    mix += sub * (subMix * 0.55f);
                }
                if (noiseLevel_ > 0.0005f) mix += whiteNoise(v.rng) * noiseLevel_;
            }

            // ---- Voice lowpass ----
            const float v3 = mix - v.ic2eq;
            const float v1 = v.fa1 * v.ic1eq + v.fa2 * v3;
            const float v2 = v.ic2eq + v.fa2 * v.ic1eq + v.fa3 * v3;
            v.ic1eq = 2.0f * v1 - v.ic1eq;
            v.ic2eq = 2.0f * v2 - v.ic2eq;

            const float voiceOut = v2 * v.envLevel * v.ampVel * 0.30f * waveGain;
            dryL += voiceOut * (1.0f - v.pan * 0.5f);
            dryR += voiceOut * (0.5f + v.pan * 0.5f);
        }

        // ---- Drive ----
        if (sm_[kParamDrive] > 0.01f) {
            dryL = (std::tanh(dryL * driveGain_ + driveBias_) - driveBiasOut_) * driveNorm_;
            dryR = (std::tanh(dryR * driveGain_ + driveBias_) - driveBiasOut_) * driveNorm_;
        }

        // ---- Bitcrush + decimate ----
        if (crushMix_ > 0.0f) {
            crushPhase_ += crushStepRate_;
            if (crushPhase_ >= 1.0) {
                crushPhase_ -= std::floor(crushPhase_);
                if (crushLevels_ > 1.0f) {
                    crushHoldL_ = std::round(dryL * crushLevels_) / crushLevels_;
                    crushHoldR_ = std::round(dryR * crushLevels_) / crushLevels_;
                } else {
                    crushHoldL_ = dryL;
                    crushHoldR_ = dryR;
                }
            }
            dryL = dryL * (1.0f - crushMix_) + crushHoldL_ * crushMix_;
            dryR = dryR * (1.0f - crushMix_) + crushHoldR_ * crushMix_;
        }

        // ---- Phaser (4 all-pass stages per side, regenerating) ----
        const float phaser = sm_[kParamPhaser];
        if (phaser > 0.01f) {
            float in[2] = { dryL, dryR };
            for (int ch = 0; ch < 2; ++ch) {
                float x = in[ch] + phFb_[ch] * (0.55f * phaser);
                const float a = phCoef_[ch];
                for (int st = 0; st < 4; ++st) {
                    const float y = a * x + phZ_[ch][st];
                    phZ_[ch][st] = x - a * y;
                    x = y;
                }
                phFb_[ch] = x;
                in[ch] = in[ch] * (1.0f - 0.5f * phaser) + x * (0.5f * phaser);
            }
            dryL = in[0];
            dryR = in[1];
        }

        // ---- Ensemble chorus (two modulated taps per side) ----
        const float chorusAmt = sm_[kParamChorus];
        chorusBufL_[chorusWriteIdx_] = dryL;
        chorusBufR_[chorusWriteIdx_] = dryR;
        if (chorusAmt > 0.01f) {
            chorusLfoPhase_ += 0.85 * invSr;
            if (chorusLfoPhase_ >= 1.0) chorusLfoPhase_ -= 1.0;
            auto readInterp = [&](const std::vector<float>& buf, double delaySec) {
                double rPos = chorusWriteIdx_ - delaySec * sampleRate_;
                if (rPos < 0.0) rPos += chorusSize;
                int i0 = static_cast<int>(rPos);
                float frac = static_cast<float>(rPos - i0);
                i0 %= chorusSize;
                int i1 = i0 + 1; if (i1 >= chorusSize) i1 = 0;
                return buf[i0] * (1.0f - frac) + buf[i1] * frac;
            };
            const float l1 = fsin(chorusLfoPhase_), l2 = fsin(chorusLfoPhase_ + 0.25);
            const float l3 = fsin(chorusLfoPhase_ * 1.0 + 0.37), l4 = fsin(chorusLfoPhase_ + 0.62);
            const float cL = readInterp(chorusBufL_, 0.012 + 0.004 * l1) + 0.7f * readInterp(chorusBufL_, 0.021 + 0.005 * l3);
            const float cR = readInterp(chorusBufR_, 0.015 + 0.004 * l2) + 0.7f * readInterp(chorusBufR_, 0.024 + 0.005 * l4);
            dryL += cL * (chorusAmt * 0.36f);
            dryR += cR * (chorusAmt * 0.36f);
        }
        if (++chorusWriteIdx_ >= chorusSize) chorusWriteIdx_ = 0;

        // ---- Tremolo (stereo, right side a quarter cycle late) ----
        const float trem = sm_[kParamTremolo];
        if (trem > 0.01f) {
            tremPhase_ += tremRate_ * invSr;
            if (tremPhase_ >= 1.0) tremPhase_ -= 1.0;
            dryL *= 1.0f - trem * (0.5f + 0.5f * fsin(tremPhase_));
            dryR *= 1.0f - trem * (0.5f + 0.5f * fsin(tremPhase_ + 0.25));
        }

        // ---- Tape: wow + flutter, soft saturation, rolled-off top ----
        const float tape = sm_[kParamTape];
        tapeBufL_[tapeWriteIdx_] = dryL;
        tapeBufR_[tapeWriteIdx_] = dryR;
        if (tape > 0.01f) {
            tapeWowPhase_ += 0.55 * invSr;      if (tapeWowPhase_ >= 1.0) tapeWowPhase_ -= 1.0;
            tapeFlutterPhase_ += 6.3 * invSr;   if (tapeFlutterPhase_ >= 1.0) tapeFlutterPhase_ -= 1.0;
            const double delaySec = 0.008 + tape * (0.0022 * fsin(tapeWowPhase_) + 0.00025 * fsin(tapeFlutterPhase_));
            double rPos = tapeWriteIdx_ - delaySec * sampleRate_;
            if (rPos < 0.0) rPos += tapeSize;
            int i0 = static_cast<int>(rPos);
            const float frac = static_cast<float>(rPos - i0);
            i0 %= tapeSize;
            int i1 = i0 + 1; if (i1 >= tapeSize) i1 = 0;
            const float tL = tapeBufL_[i0] * (1.0f - frac) + tapeBufL_[i1] * frac;
            const float tR = tapeBufR_[i0] * (1.0f - frac) + tapeBufR_[i1] * frac;
            const float pre = 1.0f + 1.5f * tape;
            const float post = 1.0f / (1.0f + 1.2f * tape);
            tapeLpL_ += tapeLpCoef_ * (std::tanh(tL * pre) * post - tapeLpL_);
            tapeLpR_ += tapeLpCoef_ * (std::tanh(tR * pre) * post - tapeLpR_);
            dryL = tapeLpL_;
            dryR = tapeLpR_;
        }
        if (++tapeWriteIdx_ >= tapeSize) tapeWriteIdx_ = 0;

        // ---- Delay ----
        int rIdxL = delayWriteIdx_ - delaySamplesL_; if (rIdxL < 0) rIdxL += delaySize;
        int rIdxR = delayWriteIdx_ - delaySamplesR_; if (rIdxR < 0) rIdxR += delaySize;
        delayLpL_ += delayLpCoef_ * (delayBufL_[rIdxL] - delayLpL_);
        delayLpR_ += delayLpCoef_ * (delayBufR_[rIdxR] - delayLpR_);
        float fbL = (delayCross_ ? delayLpR_ : delayLpL_) * delayFb_;
        float fbR = (delayCross_ ? delayLpL_ : delayLpR_) * delayFb_;
        if (delaySat_) { fbL = std::tanh(fbL * 1.3f) * 0.77f; fbR = std::tanh(fbR * 1.3f) * 0.77f; }
        delayBufL_[delayWriteIdx_] = dryL + fbL;
        delayBufR_[delayWriteIdx_] = dryR + fbR;
        if (++delayWriteIdx_ >= delaySize) delayWriteIdx_ = 0;

        float sigL = dryL + delayLpL_ * delayWet_;
        float sigR = dryR + delayLpR_ * delayWet_;

        // ---- Reverb (8 combs + 4 all-passes per side, pre-delayed) ----
        if (sm_[kParamReverb] > 0.005f) {
            const float revIn = preDelayBuf_[preDelayIdx_];
            preDelayBuf_[preDelayIdx_] = (sigL + sigR) * 0.5f;
            if (++preDelayIdx_ >= preDelayLen_) preDelayIdx_ = 0;
            float revL = 0.0f, revR = 0.0f;
            for (int c = 0; c < 8; ++c) {
                revL += combsL_[c].process(revIn, revFeedback_, revDamp_);
                revR += combsR_[c].process(revIn, revFeedback_, revDamp_);
            }
            for (int a = 0; a < 4; ++a) {
                revL = allpassL_[a].process(revL);
                revR = allpassR_[a].process(revR);
            }
            sigL += revL * revWet_;
            sigR += revR * revWet_;
        }

        // ---- Handheld speaker ----
        const float spk = sm_[kParamSpeaker];
        if (spk > 0.01f) {
            float in[2] = { sigL, sigR };
            float wet[2];
            for (int ch = 0; ch < 2; ++ch) {
                float v3 = in[ch] - spkHp_[ch][1];
                float v1 = spkHpA_[0] * spkHp_[ch][0] + spkHpA_[1] * v3;
                float v2 = spkHp_[ch][1] + spkHpA_[1] * spkHp_[ch][0] + spkHpA_[2] * v3;
                spkHp_[ch][0] = 2.0f * v1 - spkHp_[ch][0];
                spkHp_[ch][1] = 2.0f * v2 - spkHp_[ch][1];
                const float hp = in[ch] - 1.2f * v1 - v2;
                v3 = hp - spkLp_[ch][1];
                v1 = spkLpA_[0] * spkLp_[ch][0] + spkLpA_[1] * v3;
                v2 = spkLp_[ch][1] + spkLpA_[1] * spkLp_[ch][0] + spkLpA_[2] * v3;
                spkLp_[ch][0] = 2.0f * v1 - spkLp_[ch][0];
                spkLp_[ch][1] = 2.0f * v2 - spkLp_[ch][1];
                wet[ch] = softClip(v2 * 1.7f);
            }
            const float mono = 0.5f * (wet[0] + wet[1]);
            wet[0] += (mono - wet[0]) * 0.6f;
            wet[1] += (mono - wet[1]) * 0.6f;
            sigL += (wet[0] - sigL) * spk;
            sigR += (wet[1] - sigR) * spk;
        }

        // ---- Stereo width ----
        {
            const float mid = 0.5f * (sigL + sigR);
            const float side = 0.5f * (sigL - sigR) * (2.0f * sm_[kParamWidth]);
            sigL = mid + side;
            sigR = mid - side;
        }

        // ---- DC blocker, output level, soft limiter ----
        const float yL = sigL - dcX_[0] + dcR * dcY_[0];
        const float yR = sigR - dcX_[1] + dcR * dcY_[1];
        dcX_[0] = sigL; dcY_[0] = yL;
        dcX_[1] = sigR; dcY_[1] = yR;

        const float expr = sm_[kParamExpression];
        const float exprGain = 0.10f + 1.15f * expr * expr;
        const float finalL = softClip(yL * exprGain);
        const float finalR = softClip(yR * exprGain);

        outL[s] = finalL;
        if (outR) outR[s] = finalR;

        blockPeakL = std::max(blockPeakL, std::fabs(finalL));
        blockPeakR = std::max(blockPeakR, std::fabs(finalR));

        if (telemetry && (s & 7) == 0) {
            uint32_t wIdx = telemetry->scopeWriteIdx.load(std::memory_order_relaxed);
            telemetry->scopeSamples[wIdx & 255] = 0.5f * (finalL + finalR);
            telemetry->scopeWriteIdx.store(wIdx + 1, std::memory_order_relaxed);
        }
    }

    if (telemetry) {
        float prevL = telemetry->peakL.load(std::memory_order_relaxed);
        float prevR = telemetry->peakR.load(std::memory_order_relaxed);
        telemetry->peakL.store(std::max(blockPeakL, prevL * 0.85f), std::memory_order_relaxed);
        telemetry->peakR.store(std::max(blockPeakR, prevR * 0.85f), std::memory_order_relaxed);
        updateActiveNoteMask(telemetry);
    }

    _mm_setcsr(savedCsr);
}

} // namespace PixelLabs
