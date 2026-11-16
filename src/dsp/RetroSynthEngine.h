#pragma once

#include "../plugin/PixelLabsIds.h"
#include <vector>
#include <cmath>
#include <cstdint>

namespace PixelLabs {

struct Voice {
    bool active = false;
    bool releasing = false;
    bool sustained = false; // held by the sustain pedal
    int midiNote = -1;
    int noteId = -1;
    float velocity = 0.0f;
    float pan = 0.5f;
    uint32_t age = 0;

    // Oscillators
    double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0, subPhase = 0.0;
    double dt1 = 0.0, dt2 = 0.0, dt3 = 0.0, subDt = 0.0;
    double pwmPhase = 0.0;
    float pwA = 0.5f, pwB = 0.5f;

    // Pitch motion (updated at control rate)
    double noteTime = 0.0;
    double arpTimer = 0.0;
    int arpStep = 0;
    float glideSemis = 0.0f;
    float sweepStart = 0.0f;
    float driftSemis = 0.0f;

    // Amp envelope: 0 off, 1 attack, 2 decay/sustain, 4 release
    int envStage = 0;
    float envLevel = 0.0f;

    // Voice lowpass (TPT state variable)
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f, fk = 1.0f;

    // FM
    float fmPrev = 0.0f;
    float fmEnvFast = 1.0f, fmEnvSlow = 1.0f;

    // Noise
    uint32_t lfsr = 0x7FFFu;
    double noisePhase = 0.0;
    float noiseVal = 1.0f;
    uint32_t rng = 0x1234567u;

    // One-pole helpers (string softening, breath, bow jitter)
    float lpA = 0.0f, lpB = 0.0f, lpCoef = 0.5f;

    // Formant resonators (choir, reed, bowed body)
    float fz[3][2] = {};

    // Plucked string (two delay lines per voice)
    int ksWrite = 0;
    double ksDelay1 = 100.0, ksDelay2 = 100.0;
    float ksPrev1 = 0.0f, ksPrev2 = 0.0f;
    float ksGain1 = 0.99f, ksGain2 = 0.99f;
    float ksBlend = 0.3f;

    // Drum kit
    int drumType = -1;
    float dAmpA = 0.0f, dAmpB = 0.0f, dPitch = 0.0f;
    float dCoefA = 0.0f, dCoefB = 0.0f, dCoefP = 0.0f;
    float dBaseHz = 100.0f, dRangeHz = 0.0f;
};

class RetroSynthEngine {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kCtrlInterval = 16; // samples between control-rate updates
    static constexpr int kNumDemoSongs = 4;
    static constexpr int kDemoTracks = 5;

    RetroSynthEngine();
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setParameterNormalized(int paramId, float normValue);
    float getParameterNormalized(int paramId) const;
    void applyPreset(int presetIndex);
    void setHostTempo(double bpm);

    void noteOn(int pitch, float velocity, int noteId = -1);
    void noteOff(int pitch, int noteId = -1);
    void allNotesOff();

    void renderBlock(float* outL, float* outR, int numSamples, SharedUITelemetry* telemetry);

