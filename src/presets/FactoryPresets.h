#pragma once

#include "../plugin/PixelLabsIds.h"
#include <cmath>

namespace PixelLabs {

struct FactoryPreset {
    const char* name = "";
    const char* category = "";
    const char* description = "";
    int era = kEraChip80;
    int defaultMacro = 0;
    float values[kNumParams] = {};
};

// Slots are append-only: a project stores the slot index, so existing slots never move.
static constexpr int kNumFactoryPresets = 80;

static const char* const kEraNames[kNumEras] = {
    "80S CHIP", "90S CHIP", "SYNTH 80S", "MEDIEVAL", "SFX + KIT"
};

namespace detail {

// Normalised value that makes the engine start a sweep `semis` away from the note.
inline float sweepNorm(float semis) {
    float a = std::sqrt(std::fabs(semis) / 48.0f) * 0.5f;
    return semis >= 0.0f ? 0.5f + a : 0.5f - a;
}
inline float sweepTimeNorm(float sec) { return std::sqrt((sec > 0.01f ? sec - 0.01f : 0.0f) / 1.5f); }
inline float arpRateNorm(float hz)    { return std::log(hz / 4.0f) / std::log(16.0f); }

class Builder {
public:
    Builder(const char* name, const char* category, int era, const char* description) {
        p_.name = name;
        p_.category = category;
        p_.era = era;
        p_.description = description;
        float* v = p_.values;
        v[kParamExpression] = 0.85f;  v[kParamDynamics] = 0.55f;
        v[kParamReverb] = 0.25f;      v[kParamCrush] = 0.0f;
        v[kParamChorus] = 0.20f;      v[kParamDelay] = 0.15f;
        v[kParamEnvShape] = 0.5f;     v[kParamVibrato] = 0.15f;
        v[kParamWaveform] = 0.0f;     v[kParamHarmony] = 0.0f;
        v[kParamSubMix] = 0.20f;      v[kParamCutoff] = 0.80f;
        v[kParamResonance] = 0.18f;   v[kParamPreset] = 0.0f;
        v[kParamPitchBend] = 0.5f;    v[kParamSustain] = 0.0f;
        v[kParamPhaser] = 0.0f;       v[kParamDrive] = 0.0f;
        v[kParamTape] = 0.0f;         v[kParamTremolo] = 0.0f;
        v[kParamSpeaker] = 0.0f;      v[kParamWidth] = 0.5f;
        v[kParamReverbType] = listToNorm(kRevHall, kNumReverbTypes);
        v[kParamDelayType] = listToNorm(kDlyPing, kNumDelayTypes);
        v[kParamSweep] = 0.5f;        v[kParamSweepTime] = 0.3f;
        v[kParamArpRate] = arpRateNorm(50.0f);
        v[kParamGlide] = 0.0f;        v[kParamNoise] = 0.0f;
        v[kParamAttack] = 0.02f;      v[kParamDecay] = 0.40f;
        v[kParamSustainLevel] = 0.80f; v[kParamRelease] = 0.30f;
        v[kParamFilterEnv] = 0.30f;   v[kParamDetune] = 0.40f;
        v[kParamVelSens] = kDefaultVelSens;
        v[kParamTouch] = listToNorm(kTouchNormal, kNumTouchCurves);
    }

    Builder& set(int id, float value)   { p_.values[id] = value; return *this; }
    Builder& wave(int w)                { return set(kParamWaveform, listToNorm(w, kNumWaveforms)); }
    Builder& harm(int h)                { return set(kParamHarmony, listToNorm(h, kNumHarmonyModes)); }
    Builder& kit(int k)                 { return harm(k); } // the drum oscillator reads HARM as its kit
    Builder& dyn(float d)               { return set(kParamDynamics, d); }
    Builder& expr(float e)              { return set(kParamExpression, e); }
    Builder& env(float a, float d, float s, float r) {
        set(kParamAttack, a); set(kParamDecay, d); set(kParamSustainLevel, s); return set(kParamRelease, r);
    }
    Builder& filt(float cut, float res, float envAmt) {
        set(kParamCutoff, cut); set(kParamResonance, res); return set(kParamFilterEnv, envAmt);
    }
    Builder& sub(float s)               { return set(kParamSubMix, s); }
    Builder& noise(float n)             { return set(kParamNoise, n); }
    Builder& detune(float d)            { return set(kParamDetune, d); }
    Builder& vib(float v)               { return set(kParamVibrato, v); }
    Builder& glide(float g)             { return set(kParamGlide, g); }
    Builder& arp(float hz)              { return set(kParamArpRate, arpRateNorm(hz)); }
    Builder& sweep(float semis, float sec) {
        set(kParamSweep, sweepNorm(semis)); return set(kParamSweepTime, sweepTimeNorm(sec));
    }
    Builder& reverb(float amt, int type) { set(kParamReverb, amt); return set(kParamReverbType, listToNorm(type, kNumReverbTypes)); }
    Builder& delay(float amt, int type)  { set(kParamDelay, amt); return set(kParamDelayType, listToNorm(type, kNumDelayTypes)); }
    Builder& chorus(float c)            { return set(kParamChorus, c); }
    Builder& phaser(float c)            { return set(kParamPhaser, c); }
    Builder& drive(float c)             { return set(kParamDrive, c); }
    Builder& tape(float c)              { return set(kParamTape, c); }
    Builder& trem(float c)              { return set(kParamTremolo, c); }
    Builder& speaker(float c)           { return set(kParamSpeaker, c); }
    Builder& crush(float c)             { return set(kParamCrush, c); }
    Builder& width(float c)             { return set(kParamWidth, c); }
    Builder& macro(int m)               { p_.defaultMacro = m; return *this; }

