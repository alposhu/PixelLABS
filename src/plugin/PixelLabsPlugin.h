#pragma once

#include "vst3/vst3_abi.h"
#include "PixelLabsIds.h"
#include "../dsp/RetroSynthEngine.h"
#include <atomic>
#include <memory>

namespace PixelLabs {

class PixelLabsPlugin : public Steinberg::Vst::IComponent,
                        public Steinberg::Vst::IAudioProcessor,
                        public Steinberg::Vst::IProcessContextRequirements,
                        public Steinberg::Vst::IEditController,
                        public Steinberg::Vst::IMidiMapping {
public:
    PixelLabsPlugin();
    virtual ~PixelLabsPlugin();

    // FUnknown
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // IPluginBase
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API terminate() override;

    // IComponent
    Steinberg::tresult PLUGIN_API getControllerClassId(Steinberg::TUID classId) override;
    Steinberg::tresult PLUGIN_API setIoMode(Steinberg::int32 mode) override;
    Steinberg::int32 PLUGIN_API getBusCount(Steinberg::int32 type, Steinberg::int32 dir) override;
    Steinberg::tresult PLUGIN_API getBusInfo(Steinberg::int32 type, Steinberg::int32 dir,
                                             Steinberg::int32 index, Steinberg::Vst::BusInfo& bus) override;
    Steinberg::tresult PLUGIN_API getRoutingInfo(Steinberg::Vst::RoutingInfo& inInfo,
                                                 Steinberg::Vst::RoutingInfo& outInfo) override;
    Steinberg::tresult PLUGIN_API activateBus(Steinberg::int32 type, Steinberg::int32 dir,
                                              Steinberg::int32 index, Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;

    // IAudioProcessor
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                     Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override;
    Steinberg::tresult PLUGIN_API getBusArrangement(Steinberg::int32 dir, Steinberg::int32 index,
                                                    Steinberg::Vst::SpeakerArrangement& arr) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) override;
    Steinberg::uint32 PLUGIN_API getLatencySamples() override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::uint32 PLUGIN_API getTailSamples() override;

    // IProcessContextRequirements
    Steinberg::uint32 PLUGIN_API getProcessContextRequirements() override;

    // IEditController
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
    Steinberg::int32 PLUGIN_API getParameterCount() override;
    Steinberg::tresult PLUGIN_API getParameterInfo(Steinberg::int32 paramIndex,
                                                   Steinberg::Vst::ParameterInfo& info) override;
    Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID id,
                                                        Steinberg::Vst::ParamValue valueNormalized,
                                                        Steinberg::Vst::String128 string) override;
    Steinberg::tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID id,
                                                        Steinberg::char16* string,
                                                        Steinberg::Vst::ParamValue& valueNormalized) override;
    Steinberg::Vst::ParamValue PLUGIN_API normalizedParamToPlain(Steinberg::Vst::ParamID id,
                                                                 Steinberg::Vst::ParamValue valueNormalized) override;
    Steinberg::Vst::ParamValue PLUGIN_API plainToNormalized(Steinberg::Vst::ParamID id,
                                                            Steinberg::Vst::ParamValue plainValue) override;
    Steinberg::Vst::ParamValue PLUGIN_API getParamNormalized(Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID id,
                                                     Steinberg::Vst::ParamValue value) override;
    Steinberg::tresult PLUGIN_API setComponentHandler(Steinberg::Vst::IComponentHandler* handler) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;

    // IMidiMapping
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(Steinberg::int32 busIndex, Steinberg::int16 channel,
                                                              Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                              Steinberg::Vst::ParamID& id) override;

private:
    std::atomic<uint32_t> refCount_{1};
    bool initialized_ = false;
    bool audioOutActive_ = true;
    bool eventInActive_ = true;

    RetroSynthEngine synth_;
    SharedUITelemetry telemetry_;
    Steinberg::Vst::IComponentHandler* componentHandler_ = nullptr;

    void syncPresetToAllParams(int presetIdx, bool notifyHost);
};

} // namespace PixelLabs
