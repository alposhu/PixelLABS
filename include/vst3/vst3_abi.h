#pragma once

#include <cstdint>
#include <cstring>

#pragma pack(push, 8)

namespace Steinberg {

using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using tresult = int32;
using TSize = int64;
using TBool = uint8;
using char8 = char;
using char16 = char16_t;
using FIDString = const char8*;

using TUID = uint8[16];

#define PLUGIN_API __stdcall

// Windows COM_COMPATIBLE=1 HRESULT codes
enum : tresult {
    kNoInterface     = static_cast<tresult>(0x80004002L),
    kResultOk        = static_cast<tresult>(0x00000000L),
    kResultTrue      = kResultOk,
    kResultFalse     = static_cast<tresult>(0x00000001L),
    kInvalidArgument = static_cast<tresult>(0x80070057L),
    kNotImplemented  = static_cast<tresult>(0x80004001L),
    kInternalError   = static_cast<tresult>(0x80004005L),
    kNotInitialized  = static_cast<tresult>(0x8000FFFFL),
    kOutOfMemory     = static_cast<tresult>(0x8007000EL)
};

// Windows COM_COMPATIBLE=1 UID byte layout macro
#define INLINE_UID_COM(l1, l2, l3, l4) \
    { \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l1)      ) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l1) >>  8) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l1) >> 16) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l1) >> 24) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l2) >> 16) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l2) >> 24) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l2)      ) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l2) >>  8) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l3) >> 24) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l3) >> 16) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l3) >>  8) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l3)      ) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l4) >> 24) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l4) >> 16) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l4) >>  8) & 0xFF), \
        static_cast<Steinberg::uint8>(((Steinberg::uint32)(l4)      ) & 0xFF)  \
    }

inline bool iidEqual(const TUID a, const TUID b) {
    return std::memcmp(a, b, 16) == 0;
}

inline void copyTUID(TUID dst, const TUID src) {
    std::memcpy(dst, src, 16);
}

class FUnknown {
public:
    virtual tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) = 0;
    virtual uint32 PLUGIN_API addRef() = 0;
    virtual uint32 PLUGIN_API release() = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x00000000, 0x00000000, 0xC0000000, 0x00000046);
};

class IBStream : public FUnknown {
public:
    enum IStreamSeekMode {
        kIBSeekSet = 0,
        kIBSeekCur,
        kIBSeekEnd
    };
    virtual tresult PLUGIN_API read(void* buffer, int32 numBytes, int32* numBytesRead = nullptr) = 0;
    virtual tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) = 0;
    virtual tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result = nullptr) = 0;
    virtual tresult PLUGIN_API tell(int64* pos) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0xC3BF6EA2, 0x30994752, 0x9B6BF990, 0x1EE33E9B);
};

class IPluginBase : public FUnknown {
public:
    virtual tresult PLUGIN_API initialize(FUnknown* context) = 0;
    virtual tresult PLUGIN_API terminate() = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x22888DDB, 0x156E45AE, 0x8358B348, 0x08190625);
};

struct PFactoryInfo {
    enum FactoryFlags {
        kNoFlags = 0,
        kClassesDiscardable = 1 << 0,
        kLicenseCheck = 1 << 1,
        kComponentNonDiscardable = 1 << 3,
        kUnicode = 1 << 4
    };
    char8 vendor[64];
    char8 url[256];
    char8 email[128];
    int32 flags;
};

struct PClassInfo {
    enum ClassCardinality {
        kManyInstances = 0x7FFFFFFF
    };
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
};

struct PClassInfo2 {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
    uint32 classFlags;
    char8 subCategories[128];
    char8 vendor[64];
    char8 version[64];
    char8 sdkVersion[64];
};

struct PClassInfoW {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char16 name[64];
    uint32 classFlags;
    char8 subCategories[128];
    char16 vendor[64];
    char16 version[64];
    char16 sdkVersion[64];
};

class IPluginFactory : public FUnknown {
public:
    virtual tresult PLUGIN_API getFactoryInfo(PFactoryInfo* info) = 0;
    virtual int32 PLUGIN_API countClasses() = 0;
    virtual tresult PLUGIN_API getClassInfo(int32 index, PClassInfo* info) = 0;
    virtual tresult PLUGIN_API createInstance(FIDString cid, FIDString _iid, void** obj) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x7A4D811C, 0x52114A1F, 0xAED9D2EE, 0x0B43BF9F);
};

class IPluginFactory2 : public IPluginFactory {
public:
    virtual tresult PLUGIN_API getClassInfo2(int32 index, PClassInfo2* info) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x0007B650, 0xF24B4C0B, 0xA464EDB9, 0xF00B2ABB);
};