    const FactoryPreset& done() const { return p_; }

private:
    FactoryPreset p_;
};

struct PresetBank {
    FactoryPreset presets[kNumFactoryPresets];
    int built = 0;

    void add(const Builder& b) {
        if (built >= kNumFactoryPresets) return;
        presets[built] = b.done();
        presets[built].values[kParamPreset] = listToNorm(built, kNumFactoryPresets);
        ++built;
    }

    PresetBank() {
        using B = Builder;

        // ---- Slots 0..7 keep the order of the v1 bank so old projects recall the same sound ----
        add(B("Cascade Lead", "LEAD", kEraChip80, "Square lead doubled a fifth up")
            .wave(kWavePulse50).harm(kHarmFifth).dyn(0.55f).env(0.0f, 0.42f, 0.70f, 0.30f)
            .filt(0.82f, 0.18f, 0.45f).sub(0.25f).vib(0.30f).detune(0.44f)
            .reverb(0.32f, kRevHall).delay(0.25f, kDlyPing).chorus(0.35f).crush(0.10f).macro(0));
        add(B("Raindrops Glass Arp", "ARP", kEraChip80, "SID-style glass arpeggio with echo")
            .wave(kWavePulse25).harm(kHarmArpMajor).arp(50.0f).dyn(0.65f).env(0.0f, 0.40f, 0.62f, 0.36f)
            .filt(0.88f, 0.25f, 0.50f).sub(0.15f).vib(0.12f).detune(0.44f)
            .reverb(0.48f, kRevHall).delay(0.45f, kDlyPing).chorus(0.42f).crush(0.12f).macro(1));
        add(B("Mega FM Brass", "BRASS", kEraChip90, "Two-operator FM brass, 16-bit console style")
            .wave(kWaveFm16).dyn(0.72f).env(0.10f, 0.48f, 0.72f, 0.38f)
            .filt(0.78f, 0.22f, 0.40f).sub(0.30f).vib(0.32f).detune(0.44f)
            .reverb(0.30f, kRevHall).delay(0.20f, kDlyPing).chorus(0.50f).crush(0.08f).macro(2));
        add(B("Comet Zap Bass", "BASS", kEraChip90, "Tracker bass with a pitch zap on every note")
            .wave(kWavePulse12).dyn(0.60f).env(0.0f, 0.36f, 0.62f, 0.22f)
            .filt(0.58f, 0.48f, 0.55f).sub(0.65f).vib(0.05f).detune(0.40f).sweep(12.0f, 0.06f)
            .reverb(0.12f, kRevRoom).delay(0.08f, kDlyPing).chorus(0.20f).crush(0.30f).macro(7));
        add(B("Starlight Pad", "PAD", kEraChip90, "Warm detuned saw pad with octave shimmer")
            .wave(kWaveChipSaw).harm(kHarmOctave).dyn(0.68f).env(0.48f, 0.60f, 0.90f, 0.62f)
            .filt(0.64f, 0.20f, 0.25f).sub(0.35f).vib(0.30f).detune(0.55f).width(0.70f)
            .reverb(0.65f, kRevHall).delay(0.35f, kDly16Bit).chorus(0.68f).macro(0));
        add(B("Triangle Flute", "SOLO", kEraChip80, "Stepped 4-bit triangle with singing vibrato")
            .wave(kWaveNesTri).dyn(0.50f).env(0.03f, 0.40f, 0.72f, 0.34f)
            .filt(0.75f, 0.15f, 0.35f).sub(0.10f).vib(0.50f)
            .reverb(0.38f, kRevHall).delay(0.30f, kDlyPing).chorus(0.25f).crush(0.15f).macro(11));
        add(B("Cyber Arcade Pluck", "PLUCK", kEraChip80, "Snappy pulse pluck, wide chorus")
            .wave(kWavePulse25).dyn(0.75f).env(0.0f, 0.34f, 0.0f, 0.30f)
            .filt(0.60f, 0.32f, 0.65f).sub(0.25f).vib(0.10f).detune(0.50f).width(0.70f)
            .reverb(0.35f, kRevHall).delay(0.40f, kDlyPing).chorus(0.55f).crush(0.10f).macro(10));
        add(B("Subsonic Breaks Organ", "ORGAN", kEraChip90, "Tracker power-chord organ")
            .wave(kWavePulse50).harm(kHarmFifth).dyn(0.62f).env(0.01f, 0.45f, 0.80f, 0.40f)
            .filt(0.76f, 0.24f, 0.35f).sub(0.45f).vib(0.20f).detune(0.44f)
            .reverb(0.42f, kRevHall).delay(0.28f, kDlyPing).chorus(0.48f).crush(0.15f).macro(2));

        // ---- 80s chip: 8-bit consoles, home computers, arcades ----
        add(B("Pulse Hero Lead", "LEAD", kEraChip80, "25% duty lead with delayed vibrato")
            .wave(kWavePulse25).dyn(0.50f).env(0.0f, 0.36f, 0.82f, 0.22f)
            .filt(0.86f, 0.15f, 0.25f).sub(0.08f).vib(0.52f).detune(0.25f)
            .reverb(0.20f, kRevHall).delay(0.32f, kDlyPing).chorus(0.10f).macro(1));
        add(B("SID Sweep Strings", "PAD", kEraChip80, "Slow pulse-width sweep, home-computer strings")
            .wave(kWavePwm).dyn(0.42f).env(0.42f, 0.50f, 0.92f, 0.52f)
            .filt(0.70f, 0.22f, 0.20f).sub(0.15f).vib(0.20f).detune(0.58f).width(0.70f)
            .reverb(0.45f, kRevHall).delay(0.18f, kDlyPing).chorus(0.50f).phaser(0.22f).macro(3));
        add(B("Tri Dungeon Bass", "BASS", kEraChip80, "Raw stepped triangle bass")
            .wave(kWaveNesTri).dyn(0.15f).env(0.0f, 0.30f, 0.90f, 0.14f)
            .filt(0.92f, 0.05f, 0.10f).sub(0.30f).vib(0.0f)
            .reverb(0.08f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(7));
        add(B("Castle Square Organ", "ORGAN", kEraChip80, "Square + octave, echoing stone hall")
            .wave(kWavePulse50).harm(kHarmOctave).dyn(0.50f).env(0.01f, 0.25f, 0.95f, 0.16f)
            .filt(0.70f, 0.12f, 0.15f).sub(0.40f).vib(0.10f)
            .reverb(0.50f, kRevChurch).delay(0.10f, kDlyPing).chorus(0.15f).macro(0));
        add(B("Noise Snare & Hats", "PERC", kEraChip80, "Pitched LFSR noise: low keys thump, high keys tick")
            .wave(kWaveNoise).dyn(0.30f).env(0.0f, 0.24f, 0.0f, 0.16f)
            .filt(0.95f, 0.05f, 0.20f).sub(0.0f).vib(0.0f)
            .reverb(0.14f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(7));

        // ---- 90s chip: handhelds, 16-bit consoles, trackers ----
        add(B("Pocket Route Lead", "LEAD", kEraChip90, "Handheld pulse lead through a tiny speaker")
            .wave(kWavePulse25).dyn(0.48f).env(0.0f, 0.34f, 0.78f, 0.18f)
            .filt(0.90f, 0.10f, 0.20f).sub(0.05f).vib(0.55f).detune(0.20f)
            .reverb(0.12f, kRevRoom).delay(0.26f, kDly16Bit).chorus(0.0f).speaker(0.40f).macro(8));
        add(B("Pocket Wave Bass", "BASS", kEraChip90, "4-bit wavetable channel bass")
            .wave(kWaveGbWave).dyn(0.35f).env(0.0f, 0.32f, 0.85f, 0.12f)
            .filt(0.84f, 0.10f, 0.15f).sub(0.22f).vib(0.0f)
            .reverb(0.06f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).speaker(0.25f).macro(8));
        add(B("Tall Grass Twinkle", "ARP", kEraChip90, "Octave-flicker pulse chime")
            .wave(kWavePulse12).harm(kHarmArpOctave).arp(30.0f).dyn(0.50f).env(0.0f, 0.44f, 0.20f, 0.36f)
            .filt(0.92f, 0.12f, 0.25f).sub(0.0f).vib(0.10f)
            .reverb(0.22f, kRevHall).delay(0.36f, kDly16Bit).chorus(0.10f).speaker(0.30f).macro(1));
        add(B("Mode Seven Strings", "STRINGS", kEraChip90, "Soft sampled-style string ensemble")
            .wave(kWaveSoftStr).dyn(0.50f).env(0.36f, 0.50f, 0.90f, 0.46f)
            .filt(0.74f, 0.10f, 0.20f).sub(0.12f).vib(0.28f).detune(0.52f).width(0.68f)
            .reverb(0.50f, kRevHall).delay(0.22f, kDly16Bit).chorus(0.30f).macro(0));
        add(B("Mega Slap Bass", "BASS", kEraChip90, "Punchy FM bass with a hard attack")
            .wave(kWaveFmBass).dyn(0.66f).env(0.0f, 0.40f, 0.55f, 0.14f)
            .filt(0.86f, 0.10f, 0.25f).sub(0.30f).vib(0.0f).detune(0.25f)
            .reverb(0.08f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.15f).drive(0.20f).macro(6));
        add(B("Crystal Cave Keys", "KEYS", kEraChip90, "Glassy FM keys in a dripping cavern")
            .wave(kWaveFmKeys).dyn(0.55f).env(0.0f, 0.62f, 0.25f, 0.42f)
            .filt(0.88f, 0.08f, 0.20f).sub(0.05f).vib(0.08f).detune(0.40f).width(0.66f)
            .reverb(0.42f, kRevCave).delay(0.40f, kDly16Bit).chorus(0.35f).macro(1));

        // ---- Synth 80s: analog polysynths, FM keys, tape ----
        add(B("Neon Sunset Lead", "LEAD", kEraSynth, "Gliding supersaw lead with tape echo")
            .wave(kWaveChipSaw).dyn(0.60f).env(0.01f, 0.42f, 0.80f, 0.28f).glide(0.38f)
            .filt(0.74f, 0.30f, 0.38f).sub(0.15f).vib(0.42f).detune(0.52f).width(0.66f)
            .reverb(0.30f, kRevHall).delay(0.36f, kDlyTape).chorus(0.40f).macro(1));
        add(B("Outrun Bass", "BASS", kEraSynth, "Filter-plucked saw bass with solid sub")
            .wave(kWaveChipSaw).dyn(0.40f).env(0.0f, 0.34f, 0.35f, 0.14f)
            .filt(0.36f, 0.36f, 0.75f).sub(0.58f).vib(0.0f).detune(0.30f)
            .reverb(0.05f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.15f).drive(0.25f).macro(6));
        add(B("DX Ballad Keys", "KEYS", kEraSynth, "FM electric piano with chorus and tremolo")
            .wave(kWaveFmKeys).dyn(0.62f).env(0.0f, 0.66f, 0.20f, 0.42f)
            .filt(0.90f, 0.05f, 0.15f).sub(0.0f).vib(0.05f).detune(0.38f).width(0.72f)
            .reverb(0.40f, kRevHall).delay(0.12f, kDlyTape).chorus(0.55f).trem(0.22f).tape(0.12f).macro(2));
        add(B("Stadium Brass", "BRASS", kEraSynth, "Big polysynth brass stab")
            .wave(kWaveChipSaw).dyn(0.60f).env(0.09f, 0.46f, 0.82f, 0.26f)
            .filt(0.48f, 0.20f, 0.62f).sub(0.20f).vib(0.18f).detune(0.60f).width(0.70f)
            .reverb(0.36f, kRevHall).delay(0.12f, kDlyPing).chorus(0.36f).macro(0));
        add(B("Glass Pad 84", "PAD", kEraSynth, "Phased pulse-width pad")
            .wave(kWavePwm).dyn(0.30f).env(0.52f, 0.60f, 0.92f, 0.62f)
            .filt(0.62f, 0.25f, 0.20f).sub(0.20f).vib(0.20f).detune(0.60f).width(0.75f)
            .reverb(0.60f, kRevHall).delay(0.25f, kDlyTape).chorus(0.50f).phaser(0.45f).macro(3));
        add(B("Tubular Night Bell", "BELL", kEraSynth, "FM tubular bell with long tail")
            .wave(kWaveBell).dyn(0.55f).env(0.0f, 0.82f, 0.0f, 0.76f)
            .filt(0.92f, 0.05f, 0.10f).sub(0.0f).vib(0.0f).detune(0.30f).width(0.66f)
            .reverb(0.55f, kRevHall).delay(0.30f, kDlyPing).chorus(0.20f).macro(0));
        add(B("VHS Dream Pad", "PAD", kEraSynth, "Wobbly worn-tape string pad")
            .wave(kWaveSoftStr).harm(kHarmOctave).dyn(0.55f).env(0.55f, 0.60f, 0.92f, 0.66f)
            .filt(0.60f, 0.15f, 0.20f).sub(0.25f).vib(0.15f).detune(0.65f).width(0.72f)
            .reverb(0.55f, kRevHall).delay(0.20f, kDlyTape).chorus(0.45f).tape(0.60f).drive(0.10f).macro(5));
        add(B("Arp Runner", "ARP", kEraSynth, "Minor-key synth arpeggio, resonant filter")
            .wave(kWaveChipSaw).harm(kHarmArpMinor).arp(12.0f).dyn(0.50f).env(0.0f, 0.36f, 0.60f, 0.22f)
            .filt(0.52f, 0.42f, 0.55f).sub(0.25f).vib(0.0f).detune(0.40f)
            .reverb(0.25f, kRevHall).delay(0.42f, kDlyPing).chorus(0.30f).macro(1));

        // ---- Medieval: plucked, bowed, blown, sung ----
        add(B("Minstrel Lute", "PLUCKED", kEraMedieval, "Double-strung plucked lute")
            .wave(kWaveLute).dyn(0.50f).env(0.0f, 0.50f, 1.0f, 0.26f)
            .filt(0.86f, 0.05f, 0.10f).sub(0.0f).vib(0.0f).detune(0.36f)
            .reverb(0.30f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(0));
        add(B("Gothic Harp", "PLUCKED", kEraMedieval, "Soft gut-string harp, long ring")
            .wave(kWaveLute).dyn(0.28f).env(0.0f, 0.78f, 1.0f, 0.55f)
            .filt(0.80f, 0.05f, 0.10f).sub(0.0f).vib(0.0f).detune(0.15f).width(0.68f)
            .reverb(0.50f, kRevHall).delay(0.12f, kDlySlap).chorus(0.10f).macro(0));
        add(B("Royal Harpsichord", "PLUCKED", kEraMedieval, "Bright quill pluck with an octave stop")
            .wave(kWaveLute).harm(kHarmOctave).dyn(0.92f).env(0.0f, 0.42f, 1.0f, 0.12f)
            .filt(0.96f, 0.05f, 0.05f).sub(0.0f).vib(0.0f).detune(0.20f)
            .reverb(0.34f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(0));
        add(B("Tavern Recorder", "WINDS", kEraMedieval, "Breathy wooden recorder")
            .wave(kWaveFlute).dyn(0.45f).env(0.07f, 0.30f, 0.95f, 0.16f)
            .filt(0.88f, 0.05f, 0.10f).sub(0.0f).vib(0.38f).noise(0.10f)
            .reverb(0.36f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(11));
        add(B("Cathedral Organ", "ORGAN", kEraMedieval, "Full pipe organ under a stone vault")
            .wave(kWaveOrgan).harm(kHarmOctave).dyn(0.72f).env(0.04f, 0.20f, 1.0f, 0.26f)
            .filt(0.92f, 0.05f, 0.05f).sub(0.50f).vib(0.0f).detune(0.30f).width(0.72f)
            .reverb(0.70f, kRevChurch).delay(0.0f, kDlyPing).chorus(0.12f).macro(0));
        add(B("Portative Organ", "ORGAN", kEraMedieval, "Small hand-pumped flute organ")
            .wave(kWaveOrgan).dyn(0.25f).env(0.03f, 0.20f, 1.0f, 0.12f)
            .filt(0.88f, 0.05f, 0.05f).sub(0.0f).vib(0.10f).noise(0.06f)
            .reverb(0.30f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).trem(0.14f).macro(4));
        add(B("Hurdy-Gurdy Drone", "STRINGS", kEraMedieval, "Buzzing wheel fiddle with a fourth-below drone")
            .wave(kWaveBowed).harm(kHarmOrganum).dyn(0.78f).env(0.09f, 0.30f, 0.98f, 0.20f)
            .filt(0.84f, 0.10f, 0.10f).sub(0.40f).vib(0.08f).detune(0.50f)
            .reverb(0.30f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.10f).drive(0.15f).macro(6));
        add(B("Viol Consort", "STRINGS", kEraMedieval, "Slow bowed viols")
            .wave(kWaveBowed).dyn(0.40f).env(0.32f, 0.40f, 0.95f, 0.40f)
            .filt(0.80f, 0.08f, 0.15f).sub(0.15f).vib(0.32f).detune(0.56f).width(0.68f)
            .reverb(0.46f, kRevHall).delay(0.0f, kDlyPing).chorus(0.25f).macro(0));
        add(B("Monk Choir", "VOICE", kEraMedieval, "Low chant; Dynamics moves the vowel from OO to AH")
            .wave(kWaveChoir).dyn(0.30f).env(0.36f, 0.40f, 0.98f, 0.52f)
            .filt(0.86f, 0.05f, 0.05f).sub(0.20f).vib(0.22f).detune(0.60f).width(0.70f)
            .reverb(0.75f, kRevChurch).delay(0.0f, kDlyPing).chorus(0.30f).macro(0));
        add(B("Shawm & Bagpipe", "WINDS", kEraMedieval, "Nasal double reed over a low drone")
            .wave(kWaveReed).dyn(0.60f).env(0.03f, 0.20f, 0.98f, 0.10f)
            .filt(0.88f, 0.10f, 0.05f).sub(0.50f).vib(0.15f).detune(0.30f)
            .reverb(0.26f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.10f).macro(6));
        add(B("Monastery Bells", "BELL", kEraMedieval, "Bronze bells ringing through a cloister")
            .wave(kWaveBell).dyn(0.40f).env(0.0f, 0.90f, 0.0f, 0.86f)
            .filt(0.90f, 0.05f, 0.05f).sub(0.20f).vib(0.0f).detune(0.50f).width(0.70f)
            .reverb(0.70f, kRevChurch).delay(0.10f, kDlyCanyon).chorus(0.10f).macro(0));

        // ---- SFX + kit: one-key game sounds ----
        add(B("Coin Pickup", "SFX", kEraSfx, "Two-note pickup blip")
            .wave(kWavePulse50).harm(kHarmCoin).arp(14.0f).dyn(0.50f).env(0.0f, 0.44f, 0.0f, 0.30f)
            .filt(0.96f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.10f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(8));
        add(B("Hero Jump", "SFX", kEraSfx, "Rising hop")
            .wave(kWavePulse25).dyn(0.50f).env(0.0f, 0.30f, 0.0f, 0.10f).sweep(-14.0f, 0.13f)
            .filt(0.94f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.06f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).macro(8));
        add(B("Laser Zap", "SFX", kEraSfx, "Falling pew")
            .wave(kWavePulse12).dyn(0.55f).env(0.0f, 0.32f, 0.0f, 0.10f).sweep(30.0f, 0.22f)
            .filt(0.96f, 0.10f, 0.0f).sub(0.0f).vib(0.0f).detune(0.20f)
            .reverb(0.10f, kRevRoom).delay(0.20f, kDlyPing).chorus(0.0f).crush(0.20f).macro(7));
        add(B("Power-Up", "SFX", kEraSfx, "Climbing arpeggio fanfare")
            .wave(kWavePulse50).harm(kHarmArpMajor).arp(20.0f).dyn(0.50f).env(0.0f, 0.62f, 0.30f, 0.22f)
            .sweep(-24.0f, 0.70f).filt(0.94f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.10f)
            .reverb(0.14f, kRevHall).delay(0.20f, kDlyPing).chorus(0.0f).macro(1));
        add(B("Critter Cry", "SFX", kEraSfx, "Pocket-creature battle cry")
            .wave(kWavePwm).dyn(0.92f).env(0.0f, 0.42f, 0.0f, 0.16f).sweep(9.0f, 0.40f)
            .filt(0.90f, 0.20f, 0.0f).sub(0.0f).vib(0.85f).noise(0.35f).detune(0.50f)
            .reverb(0.08f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f)
            .crush(0.35f).speaker(0.50f).drive(0.30f).macro(8));
        add(B("Heal Chime", "SFX", kEraSfx, "Sparkling recovery jingle")
            .wave(kWaveBell).harm(kHarmArpMajor).arp(11.0f).dyn(0.35f).env(0.0f, 0.58f, 0.0f, 0.50f)
            .filt(0.94f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.20f)
            .reverb(0.40f, kRevHall).delay(0.35f, kDly16Bit).chorus(0.15f).speaker(0.20f).macro(1));
        add(B("Menu Blip", "SFX", kEraSfx, "Cursor tick")
            .wave(kWavePulse25).dyn(0.50f).env(0.0f, 0.14f, 0.0f, 0.06f)
            .filt(0.92f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.04f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).speaker(0.30f).macro(8));
        add(B("Explosion", "SFX", kEraSfx, "Crunchy falling noise burst")
            .wave(kWaveNoise).dyn(0.20f).env(0.0f, 0.64f, 0.0f, 0.42f).sweep(24.0f, 0.90f)
            .filt(0.80f, 0.10f, 0.30f).sub(0.0f).vib(0.0f)
            .reverb(0.30f, kRevCave).delay(0.0f, kDlyPing).chorus(0.0f).crush(0.30f).drive(0.50f).macro(6));
        add(B("Chip Drum Kit", "DRUMS", kEraSfx, "12 drums, one per note name: C kick, D snare, F# hat...")
            .wave(kWaveDrums).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.12f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).crush(0.08f).macro(7));

        // ================= Expansion bank (slots 47..71) =================

        // ---- 80s chip ----
        add(B("Octave Bounce Bass", "BASS", kEraChip80, "Fast octave-flip square bass, home-computer style")
            .wave(kWavePulse50).harm(kHarmArpOctave).arp(25.0f).dyn(0.45f).env(0.0f, 0.32f, 0.78f, 0.12f)
            .filt(0.78f, 0.20f, 0.30f).sub(0.35f).vib(0.0f).detune(0.20f)
            .reverb(0.08f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).crush(0.08f).macro(7));
        add(B("Echo Canyon Lead", "LEAD", kEraChip80, "Thin 12.5% pulse lead lost in a long echo")
            .wave(kWavePulse12).dyn(0.50f).env(0.0f, 0.40f, 0.76f, 0.30f)
            .filt(0.88f, 0.12f, 0.30f).sub(0.10f).vib(0.42f).detune(0.30f)
            .reverb(0.34f, kRevHall).delay(0.46f, kDlyCanyon).chorus(0.15f).macro(1));
        add(B("Minor Key Arp", "ARP", kEraChip80, "Brooding minor-chord arpeggio, one key per chord")
            .wave(kWavePulse25).harm(kHarmArpMinor).arp(40.0f).dyn(0.58f).env(0.0f, 0.38f, 0.62f, 0.30f)
            .filt(0.82f, 0.22f, 0.40f).sub(0.15f).vib(0.10f).detune(0.40f)
            .reverb(0.30f, kRevHall).delay(0.32f, kDlyPing).chorus(0.25f).crush(0.10f).macro(1));
        add(B("Square Brass Stab", "BRASS", kEraChip80, "Square fifths with a swelling filter, 8-bit fanfare")
            .wave(kWavePulse50).harm(kHarmFifth).dyn(0.60f).env(0.06f, 0.44f, 0.70f, 0.22f)
            .filt(0.46f, 0.26f, 0.62f).sub(0.30f).vib(0.20f).detune(0.50f)
            .reverb(0.28f, kRevHall).delay(0.14f, kDlyPing).chorus(0.30f).crush(0.06f).macro(0));
        add(B("Chip Harmonica", "SOLO", kEraChip80, "Reedy pulse-width solo with breath and wobble")
            .wave(kWavePwm).dyn(0.22f).env(0.05f, 0.30f, 0.86f, 0.16f)
            .filt(0.66f, 0.18f, 0.25f).sub(0.05f).vib(0.46f).noise(0.08f).detune(0.30f)
            .reverb(0.24f, kRevRoom).delay(0.10f, kDlySlap).chorus(0.10f).trem(0.18f).macro(11));

        // ---- 90s chip ----
        add(B("Wave Channel Lead", "LEAD", kEraChip90, "Hollow 4-bit wavetable lead through a small speaker")
            .wave(kWaveGbWave).dyn(0.66f).env(0.0f, 0.34f, 0.80f, 0.18f)
            .filt(0.86f, 0.10f, 0.20f).sub(0.05f).vib(0.48f).detune(0.25f)
            .reverb(0.14f, kRevRoom).delay(0.26f, kDly16Bit).chorus(0.0f).speaker(0.35f).macro(8));
        add(B("Tracker Rock Organ", "ORGAN", kEraChip90, "Driven FM organ with an octave drawbar and tremolo")
            .wave(kWaveFm16).harm(kHarmOctave).dyn(0.30f).env(0.01f, 0.30f, 0.95f, 0.14f)
            .filt(0.80f, 0.12f, 0.15f).sub(0.30f).vib(0.10f).detune(0.40f)
            .reverb(0.26f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.30f).drive(0.30f).trem(0.20f).macro(6));
        add(B("Battle Brass Hit", "BRASS", kEraChip90, "Short FM brass hit in fifths, boss-fight energy")
            .wave(kWaveFm16).harm(kHarmFifth).dyn(0.85f).env(0.02f, 0.36f, 0.30f, 0.20f)
            .filt(0.74f, 0.20f, 0.50f).sub(0.30f).vib(0.10f).detune(0.40f)
            .reverb(0.38f, kRevHall).delay(0.12f, kDly16Bit).chorus(0.35f).macro(0));
        add(B("Handheld Glow Pad", "PAD", kEraChip90, "Soft sine-table pad, slow fade, gentle chorus")
            .wave(kWaveGbWave).dyn(0.0f).env(0.46f, 0.55f, 0.92f, 0.52f)
            .filt(0.70f, 0.10f, 0.15f).sub(0.15f).vib(0.22f).detune(0.62f).width(0.72f)
            .reverb(0.48f, kRevHall).delay(0.22f, kDly16Bit).chorus(0.62f).speaker(0.15f).macro(2));
        add(B("Pocket Kalimba", "MALLET", kEraChip90, "Short FM thumb-piano plinks with a 16-bit echo")
            .wave(kWaveBell).dyn(0.22f).env(0.0f, 0.46f, 0.0f, 0.32f)
            .filt(0.86f, 0.08f, 0.20f).sub(0.0f).vib(0.0f).detune(0.20f).width(0.64f)
            .reverb(0.30f, kRevRoom).delay(0.30f, kDly16Bit).chorus(0.15f).macro(1));

        // ---- Synth 80s ----
        add(B("Ensemble Strings 79", "STRINGS", kEraSynth, "Lush string machine with phaser and wide chorus")
            .wave(kWaveSoftStr).dyn(0.66f).env(0.30f, 0.50f, 0.92f, 0.50f)
            .filt(0.72f, 0.12f, 0.15f).sub(0.10f).vib(0.12f).detune(0.66f).width(0.80f)
            .reverb(0.42f, kRevHall).delay(0.12f, kDlyTape).chorus(0.78f).phaser(0.36f).macro(3));
        add(B("Supersaw Anthem", "LEAD", kEraSynth, "Huge detuned saw lead with an octave and tape echo")
            .wave(kWaveChipSaw).harm(kHarmOctave).dyn(0.62f).env(0.01f, 0.40f, 0.85f, 0.30f)
            .filt(0.78f, 0.22f, 0.30f).sub(0.10f).vib(0.30f).detune(0.80f).width(0.76f)
            .reverb(0.34f, kRevHall).delay(0.30f, kDlyTape).chorus(0.50f).drive(0.12f).macro(1));
        add(B("Neon Pluck", "PLUCK", kEraSynth, "Resonant saw pluck for sequenced lines")
            .wave(kWaveChipSaw).dyn(0.45f).env(0.0f, 0.30f, 0.0f, 0.24f)
            .filt(0.32f, 0.46f, 0.80f).sub(0.20f).vib(0.0f).detune(0.45f).width(0.66f)
            .reverb(0.26f, kRevHall).delay(0.38f, kDlyPing).chorus(0.30f).macro(1));
        add(B("Midnight Sub Bass", "BASS", kEraSynth, "Round square sub bass behind a closed filter")
            .wave(kWavePulse50).dyn(0.50f).env(0.0f, 0.36f, 0.80f, 0.14f)
            .filt(0.26f, 0.22f, 0.32f).sub(0.80f).vib(0.0f).detune(0.25f)
            .reverb(0.04f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.18f).macro(6));
        add(B("Machine Choir", "VOICE", kEraSynth, "Sampled-choir pad from an 80s workstation")
            .wave(kWaveChoir).dyn(0.72f).env(0.30f, 0.50f, 0.95f, 0.50f)
            .filt(0.80f, 0.08f, 0.10f).sub(0.10f).vib(0.18f).detune(0.66f).width(0.78f)
            .reverb(0.52f, kRevHall).delay(0.18f, kDlyTape).chorus(0.55f).tape(0.25f).phaser(0.15f).macro(2));

        // ---- Medieval ----
        add(B("Shepherd Pan Pipes", "WINDS", kEraMedieval, "Breathy pan flute with a long hall")
            .wave(kWaveFlute).dyn(0.78f).env(0.05f, 0.35f, 0.92f, 0.26f)
            .filt(0.84f, 0.05f, 0.10f).sub(0.0f).vib(0.26f).noise(0.06f)
            .reverb(0.48f, kRevHall).delay(0.10f, kDlySlap).chorus(0.0f).macro(0));
        add(B("Bright Psaltery", "PLUCKED", kEraMedieval, "Doubled metal strings with a shimmering ring")
            .wave(kWaveLute).dyn(0.82f).env(0.0f, 0.60f, 1.0f, 0.36f)
            .filt(0.92f, 0.08f, 0.10f).sub(0.0f).vib(0.0f).detune(0.55f).width(0.70f)
            .reverb(0.42f, kRevChurch).delay(0.12f, kDlySlap).chorus(0.20f).macro(0));
        add(B("Capped Crumhorn", "WINDS", kEraMedieval, "Buzzy capped reed from a renaissance band")
            .wave(kWaveReed).dyn(0.18f).env(0.03f, 0.20f, 0.96f, 0.10f)
            .filt(0.62f, 0.16f, 0.10f).sub(0.30f).vib(0.06f).detune(0.25f)
            .reverb(0.30f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.12f).macro(6));
        add(B("Abbey Choir AH", "VOICE", kEraMedieval, "Open AH choir with an octave, under a stone vault")
            .wave(kWaveChoir).harm(kHarmOctave).dyn(0.88f).env(0.40f, 0.40f, 0.98f, 0.56f)
            .filt(0.88f, 0.05f, 0.05f).sub(0.10f).vib(0.20f).detune(0.62f).width(0.74f)
            .reverb(0.78f, kRevChurch).delay(0.0f, kDlyPing).chorus(0.28f).macro(0));
        add(B("Rebec Fiddle", "STRINGS", kEraMedieval, "Solo bowed fiddle with a singing vibrato")
            .wave(kWaveBowed).dyn(0.66f).env(0.06f, 0.30f, 0.92f, 0.18f)
            .filt(0.86f, 0.12f, 0.12f).sub(0.05f).vib(0.44f).detune(0.30f)
            .reverb(0.36f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.10f).macro(11));

        // ---- SFX + kit ----
        add(B("Stadium Drum Kit", "DRUMS", kEraSfx, "Chip drums in a big room, with some grit")
            .wave(kWaveDrums).dyn(0.55f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.36f, kRevHall).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.28f).width(0.66f).macro(0));
        add(B("Handheld Crunch Kit", "DRUMS", kEraSfx, "Lo-fi drums through a crushed pocket speaker")
            .wave(kWaveDrums).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(0.92f, 0.10f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.08f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).crush(0.48f).speaker(0.45f).macro(7));
        add(B("Warp Portal", "SFX", kEraSfx, "Rising phased whoosh into a cave")
            .wave(kWavePwm).dyn(0.70f).env(0.0f, 0.70f, 0.40f, 0.50f).sweep(-24.0f, 1.20f)
            .filt(0.80f, 0.30f, 0.20f).sub(0.0f).vib(0.30f).noise(0.20f).detune(0.60f).width(0.75f)
            .reverb(0.45f, kRevCave).delay(0.30f, kDlyCanyon).chorus(0.20f).phaser(0.65f).macro(3));
        add(B("Red Alert Klaxon", "SFX", kEraSfx, "Two-tone alarm: hold a key to keep it wailing")
            .wave(kWavePulse25).harm(kHarmArpOctave).arp(4.0f).dyn(0.55f).env(0.0f, 0.30f, 1.0f, 0.12f)
            .filt(0.84f, 0.20f, 0.0f).sub(0.0f).vib(0.20f).detune(0.30f)
            .reverb(0.20f, kRevHall).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.20f).speaker(0.30f).macro(6));
        add(B("Bubble Pop", "SFX", kEraSfx, "Short upward blip, like a bubble bursting")
            .wave(kWaveNesTri).dyn(0.20f).env(0.0f, 0.22f, 0.0f, 0.08f).sweep(-9.0f, 0.06f)
            .filt(0.94f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.16f, kRevRoom).delay(0.12f, kDly16Bit).chorus(0.0f).macro(8));

        // ================= 1.2 bank (slots 72..79): drum kits and soft keys =================

        add(B("Boom 808 Kit", "DRUMS", kEraSfx, "Long sub kick, metallic hats and a ringing cymbal")
            .wave(kWaveDrums).expr(0.78f).kit(kKit808).dyn(0.55f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.10f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.12f).macro(6));
        add(B("NES Noise Kit", "DRUMS", kEraChip80, "Triangle kicks and toms with the console noise channel")
            .wave(kWaveDrums).kit(kKitNes).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.06f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).crush(0.12f).macro(7));
        add(B("Electro Box Kit", "DRUMS", kEraSynth, "Clicky drum-machine kick, bright snare and metal hats")
            .wave(kWaveDrums).expr(0.76f).kit(kKitElectro).dyn(0.55f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.18f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).drive(0.20f).width(0.62f).macro(6));
        add(B("Dusty Break Kit", "DRUMS", kEraSfx, "Soft, dark break-beat drums with a shaker on B")
            .wave(kWaveDrums).kit(kKitDusty).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(0.82f, 0.05f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.14f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).tape(0.35f).crush(0.10f).macro(5));
        add(B("Tavern Percussion", "DRUMS", kEraMedieval, "Frame drum, tabor, darbuka, tambourine, finger cymbals")
            .wave(kWaveDrums).expr(0.68f).kit(kKitTavern).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(0.96f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.30f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).width(0.62f).macro(0));
        add(B("War Drums", "DRUMS", kEraMedieval, "Great drums, timpani and a gong for battle scenes")
            .wave(kWaveDrums).expr(0.62f).kit(kKitWar).dyn(0.55f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.45f, kRevHall).delay(0.0f, kDlyPing).chorus(0.0f).width(0.70f).macro(0));
        add(B("Hand Drum Circle", "DRUMS", kEraSfx, "Congas, bongos, djembe, shakers and claves")
            .wave(kWaveDrums).expr(0.76f).kit(kKitHand).dyn(0.50f).env(0.0f, 0.40f, 1.0f, 0.20f)
            .filt(1.0f, 0.0f, 0.0f).sub(0.0f).vib(0.0f).detune(0.0f)
            .reverb(0.22f, kRevRoom).delay(0.0f, kDlyPing).chorus(0.0f).width(0.64f).macro(0));
        add(B("Felt Room Keys", "KEYS", kEraSynth, "Soft, warm piano-like keys for the ambient demo songs")
            .wave(kWaveFmKeys).dyn(0.30f).env(0.0f, 0.74f, 0.0f, 0.60f)
            .filt(0.64f, 0.04f, 0.18f).sub(0.10f).vib(0.0f).detune(0.15f).width(0.64f)
            .reverb(0.44f, kRevHall).delay(0.10f, kDlyTape).chorus(0.12f).tape(0.22f).macro(0));
    }
};

inline const PresetBank& presetBank() {
    static const PresetBank bank;
    return bank;
}

} // namespace detail

inline const FactoryPreset& factoryPreset(int index) {
    if (index < 0) index = 0;
    if (index >= kNumFactoryPresets) index = kNumFactoryPresets - 1;
    return detail::presetBank().presets[index];
}

inline int factoryPresetsBuilt() { return detail::presetBank().built; }

} // namespace PixelLabs
