#include "PixelLabsPlugin.h"
#include "../presets/FactoryPresets.h"
#include "ParamNames.h"
#include "../gui/PixelLabsView.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

namespace PixelLabs {

using namespace Steinberg;
using namespace Steinberg::Vst;

struct ParamMeta {
    ParamID id;
    const char* title;
    const char* shortTitle;
    const char* units;
    int32 stepCount;
    double defaultNorm;
};

static const ParamMeta kParamTable[kNumParams] = {
    { kParamExpression,   "Expression",       "Expr",    "",  0, 0.85 },
    { kParamDynamics,     "Timbre",           "Timbre",  "",  0, 0.55 },
    { kParamReverb,       "Reverb Amount",    "Reverb",  "",  0, 0.25 },
    { kParamCrush,        "Pixel Crush",      "Crush",   "",  0, 0.00 },
    { kParamChorus,       "Ensemble Chorus",  "Chorus",  "",  0, 0.20 },
    { kParamDelay,        "Delay Amount",     "Delay",   "",  0, 0.15 },
    { kParamEnvShape,     "Contour",          "Contour", "",  0, 0.50 },
    { kParamVibrato,      "Pitch Vibrato",    "Vib",     "",  0, 0.15 },
    { kParamWaveform,     "Oscillator",       "Osc",     "",   kNumWaveforms - 1, 0.00 },
    { kParamHarmony,      "Harmony Mode",     "Harm",    "",   kNumHarmonyModes - 1, 0.00 },
    { kParamSubMix,       "Sub Oscillator",   "Sub",     "",  0, 0.20 },
    { kParamCutoff,       "Filter Cutoff",    "Cutoff",  "",  0, 0.80 },
    { kParamResonance,    "Filter Resonance", "Reso",    "",  0, 0.18 },
    { kParamPreset,       "Factory Preset",   "Preset",  "",   kNumFactoryPresets - 1, 0.00 },
    { kParamPitchBend,    "Pitch Bend",       "Bend",    "", 0, 0.50 },
    { kParamSustain,      "Sustain Pedal",    "Sust",    "",   1, 0.00 },
    { kParamPhaser,       "Phaser",           "Phaser",  "",  0, 0.00 },
    { kParamDrive,        "Drive",            "Drive",   "",  0, 0.00 },
    { kParamTape,         "Tape Wow",         "Tape",    "",  0, 0.00 },
    { kParamTremolo,      "Tremolo",          "Trem",    "",  0, 0.00 },
    { kParamSpeaker,      "Handheld Speaker", "Speaker", "",  0, 0.00 },
    { kParamWidth,        "Stereo Width",     "Width",   "",  0, 0.50 },
    { kParamReverbType,   "Reverb Type",      "RvbType", "",   kNumReverbTypes - 1, 1.0 / 3.0 },
    { kParamDelayType,    "Delay Type",       "DlyType", "",   kNumDelayTypes - 1, 0.00 },
    { kParamSweep,        "Pitch Sweep",      "Sweep",   "", 0, 0.50 },
    { kParamSweepTime,    "Sweep Time",       "SwpTime", "",  0, 0.30 },
    { kParamArpRate,      "Arp Rate",         "ArpRate", "", 0, 0.665 },
    { kParamGlide,        "Glide",            "Glide",   "",  0, 0.00 },
    { kParamNoise,        "Noise Layer",      "Noise",   "",  0, 0.00 },
    { kParamAttack,       "Attack",           "Atk",     "",  0, 0.02 },
    { kParamDecay,        "Decay",            "Dec",     "",  0, 0.40 },
    { kParamSustainLevel, "Sustain Level",    "SusLvl",  "",  0, 0.80 },
    { kParamRelease,      "Release",          "Rel",     "",  0, 0.30 },
    { kParamFilterEnv,    "Filter Envelope",  "FiltEnv", "",  0, 0.30 },
    { kParamDetune,       "Detune",           "Detune",  "",  0, 0.40 },
    { kParamVelSens,      "Velocity Sens",    "VelSens", "",  0, kDefaultVelSens },
    { kParamTouch,        "Touch Curve",      "Touch",   "",   kNumTouchCurves - 1, 2.0 / 3.0 }
};

PixelLabsPlugin::PixelLabsPlugin() {
    syncPresetToAllParams(0, false);
    for (int id : { kParamPitchBend, kParamSustain, kParamVelSens, kParamTouch }) {
        telemetry_.params[id].store(synth_.getParameterNormalized(id), std::memory_order_relaxed);
    }
}

void PixelLabsPlugin::notifyHostParam(int id, float value) {
    if (!componentHandler_) return;
    componentHandler_->beginEdit(id);
    componentHandler_->performEdit(id, value);
    componentHandler_->endEdit(id);
}

void PixelLabsPlugin::setCurrentFactory(int slot) {
    const FactoryPreset& fp = factoryPreset(slot);
    std::lock_guard<std::mutex> lock(soundMutex_);
    current_.key = SoundKey::factory(slot);
    current_.name = fp.name;
    current_.category = fp.category;
    current_.era = fp.era;
    current_.basePreset = slot;
    for (int i = 0; i < kNumParams; ++i) current_.ref[i] = fp.values[i];
}

void PixelLabsPlugin::applySoundValues(const float* values, int era, bool notifyHost) {
    for (int i = 0; i < kNumParams; ++i) {
        if (isPerformanceParam(i)) continue;
        const float v = std::clamp(values[i], 0.0f, 1.0f);
        synth_.setParameterNormalized(i, v);
        telemetry_.params[i].store(v, std::memory_order_relaxed);
        if (notifyHost) notifyHostParam(i, v);
    }
    synth_.setEra(era);
}

void PixelLabsPlugin::editorSetParam(int id, float value) {
    if (id < 0 || id >= kNumParams) return;
    value = std::clamp(value, 0.0f, 1.0f);
    synth_.setParameterNormalized(id, value);
    telemetry_.params[id].store(value, std::memory_order_relaxed);
    notifyHostParam(id, value);
}

void PixelLabsPlugin::editorLoadFactory(int slot) {
    syncPresetToAllParams(slot, true);
}

bool PixelLabsPlugin::editorLoadUser(uint32_t id) {
    UserSound s;
    if (!SoundLibrary::instance().getSound(id, s)) return false;
    applySoundValues(s.values, s.era, true);
    std::lock_guard<std::mutex> lock(soundMutex_);
    current_.key = SoundKey::user(id);
    current_.name = s.name;
    current_.category = s.category;
    current_.era = s.era;
    current_.basePreset = s.basePreset;
    for (int i = 0; i < kNumParams; ++i) current_.ref[i] = s.values[i];
    return true;
}

CurrentSoundInfo PixelLabsPlugin::editorCurrentSound() {
    std::lock_guard<std::mutex> lock(soundMutex_);
    return current_;
}

void PixelLabsPlugin::editorSetCurrentSound(const CurrentSoundInfo& info) {
    std::lock_guard<std::mutex> lock(soundMutex_);
    current_ = info;
}

PixelLabsPlugin::~PixelLabsPlugin() {
    if (componentHandler_) {
        componentHandler_->release();
        componentHandler_ = nullptr;
    }
}

void PixelLabsPlugin::syncPresetToAllParams(int presetIdx, bool notifyHost) {
    presetIdx = std::clamp(presetIdx, 0, kNumFactoryPresets - 1);
    synth_.applyPreset(presetIdx);
    telemetry_.currentPreset.store(presetIdx, std::memory_order_relaxed);
    telemetry_.activeMacro.store(factoryPreset(presetIdx).defaultMacro, std::memory_order_relaxed);

    for (int i = 0; i < kNumParams; ++i) {
        if (isPerformanceParam(i) && i != kParamPreset) continue; // untouched by a preset load
        float v = synth_.getParameterNormalized(i);
        telemetry_.params[i].store(v, std::memory_order_relaxed);
        if (notifyHost) notifyHostParam(i, v);
    }
    setCurrentFactory(presetIdx);
}

tresult PLUGIN_API PixelLabsPlugin::queryInterface(const TUID _iid, void** obj) {
    if (iidEqual(_iid, FUnknown::iid) ||
        iidEqual(_iid, IPluginBase::iid) ||
        iidEqual(_iid, IComponent::iid)) {
        addRef();
        *obj = static_cast<IComponent*>(this);
        return kResultOk;
    }
    if (iidEqual(_iid, IAudioProcessor::iid)) {
        addRef();
        *obj = static_cast<IAudioProcessor*>(this);
        return kResultOk;
    }
    if (iidEqual(_iid, IProcessContextRequirements::iid)) {
        addRef();
        *obj = static_cast<IProcessContextRequirements*>(this);
        return kResultOk;
    }
    if (iidEqual(_iid, IEditController::iid)) {
        addRef();
        *obj = static_cast<IEditController*>(this);
        return kResultOk;
    }
    if (iidEqual(_iid, IMidiMapping::iid)) {
        addRef();
        *obj = static_cast<IMidiMapping*>(this);
        return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
}

uint32 PLUGIN_API PixelLabsPlugin::addRef() {
    return ++refCount_;
}

uint32 PLUGIN_API PixelLabsPlugin::release() {
    uint32 r = --refCount_;
    if (r == 0) delete this;
    return r;
}

tresult PLUGIN_API PixelLabsPlugin::initialize(FUnknown* /*context*/) {
    if (initialized_) return kResultOk;
    initialized_ = true;
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::terminate() {
    if (!initialized_) return kResultOk;
    if (componentHandler_) {
        componentHandler_->release();
        componentHandler_ = nullptr;
    }
    initialized_ = false;
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::getControllerClassId(TUID classId) {
    // As a SingleComponentEffect (implementing both IComponent and IEditController),
    // returning kNoInterface tells VST3 hosts to queryInterface(IEditController::iid) directly on this object.
    std::memset(classId, 0, sizeof(TUID));
    return kNoInterface;
}

tresult PLUGIN_API PixelLabsPlugin::setIoMode(int32 /*mode*/) {
    return kResultOk;
}

int32 PLUGIN_API PixelLabsPlugin::getBusCount(int32 type, int32 dir) {
    if (type == kAudio && dir == kOutput) return 1; // Stereo Main Audio Out
    if (type == kEvent && dir == kInput)  return 1; // MIDI Event In
    return 0;
}

tresult PLUGIN_API PixelLabsPlugin::getBusInfo(int32 type, int32 dir, int32 index, BusInfo& bus) {
    if (index != 0) return kInvalidArgument;
    std::memset(&bus, 0, sizeof(BusInfo));
    if (type == kAudio && dir == kOutput) {
        bus.mediaType = kAudio;
        bus.direction = kOutput;
        bus.channelCount = 2;
        strToU16(bus.name, "Stereo Out");
        bus.busType = kMain;
        bus.flags = BusInfo::kDefaultActive;
        return kResultOk;
    }
    if (type == kEvent && dir == kInput) {
        bus.mediaType = kEvent;
        bus.direction = kInput;
        bus.channelCount = 16;
        strToU16(bus.name, "MIDI In");
        bus.busType = kMain;
        bus.flags = BusInfo::kDefaultActive;
        return kResultOk;
    }
    return kInvalidArgument;
}

tresult PLUGIN_API PixelLabsPlugin::getRoutingInfo(RoutingInfo& /*inInfo*/, RoutingInfo& /*outInfo*/) {
    return kNotImplemented;
}

tresult PLUGIN_API PixelLabsPlugin::activateBus(int32 type, int32 dir, int32 index, TBool state) {
    if (index != 0) return kInvalidArgument;
    if (type == kAudio && dir == kOutput) {
        audioOutActive_ = (state != 0);
        return kResultOk;
    }
    if (type == kEvent && dir == kInput) {
        eventInActive_ = (state != 0);
        return kResultOk;
    }
    return kInvalidArgument;
}

tresult PLUGIN_API PixelLabsPlugin::setActive(TBool state) {
    if (!state) {
        synth_.reset();
    }
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::setState(IBStream* state) {
    if (!state) return kInvalidArgument;

    unsigned char raw[sizeof(StateSnapshot)] = {};
    int32 numRead = 0;
    if (state->read(raw, sizeof(raw), &numRead) != kResultOk || numRead < 8) return kResultOk;
    uint32_t magic = 0;
    std::memcpy(&magic, raw, sizeof(magic));

    auto restoreUi = [&](int32_t macro, int32_t theme, int32_t uiScale, int32_t tab, int32_t maxTab) {
        telemetry_.activeMacro.store(std::clamp(macro, 0, kNumMacros - 1), std::memory_order_relaxed);
        telemetry_.theme.store(std::clamp(theme, 0, kNumThemes - 1), std::memory_order_relaxed);
        telemetry_.uiScale.store(std::clamp(uiScale, 0, kNumUiScales - 1), std::memory_order_relaxed);
        telemetry_.eraTab.store(std::clamp(tab, 0, maxTab), std::memory_order_relaxed);
    };

    if (magic == 0x50584C33 && numRead >= static_cast<int32>(sizeof(StateSnapshot))) { // 'PXL3'
        StateSnapshot snap;
        std::memcpy(&snap, raw, sizeof(snap));
        if (snap.version < 4) {
            // Saved when there were 72 factory presets: re-spread the preset over today's list
            for (double* v : { &snap.params[kParamPreset], &snap.refParams[kParamPreset] }) {
                *v = listToNorm(normToList(*v, kNumFactoryPresetsV3), kNumFactoryPresets);
            }
        }
        const int pIdx = normToList(snap.params[kParamPreset], kNumFactoryPresets);
        synth_.applyPreset(pIdx);
        snap.params[kParamSustain] = 0.0; // a pedal is live input, never part of a saved sound
        for (int i = 0; i < kNumParams; ++i) {
            float v = static_cast<float>(std::clamp(snap.params[i], 0.0, 1.0));
            synth_.setParameterNormalized(i, v);
            telemetry_.params[i].store(v, std::memory_order_relaxed);
        }
        telemetry_.currentPreset.store(pIdx, std::memory_order_relaxed);
        restoreUi(snap.activeMacroIndex, snap.theme, snap.uiScale, snap.eraTab, kNumBrowserTabs - 1);

        CurrentSoundInfo info;
        snap.soundName[sizeof(snap.soundName) - 1] = '\0';
        snap.soundCategory[sizeof(snap.soundCategory) - 1] = '\0';
        info.key.kind = std::clamp(snap.soundKind, static_cast<int32_t>(kSoundFactory), static_cast<int32_t>(kSoundDetached));
        info.key.id = snap.soundId;
        info.name = snap.soundName;
        info.category = snap.soundCategory;
        info.era = std::clamp(snap.soundEra, 0, kNumEras - 1);
        info.basePreset = std::clamp(snap.basePreset, 0, kNumFactoryPresets - 1);
        for (int i = 0; i < kNumParams; ++i) info.ref[i] = static_cast<float>(std::clamp(snap.refParams[i], 0.0, 1.0));
        if (info.key.kind == kSoundFactory) {
            info.key.id = static_cast<uint32_t>(std::min<uint32_t>(info.key.id, kNumFactoryPresets - 1));
            if (info.name.empty()) info.name = factoryPreset(static_cast<int>(info.key.id)).name;
        } else if (info.key.kind == kSoundUser && !SoundLibrary::instance().hasSound(info.key.id)) {
            info.key.kind = kSoundDetached; // made on another machine, or deleted since
        }
        if (info.name.empty()) info.name = "UNTITLED";
        synth_.setEra(info.era);
        std::lock_guard<std::mutex> lock(soundMutex_);
        current_ = info;
        return kResultOk;
    }

    if (magic == 0x50584C32 && numRead >= static_cast<int32>(sizeof(StateSnapshotV2))) { // 'PXL2' (v2 projects)
        StateSnapshotV2 snap;
        std::memcpy(&snap, raw, sizeof(snap));
        // v2 had 47 presets, so the stored list position has to be re-read with that count
        const int pIdx = normToList(snap.params[kParamPreset], kNumFactoryPresetsV2);
        syncPresetToAllParams(pIdx, false);
        for (int i = 0; i < kNumParamsV2; ++i) {
            double v = std::clamp(snap.params[i], 0.0, 1.0);
            if (i == kParamPreset) v = listToNorm(pIdx, kNumFactoryPresets);
            else if (i == kParamSustain) v = 0.0;
            const float fv = static_cast<float>(v);
            synth_.setParameterNormalized(i, fv);
            telemetry_.params[i].store(fv, std::memory_order_relaxed);
        }
        restoreUi(snap.activeMacroIndex, snap.theme, snap.uiScale, snap.eraTab, kNumEras);
        return kResultOk;
    }

    if (magic == 0x50584C31 && numRead >= static_cast<int32>(sizeof(StateSnapshotV1))) { // 'PXL1' (v1 projects)
        StateSnapshotV1 old;
        std::memcpy(&old, raw, sizeof(old));
        // v1 had 8 presets: slots 0..7 are unchanged in the new bank.
        const int pIdx = normToList(old.params[kParamPreset], 8);
        syncPresetToAllParams(pIdx, false); // fills the new parameters with that preset's values

        static const int kV1Wave[6] = { kWavePulse50, kWavePulse25, kWavePulse12, kWaveNesTri, kWaveChipSaw, kWaveFm16 };
        static const int kV1Harm[4] = { kHarmSingle, kHarmFifth, kHarmOctave, kHarmArpMajor };
        static const int kV1Macro[6] = { 0, 7, 2, 1, 10, 11 }; // reverb, crush, chorus, delay, contour, vibrato
        for (int i = 0; i < kNumParamsV1; ++i) {
            double v = std::clamp(old.params[i], 0.0, 1.0);
            if (i == kParamWaveform) v = listToNorm(kV1Wave[normToList(v, 6)], kNumWaveforms);
            else if (i == kParamHarmony) v = listToNorm(kV1Harm[normToList(v, 4)], kNumHarmonyModes);
            else if (i == kParamPreset) v = listToNorm(pIdx, kNumFactoryPresets);
            else if (i == kParamSustain) v = 0.0;
            const float fv = static_cast<float>(v);
            synth_.setParameterNormalized(i, fv);
            telemetry_.params[i].store(fv, std::memory_order_relaxed);
        }
        telemetry_.activeMacro.store(kV1Macro[std::clamp(old.activeMacroIndex, 0, 5)], std::memory_order_relaxed);
        telemetry_.theme.store(old.darkTheme ? 0 : kThemePaper, std::memory_order_relaxed);
    }
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::getState(IBStream* state) {
    if (!state) return kInvalidArgument;
    StateSnapshot snap;
    for (int i = 0; i < kNumParams; ++i) {
        snap.params[i] = telemetry_.params[i].load(std::memory_order_relaxed);
    }
    snap.activeMacroIndex = telemetry_.activeMacro.load(std::memory_order_relaxed);
    snap.theme = telemetry_.theme.load(std::memory_order_relaxed);
    snap.uiScale = telemetry_.uiScale.load(std::memory_order_relaxed);
    snap.eraTab = telemetry_.eraTab.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(soundMutex_);
        snap.soundKind = current_.key.kind;
        snap.soundId = current_.key.id;
        snap.soundEra = current_.era;
        snap.basePreset = current_.basePreset;
        std::snprintf(snap.soundName, sizeof(snap.soundName), "%s", current_.name.c_str());
        std::snprintf(snap.soundCategory, sizeof(snap.soundCategory), "%s", current_.category.c_str());
        for (int i = 0; i < kNumParams; ++i) snap.refParams[i] = current_.ref[i];
    }
    int32 numWritten = 0;
    state->write(&snap, sizeof(StateSnapshot), &numWritten);
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                       SpeakerArrangement* outputs, int32 numOuts) {
    if (numIns == 0 && numOuts == 1 && outputs && outputs[0] == SpeakerArr::kStereo) {
        return kResultTrue;
    }
    return kResultFalse;
}

tresult PLUGIN_API PixelLabsPlugin::getBusArrangement(int32 dir, int32 index, SpeakerArrangement& arr) {
    if (dir == kOutput && index == 0) {
        arr = SpeakerArr::kStereo;
        return kResultOk;
    }
    return kInvalidArgument;
}

tresult PLUGIN_API PixelLabsPlugin::canProcessSampleSize(int32 symbolicSampleSize) {
    if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) {
        return kResultTrue;
    }
    return kResultFalse;
}

uint32 PLUGIN_API PixelLabsPlugin::getLatencySamples() {
    return 0;
}

tresult PLUGIN_API PixelLabsPlugin::setupProcessing(ProcessSetup& setup) {
    synth_.prepare(setup.sampleRate, setup.maxSamplesPerBlock);
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::setProcessing(TBool /*state*/) {
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::process(ProcessData& data) {
    if (data.processContext && (data.processContext->state & ProcessContext::kTempoValid)) {
        synth_.setHostTempo(data.processContext->tempo);
    }

    // 1. Read incoming parameter automation changes
    if (data.inputParameterChanges) {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParamsChanged; ++i) {
            if (IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i)) {
                ParamID id = queue->getParameterId();
                int32 numPoints = queue->getPointCount();
                if (numPoints > 0 && id < kNumParams) {
                    int32 sampleOffset = 0;
                    ParamValue value = 0.0;
                    if (queue->getPoint(numPoints - 1, sampleOffset, value) == kResultOk) {
                        float fv = static_cast<float>(std::clamp(value, 0.0, 1.0));
                        synth_.setParameterNormalized(id, fv);
                        telemetry_.params[id].store(fv, std::memory_order_relaxed);
                    }
                }
            }
        }
    }

    // 2. Process incoming MIDI Note On / Note Off events
    if (data.inputEvents) {
        int32 numEvents = data.inputEvents->getEventCount();
        Event e = {};
        for (int32 i = 0; i < numEvents; ++i) {
            if (data.inputEvents->getEvent(i, e) == kResultOk) {
                if (e.type == Event::kNoteOnEvent) {
                    if (e.noteOn.velocity > 0.0f) {
                        synth_.noteOn(e.noteOn.pitch, e.noteOn.velocity, e.noteOn.noteId);
                        telemetry_.lastVelocity.store(std::clamp(static_cast<int>(e.noteOn.velocity * 127.0f + 0.5f), 1, 127),
                                                      std::memory_order_relaxed);
                    } else {
                        synth_.noteOff(e.noteOn.pitch, e.noteOn.noteId);
                    }
                } else if (e.type == Event::kNoteOffEvent) {
                    synth_.noteOff(e.noteOff.pitch, e.noteOff.noteId);
                }
            }
        }
    }

    // 3. Render stereo audio block
    if (data.numOutputs > 0 && data.outputs && data.numSamples > 0) {
        AudioBusBuffers& outBus = data.outputs[0];
        outBus.silenceFlags = 0;
        if (data.symbolicSampleSize == kSample32 && outBus.numChannels >= 1 && outBus.channelBuffers32) {
            float* outL = outBus.channelBuffers32[0];
            float* outR = (outBus.numChannels >= 2) ? outBus.channelBuffers32[1] : nullptr;
            synth_.renderBlock(outL, outR, data.numSamples, &telemetry_);
        } else if (data.symbolicSampleSize == kSample64 && outBus.numChannels >= 1 && outBus.channelBuffers64) {
            std::vector<float> tmpL(data.numSamples, 0.0f);
            std::vector<float> tmpR(data.numSamples, 0.0f);
            synth_.renderBlock(tmpL.data(), tmpR.data(), data.numSamples, &telemetry_);
            double* outL64 = outBus.channelBuffers64[0];
            double* outR64 = (outBus.numChannels >= 2) ? outBus.channelBuffers64[1] : nullptr;
            for (int32 s = 0; s < data.numSamples; ++s) {
                outL64[s] = static_cast<double>(tmpL[s]);
                if (outR64) outR64[s] = static_cast<double>(tmpR[s]);
            }
        }
    }

    return kResultOk;
}

uint32 PLUGIN_API PixelLabsPlugin::getTailSamples() {
    // Church / cave reverbs and the canyon delay ring for several seconds
    return 48000 * 6;
}

uint32 PLUGIN_API PixelLabsPlugin::getProcessContextRequirements() {
    return IProcessContextRequirements::kNeedTempo | IProcessContextRequirements::kNeedTransportState;
}

// =============================================================================
// IEditController Implementation
// =============================================================================

tresult PLUGIN_API PixelLabsPlugin::setComponentState(IBStream* state) {
    return setState(state);
}

int32 PLUGIN_API PixelLabsPlugin::getParameterCount() {
    return kNumParams;
}

tresult PLUGIN_API PixelLabsPlugin::getParameterInfo(int32 paramIndex, ParameterInfo& info) {
    if (paramIndex < 0 || paramIndex >= kNumParams) return kInvalidArgument;
    const auto& m = kParamTable[paramIndex];
    std::memset(&info, 0, sizeof(ParameterInfo));
    info.id = m.id;
    strToU16(info.title, m.title);
    strToU16(info.shortTitle, m.shortTitle);
    strToU16(info.units, m.units);
    info.stepCount = m.stepCount;
    info.defaultNormalizedValue = m.defaultNorm;
    info.unitId = 0;
    info.flags = ParameterInfo::kCanAutomate;
    if (paramIndex == kParamPreset) {
        info.flags |= ParameterInfo::kIsProgramChange | ParameterInfo::kIsList;
    } else if (m.stepCount > 0) {
        info.flags |= ParameterInfo::kIsList;
    }
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::getParamStringByValue(ParamID id, ParamValue valueNormalized, String128 string) {
    if (id >= kNumParams) return kInvalidArgument;
    char buf[64] = {};
    formatParamValue(static_cast<int>(id), valueNormalized, buf, sizeof(buf));
    strToU16(string, buf);
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::getParamValueByString(ParamID /*id*/, char16* /*string*/, ParamValue& /*valueNormalized*/) {
    return kNotImplemented;
}

ParamValue PLUGIN_API PixelLabsPlugin::normalizedParamToPlain(ParamID /*id*/, ParamValue valueNormalized) {
    return valueNormalized;
}

ParamValue PLUGIN_API PixelLabsPlugin::plainToNormalized(ParamID /*id*/, ParamValue plainValue) {
    return std::clamp(plainValue, 0.0, 1.0);
}

ParamValue PLUGIN_API PixelLabsPlugin::getParamNormalized(ParamID id) {
    if (id >= kNumParams) return 0.0;
    return telemetry_.params[id].load(std::memory_order_relaxed);
}

tresult PLUGIN_API PixelLabsPlugin::setParamNormalized(ParamID id, ParamValue value) {
    if (id >= kNumParams) return kInvalidArgument;
    float fv = static_cast<float>(std::clamp(value, 0.0, 1.0));
    if (id == kParamPreset) {
        int pIdx = std::clamp(static_cast<int>(std::round(fv * (kNumFactoryPresets - 1))), 0, kNumFactoryPresets - 1);
        syncPresetToAllParams(pIdx, false);
        return kResultOk;
    }
    synth_.setParameterNormalized(id, fv);
    telemetry_.params[id].store(fv, std::memory_order_relaxed);
    return kResultOk;
}

tresult PLUGIN_API PixelLabsPlugin::setComponentHandler(IComponentHandler* handler) {
    if (componentHandler_ == handler) return kResultOk;
    if (componentHandler_) componentHandler_->release();
    componentHandler_ = handler;
    if (componentHandler_) componentHandler_->addRef();
    return kResultOk;
}

IPlugView* PLUGIN_API PixelLabsPlugin::createView(FIDString name) {
    if (name && std::strcmp(name, "editor") == 0) {
        return new PixelLabsView(&telemetry_, this);
    }
    return nullptr;
}

tresult PLUGIN_API PixelLabsPlugin::getMidiControllerAssignment(int32 busIndex, int16 /*channel*/,
                                                                CtrlNumber midiControllerNumber, ParamID& id) {
    if (busIndex != 0) return kResultFalse;

    switch (midiControllerNumber) {
        // Wheels & Pedals on M-Audio Oxygen 61 MKV
        case kPitchBend:         id = kParamPitchBend;  return kResultTrue;
        case kCtrlModWheel:      id = kParamDynamics;   return kResultTrue; // Mod Wheel (CC1) -> LABS Dynamics Fader
        case kCtrlVolume:                                                   // Master Volume (CC7)
        case kCtrlExpression:    id = kParamExpression; return kResultTrue; // Expression (CC11) -> LABS Expression Fader
        case kCtrlSustainOnOff:  id = kParamSustain;    return kResultTrue; // Sustain Pedal (CC64)

        // M-Audio Oxygen 61 MKV Physical Faders 1-9 (Preset Mode CC12..CC20)
        case 12: id = kParamExpression; return kResultTrue; // Fader 1 -> Expression
        case 13: id = kParamDynamics;   return kResultTrue; // Fader 2 -> Dynamics
        case 14: id = kParamCutoff;     return kResultTrue; // Fader 3 -> Filter Cutoff
        case 15: id = kParamResonance;  return kResultTrue; // Fader 4 -> Filter Resonance
        case 16: id = kParamSubMix;     return kResultTrue; // Fader 5 -> Sub -1 Oct
        case 17: id = kParamReverb;     return kResultTrue; // Fader 6 -> Reverb Space
        case 18: id = kParamChorus;     return kResultTrue; // Fader 7 -> Ensemble Chorus
        case 19: id = kParamDelay;      return kResultTrue; // Fader 8 -> Chiptune Delay
        case 20: id = kParamEnvShape;   return kResultTrue; // Fader 9 -> Envelope Shape

        // M-Audio Oxygen 61 MKV Physical Rotary Knobs 1-8 (Preset Mode CC22..CC29)
        case 22: id = kParamReverb;     return kResultTrue; // Knob 1 -> Macro 1: Reverb Space
        case 23: id = kParamCrush;      return kResultTrue; // Knob 2 -> Macro 2: Pixel Crush
        case 24: id = kParamChorus;     return kResultTrue; // Knob 3 -> Macro 3: Ensemble Chorus
        case 25: id = kParamDelay;      return kResultTrue; // Knob 4 -> Macro 4: Chiptune Delay
        case 26: id = kParamEnvShape;   return kResultTrue; // Knob 5 -> Macro 5: Envelope Shape
        case 27: id = kParamVibrato;    return kResultTrue; // Knob 6 -> Macro 6: Pitch Vibrato
        case 28: id = kParamCutoff;     return kResultTrue; // Knob 7 -> Filter Cutoff
        case 29: id = kParamResonance;  return kResultTrue; // Knob 8 -> Filter Resonance

        // Standard GM / Synth Sound Controller CCs (CC71..CC76, CC91, CC93, CC94)
        case 5:  id = kParamGlide;      return kResultTrue; // Portamento time
        case 71: id = kParamResonance;  return kResultTrue;
        case 72: id = kParamRelease;    return kResultTrue;
        case 73: id = kParamAttack;     return kResultTrue;
        case 74: id = kParamCutoff;     return kResultTrue;
        case 75: id = kParamDecay;      return kResultTrue;
        case 76: id = kParamVibrato;    return kResultTrue;
        case 91: id = kParamReverb;     return kResultTrue;
        case 92: id = kParamTremolo;    return kResultTrue;
        case 93: id = kParamChorus;     return kResultTrue;
        case 94: id = kParamDelay;      return kResultTrue;
        case 95: id = kParamPhaser;     return kResultTrue;
        default: break;
    }
    return kResultFalse;
}

} // namespace PixelLabs