class IPluginFactory3 : public IPluginFactory2 {
public:
    virtual tresult PLUGIN_API getClassInfoUnicode(int32 index, PClassInfoW* info) = 0;
    virtual tresult PLUGIN_API setHostContext(FUnknown* context) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x4555A2AB, 0xC1234E57, 0x9B122910, 0x36878931);
};

struct ViewRect {
    int32 left = 0;
    int32 top = 0;
    int32 right = 0;
    int32 bottom = 0;
    int32 getWidth() const { return right - left; }
    int32 getHeight() const { return bottom - top; }
};

class IPlugFrame;

class IPlugView : public FUnknown {
public:
    virtual tresult PLUGIN_API isPlatformTypeSupported(FIDString type) = 0;
    virtual tresult PLUGIN_API attached(void* parent, FIDString type) = 0;
    virtual tresult PLUGIN_API removed() = 0;
    virtual tresult PLUGIN_API onWheel(float distance) = 0;
    virtual tresult PLUGIN_API onKeyDown(char16 key, int16 keyCode, int16 modifiers) = 0;
    virtual tresult PLUGIN_API onKeyUp(char16 key, int16 keyCode, int16 modifiers) = 0;
    virtual tresult PLUGIN_API getSize(ViewRect* size) = 0;
    virtual tresult PLUGIN_API onSize(ViewRect* newSize) = 0;
    virtual tresult PLUGIN_API onFocus(TBool state) = 0;
    virtual tresult PLUGIN_API setFrame(IPlugFrame* frame) = 0;
    virtual tresult PLUGIN_API canResize() = 0;
    virtual tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x5BC32507, 0xD06049EA, 0xA6151B52, 0x2B755B29);
};

class IPlugFrame : public FUnknown {
public:
    virtual tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x367FAF01, 0xAFA94693, 0x8D4DA2A0, 0xED0882A3);
};

namespace Vst {

using ParamID = uint32;
using ParamValue = double;
using Sample32 = float;
using Sample64 = double;
using SampleRate = double;
using TSamples = int64;
using TQuarterNotes = double;
using SpeakerArrangement = uint64;
using Speaker = uint64;
using String128 = char16[128];
using UnitID = int32;
using ProgramListID = int32;
using CtrlNumber = int16;

namespace SpeakerArr {
    constexpr SpeakerArrangement kEmpty  = 0;
    constexpr SpeakerArrangement kMono   = 1ULL << 19;
    constexpr SpeakerArrangement kStereo = (1ULL << 0) | (1ULL << 1);
}

enum MediaTypes {
    kAudio = 0,
    kEvent,
    kNumMediaTypes
};

enum BusDirections {
    kInput = 0,
    kOutput
};

enum BusTypes {
    kMain = 0,
    kAux
};

struct BusInfo {
    int32 mediaType;
    int32 direction;
    int32 channelCount;
    String128 name;
    int32 busType;
    uint32 flags;
    enum BusFlags {
        kDefaultActive = 1 << 0,
        kIsControlVoltage = 1 << 1
    };
};

struct RoutingInfo {
    int32 mediaType;
    int32 busIndex;
    int32 channel;
};

enum IoModes {
    kSimple = 0,
    kAdvanced,
    kOfflineProcessing
};

enum ComponentFlags {
    kDistributable = 1 << 0,
    kSimpleModeSupported = 1 << 1
};

enum SymbolicSampleSizes {
    kSample32 = 0,
    kSample64
};

struct ProcessSetup {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 maxSamplesPerBlock;
    SampleRate sampleRate;
};

struct AudioBusBuffers {
    int32 numChannels;
    uint64 silenceFlags;
    union {
        Sample32** channelBuffers32;
        Sample64** channelBuffers64;
    };
};

struct FrameRate {
    uint32 framesPerSecond;
    uint32 flags;
};

struct Chord {
    uint8 keyNote;
    uint8 rootNote;
    int16 chordMask;
};

struct ProcessContext {
    enum StatesAndFlags {
        kPlaying = 1 << 1,
        kCycleActive = 1 << 2,
        kRecording = 1 << 3,
        kSystemTimeValid = 1 << 8,
        kContTimeValid = 1 << 17,
        kProjectTimeMusicValid = 1 << 9,
        kBarPositionValid = 1 << 11,
        kCycleValid = 1 << 12,
        kTempoValid = 1 << 10,
        kTimeSigValid = 1 << 13,
        kChordValid = 1 << 18,
        kSmpteValid = 1 << 14,
        kClockValid = 1 << 15
    };
    uint32 state;
    double sampleRate;
    TSamples projectTimeSamples;
    int64 systemTime;
    TSamples continousTimeSamples;
    TQuarterNotes projectTimeMusic;
    TQuarterNotes barPositionMusic;
    TQuarterNotes cycleStartMusic;
    TQuarterNotes cycleEndMusic;
    double tempo;
    int32 timeSigNumerator;
    int32 timeSigDenominator;
    Chord chord;
    int32 smpteOffsetSubframes;
    FrameRate frameRate;
    int32 samplesToNextClock;
};

class IParamValueQueue : public FUnknown {
public:
    virtual ParamID PLUGIN_API getParameterId() = 0;
    virtual int32 PLUGIN_API getPointCount() = 0;
    virtual tresult PLUGIN_API getPoint(int32 index, int32& sampleOffset, ParamValue& value) = 0;
    virtual tresult PLUGIN_API addPoint(int32 sampleOffset, ParamValue value, int32& index) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x01263A18, 0xED074F6F, 0x98C9D356, 0x4686F9BA);
};