    static const char* demoSongName(int index);

private:
    struct Comb {
        std::vector<float> buf;
        int idx = 0;
        int len = 16;
        float store = 0.0f;
        void init(int maxSize) {
            buf.assign(maxSize > 16 ? maxSize : 16, 0.0f);
            idx = 0;
            store = 0.0f;
            len = static_cast<int>(buf.size());
        }
        void setLen(int l) {
            int maxLen = static_cast<int>(buf.size());
            len = l < 16 ? 16 : (l > maxLen ? maxLen : l);
            if (idx >= len) idx = 0;
        }
        inline float process(float input, float feedback, float damp) {
            float output = buf[idx];
            store = output * (1.0f - damp) + store * damp;
            buf[idx] = input + store * feedback;
            if (++idx >= len) idx = 0;
            return output;
        }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); store = 0.0f; }
    };

    struct Allpass {
        std::vector<float> buf;
        int idx = 0;
        int len = 16;
        void init(int maxSize) {
            buf.assign(maxSize > 16 ? maxSize : 16, 0.0f);
            idx = 0;
            len = static_cast<int>(buf.size());
        }
        void setLen(int l) {
            int maxLen = static_cast<int>(buf.size());
            len = l < 16 ? 16 : (l > maxLen ? maxLen : l);
            if (idx >= len) idx = 0;
        }
        inline float process(float input) {
            float bufOut = buf[idx];
            float output = bufOut - input;
            buf[idx] = input + bufOut * 0.5f;
            if (++idx >= len) idx = 0;
            return output;
        }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    };

    struct DemoSong {
        const char* name = "";
        double stepSec = 0.12;
        int numSteps = 0;
        std::vector<int8_t> tracks[kDemoTracks]; // >=0 note on, -1 rest, -2 hold
        float velocity[kDemoTracks] = { 0.9f, 0.78f, 0.3f, 0.3f, 0.5f };
    };

    double sampleRate_ = 48000.0;
    float params_[kNumParams];
    float sm_[kNumParams]; // control-rate smoothed copies
    Voice voices_[kMaxVoices];
    int ctrlCount_ = 0;
    int lastNote_ = -1;
    int currentEra_ = kEraChip80;
    double hostTempo_ = 125.0;

    // Per-block / control-rate derived values
    int waveMode_ = 0, harmMode_ = 0;
    float attackStep_ = 1.0f, decayCoef_ = 0.999f, releaseCoef_ = 0.999f, sustainLvl_ = 0.6f;
    float formA1_[3] = {}, formA2_[3] = {}, formA3_[3] = {}, formK_[3] = {}, formGain_[3] = {};
    float formDry_ = 0.0f;
    float gbTable_[32] = {};
    double vibLfoPhase_ = 0.0;
    float vibLfo_ = 0.0f;
    float smoothCoef_ = 0.05f;
    float contourScale_ = 1.0f;
    double arpPeriod_ = 0.02;
    float glideCoef_ = 0.0f;
    double sweepSec_ = 0.1;
    float bendSemis_ = 0.0f, vibDepth_ = 0.0f;
    double detuneRatio_ = 1.004;
    float fmFastCoef_ = 0.99f, fmSlowCoef_ = 0.999f;
    float noiseLevel_ = 0.0f;
    double ksT60Held_ = 3.0, ksT60Rel_ = 0.2;
    float driveGain_ = 1.0f, driveNorm_ = 1.0f, driveBias_ = 0.0f, driveBiasOut_ = 0.0f;
    double crushStepRate_ = 1.0;
    float crushLevels_ = 0.0f, crushMix_ = 0.0f;
    float tapeLpCoef_ = 1.0f;
    float tremRate_ = 5.0f;
    float revWet_ = 0.0f;
    bool delaySat_ = false;

    // Demo sequencer
    DemoSong songs_[kNumDemoSongs];
    bool demoActive_ = false;
    int demoSongIdx_ = 0;
    double demoSampleCounter_ = 0.0;
    int demoStep_ = 0;
    int demoPrev_[kDemoTracks] = { -1, -1, -1, -1, -1 };

    // --- Effects state ---
    float dcX_[2] = {}, dcY_[2] = {};

    float crushHoldL_ = 0.0f, crushHoldR_ = 0.0f;
    double crushPhase_ = 0.0;

    float phZ_[2][4] = {};
    float phFb_[2] = {};
    float phCoef_[2] = {};
    double phLfoPhase_ = 0.0;

    std::vector<float> chorusBufL_, chorusBufR_;
    int chorusWriteIdx_ = 0;
    double chorusLfoPhase_ = 0.0;

    double tremPhase_ = 0.0;

    std::vector<float> tapeBufL_, tapeBufR_;
    int tapeWriteIdx_ = 0;
    double tapeWowPhase_ = 0.0, tapeFlutterPhase_ = 0.0;
    float tapeLpL_ = 0.0f, tapeLpR_ = 0.0f;

    std::vector<float> delayBufL_, delayBufR_;
    int delayWriteIdx_ = 0;
    int delaySamplesL_ = 1000, delaySamplesR_ = 1500;
    float delayLpL_ = 0.0f, delayLpR_ = 0.0f;
    float delayFb_ = 0.3f, delayLpCoef_ = 0.5f, delayWet_ = 0.0f;
    bool delayCross_ = true;

    Comb combsL_[8], combsR_[8];
    Allpass allpassL_[4], allpassR_[4];
    std::vector<float> preDelayBuf_;
    int preDelayIdx_ = 0, preDelayLen_ = 16;
    int reverbTypeApplied_ = -1;
    float revFeedback_ = 0.8f, revDamp_ = 0.3f;

    float spkHp_[2][2] = {}, spkLp_[2][2] = {};
    float spkHpA_[3] = {}, spkLpA_[3] = {};

    std::vector<float> ksBuf_;
    int ksSize_ = 4096;

    uint32_t noiseRng_ = 0x12345678u;

    void buildDemoSongs();
    void updateControl();
    void updateVoiceControl(Voice& v, int voiceIndex);
    void applyReverbType(int type);
    void exciteString(Voice& v, int voiceIndex, double freq1, double freq2);
    void setupDrum(Voice& v);
    void releaseDemoNotes();
    void updateActiveNoteMask(SharedUITelemetry* telemetry);
    inline float* ksLine(int voiceIndex, int line) { return ksBuf_.data() + (voiceIndex * 2 + line) * ksSize_; }
};

} // namespace PixelLabs