class IParameterChanges : public FUnknown {
public:
    virtual int32 PLUGIN_API getParameterCount() = 0;
    virtual IParamValueQueue* PLUGIN_API getParameterData(int32 index) = 0;
    virtual IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id, int32& index) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0xA4779663, 0x0BB64A56, 0xB44384A8, 0x466FEB9D);
};

struct NoteOnEvent {
    int16 channel;
    int16 pitch;
    float tuning;
    float velocity;
    int32 length;
    int32 noteId;
};

struct NoteOffEvent {
    int16 channel;
    int16 pitch;
    float velocity;
    int32 noteId;
    float tuning;
};

struct DataEvent {
    uint32 size;
    uint32 type;
    const uint8* bytes;
};

struct PolyPressureEvent {
    int16 channel;
    int16 pitch;
    float pressure;
    int32 noteId;
};

struct NoteExpressionValueEvent {
    uint32 typeId;
    int32 noteId;
    double value;
};

struct NoteExpressionTextEvent {
    uint32 typeId;
    int32 noteId;
    uint32 textLen;
    const char16* text;
};

struct ChordEvent {
    int16 root;
    int16 bassNote;
    int16 mask;
    uint16 textLen;
    const char16* text;
};

struct ScaleEvent {
    int16 root;
    int16 mask;
    uint16 textLen;
    const char16* text;
};

struct LegacyMIDICCOutEvent {
    uint8 controlNumber;
    int8 channel;
    int8 value;
    int8 value2;
};

struct Event {
    int32 busIndex;
    int32 sampleOffset;
    TQuarterNotes ppqPosition;
    uint16 flags;
    enum EventFlags {
        kIsLive = 1 << 0,
        kUserReserved1 = 1 << 14,
        kUserReserved2 = 1 << 15
    };
    enum EventTypes {
        kNoteOnEvent = 0,
        kNoteOffEvent,
        kDataEvent,
        kPolyPressureEvent,
        kNoteExpressionValueEvent = 4,
        kNoteExpressionTextEvent,
        kChordEvent,
        kScaleEvent,
        kLegacyMIDICCOutEvent = 65535
    };
    uint16 type;
    union {
        NoteOnEvent noteOn;
        NoteOffEvent noteOff;
        DataEvent data;
        PolyPressureEvent polyPressure;
        NoteExpressionValueEvent noteExpressionValue;
        NoteExpressionTextEvent noteExpressionText;
        ChordEvent chord;
        ScaleEvent scale;
        LegacyMIDICCOutEvent midiCCOut;
    };
};

class IEventList : public FUnknown {
public:
    virtual int32 PLUGIN_API getEventCount() = 0;
    virtual tresult PLUGIN_API getEvent(int32 index, Event& e) = 0;
    virtual tresult PLUGIN_API addEvent(Event& e) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x3A2C4214, 0x346349FE, 0xB2C4F397, 0xB9695A44);
};

struct ProcessData {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 numSamples;
    int32 numInputs;
    int32 numOutputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;
    IParameterChanges* inputParameterChanges;
    IParameterChanges* outputParameterChanges;
    IEventList* inputEvents;
    IEventList* outputEvents;
    ProcessContext* processContext;
};

class IComponent : public IPluginBase {
public:
    virtual tresult PLUGIN_API getControllerClassId(TUID classId) = 0;
    virtual tresult PLUGIN_API setIoMode(int32 mode) = 0;
    virtual int32 PLUGIN_API getBusCount(int32 type, int32 dir) = 0;
    virtual tresult PLUGIN_API getBusInfo(int32 type, int32 dir, int32 index, BusInfo& bus) = 0;
    virtual tresult PLUGIN_API getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo) = 0;
    virtual tresult PLUGIN_API activateBus(int32 type, int32 dir, int32 index, TBool state) = 0;
    virtual tresult PLUGIN_API setActive(TBool state) = 0;
    virtual tresult PLUGIN_API setState(IBStream* state) = 0;
    virtual tresult PLUGIN_API getState(IBStream* state) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0xE831FF31, 0xF2D54301, 0x928EBBEE, 0x25697802);
};

class IAudioProcessor : public FUnknown {
public:
    virtual tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                  SpeakerArrangement* outputs, int32 numOuts) = 0;
    virtual tresult PLUGIN_API getBusArrangement(int32 dir, int32 index, SpeakerArrangement& arr) = 0;
    virtual tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) = 0;
    virtual uint32 PLUGIN_API getLatencySamples() = 0;
    virtual tresult PLUGIN_API setupProcessing(ProcessSetup& setup) = 0;
    virtual tresult PLUGIN_API setProcessing(TBool state) = 0;
    virtual tresult PLUGIN_API process(ProcessData& data) = 0;
    virtual uint32 PLUGIN_API getTailSamples() = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x42043F99, 0xB7DA453C, 0xA569E79D, 0x9AAEC33D);
};

class IProcessContextRequirements : public FUnknown {
public:
    enum Flags {
        kNeedSystemTime = 1 << 0,
        kNeedContinousTimeSamples = 1 << 1,
        kNeedProjectTimeMusic = 1 << 2,
        kNeedBarPositionMusic = 1 << 3,
        kNeedCycleMusic = 1 << 4,
        kNeedSamplesToNextClock = 1 << 5,
        kNeedTempo = 1 << 6,
        kNeedTimeSignature = 1 << 7,
        kNeedChord = 1 << 8,
        kNeedFrameRate = 1 << 9,
        kNeedTransportState = 1 << 10
    };
    virtual uint32 PLUGIN_API getProcessContextRequirements() = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x2A654303, 0xEF764E3D, 0x95B5FE83, 0x730EF6D0);
};

struct ParameterInfo {
    ParamID id;
    String128 title;
    String128 shortTitle;
    String128 units;
    int32 stepCount;
    ParamValue defaultNormalizedValue;
    UnitID unitId;
    int32 flags;
    enum ParameterFlags {
        kNoFlags = 0,
        kCanAutomate = 1 << 0,
        kIsReadOnly = 1 << 1,
        kIsWrapAround = 1 << 2,
        kIsList = 1 << 3,
        kIsHidden = 1 << 4,
        kIsProgramChange = 1 << 15,
        kIsBypass = 1 << 16
    };
};

class IComponentHandler : public FUnknown {
public:
    virtual tresult PLUGIN_API beginEdit(ParamID id) = 0;
    virtual tresult PLUGIN_API performEdit(ParamID id, ParamValue valueNormalized) = 0;
    virtual tresult PLUGIN_API endEdit(ParamID id) = 0;
    virtual tresult PLUGIN_API restartComponent(int32 flags) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0x93A0BEA3, 0x0BD045DB, 0x8E890B0C, 0xC1E46AC6);
};

class IEditController : public IPluginBase {
public:
    virtual tresult PLUGIN_API setComponentState(IBStream* state) = 0;
    virtual tresult PLUGIN_API setState(IBStream* state) = 0;
    virtual tresult PLUGIN_API getState(IBStream* state) = 0;
    virtual int32 PLUGIN_API getParameterCount() = 0;
    virtual tresult PLUGIN_API getParameterInfo(int32 paramIndex, ParameterInfo& info) = 0;
    virtual tresult PLUGIN_API getParamStringByValue(ParamID id, ParamValue valueNormalized, String128 string) = 0;
    virtual tresult PLUGIN_API getParamValueByString(ParamID id, char16* string, ParamValue& valueNormalized) = 0;
    virtual ParamValue PLUGIN_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized) = 0;
    virtual ParamValue PLUGIN_API plainToNormalized(ParamID id, ParamValue plainValue) = 0;
    virtual ParamValue PLUGIN_API getParamNormalized(ParamID id) = 0;
    virtual tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) = 0;
    virtual tresult PLUGIN_API setComponentHandler(IComponentHandler* handler) = 0;
    virtual IPlugView* PLUGIN_API createView(FIDString name) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0xDCD7BBE3, 0x7742448D, 0xA874AACC, 0x979C759E);
};

class IMidiMapping : public FUnknown {
public:
    virtual tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel,
                                                           CtrlNumber midiControllerNumber, ParamID& id) = 0;

    static constexpr TUID iid = INLINE_UID_COM(0xDF0FF9F7, 0x49B74669, 0xB63AB732, 0x7ADBF5E5);
};

enum ControllerNumbers : int16 {
    kCtrlModWheel = 1,
    kCtrlVolume = 7,
    kCtrlExpression = 11,
    kCtrlSustainOnOff = 64,
    kPitchBend = 129
};

inline void strToU16(char16* dst, const char* src, size_t maxLen = 128) {
    size_t i = 0;
    for (; i + 1 < maxLen && src[i] != '\0'; ++i) {
        dst[i] = static_cast<char16>(static_cast<unsigned char>(src[i]));
    }
    dst[i] = 0;
}

} // namespace Vst
} // namespace Steinberg

#pragma pack(pop)
