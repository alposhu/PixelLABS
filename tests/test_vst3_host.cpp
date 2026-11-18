#include "vst3/vst3_abi.h"
#include "../src/plugin/PixelLabsIds.h"
// Header-only constants (layout anchors, preset count); the test links only against the DLL.
#include "../src/presets/FactoryPresets.h"
#include "../src/gui/PixelLabsView.h"

namespace PixelLabs {
static constexpr int kNumFactoryPresetsForTest = kNumFactoryPresets;
static constexpr int PixelLabsView_kDemoX = PixelLabsView::kDemoBtnX, PixelLabsView_kDemoY = PixelLabsView::kDemoBtnY;
static constexpr int PixelLabsView_kThemeX = PixelLabsView::kThemeBtnX, PixelLabsView_kThemeY = PixelLabsView::kThemeBtnY;
}
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

class SimpleEventList : public IEventList {
public:
    std::vector<Event> events;
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override {
        if (iidEqual(_iid, FUnknown::iid) || iidEqual(_iid, IEventList::iid)) {
            *obj = static_cast<IEventList*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getEventCount() override { return static_cast<int32>(events.size()); }
    tresult PLUGIN_API getEvent(int32 index, Event& e) override {
        if (index < 0 || index >= static_cast<int32>(events.size())) return kInvalidArgument;
        e = events[index];
        return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event& e) override {
        events.push_back(e);
        return kResultOk;
    }
};


class MemStream : public IBStream {
public:
    std::vector<unsigned char> data;
    size_t pos = 0;
    tresult PLUGIN_API queryInterface(const TUID, void** obj) override { *obj = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API read(void* buffer, int32 numBytes, int32* numBytesRead) override {
        int32 n = static_cast<int32>(std::min<size_t>(numBytes, data.size() - pos));
        std::memcpy(buffer, data.data() + pos, n);
        pos += n;
        if (numBytesRead) *numBytesRead = n;
        return kResultOk;
    }
    tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten) override {
        auto* b = static_cast<unsigned char*>(buffer);
        data.insert(data.end(), b, b + numBytes);
        if (numBytesWritten) *numBytesWritten = numBytes;
        return kResultOk;
    }
    tresult PLUGIN_API seek(int64 p, int32, int64* result) override { pos = static_cast<size_t>(p); if (result) *result = p; return kResultOk; }
    tresult PLUGIN_API tell(int64* p) override { if (p) *p = static_cast<int64>(pos); return kResultOk; }
};

static int g_failures = 0;
#define CHECK(cond, msg) do { if (cond) std::printf("  ok   %s\n", msg); else { std::printf("  FAIL %s\n", msg); ++g_failures; } } while (0)

static void renderBlocks(IAudioProcessor* proc, int numBlocks, int blockSamples,
                         std::vector<float>* outL, std::vector<float>* outR, IEventList* events = nullptr) {
    std::vector<float> bL(blockSamples), bR(blockSamples);
    for (int b = 0; b < numBlocks; ++b) {
        std::fill(bL.begin(), bL.end(), 0.0f);
        std::fill(bR.begin(), bR.end(), 0.0f);
        float* chans[2] = { bL.data(), bR.data() };
        AudioBusBuffers outBus = {};
        outBus.numChannels = 2;
        outBus.channelBuffers32 = chans;
        ProcessData pData = {};
        pData.symbolicSampleSize = kSample32;
        pData.numSamples = blockSamples;
        pData.numOutputs = 1;
        pData.outputs = &outBus;
        pData.inputEvents = (b == 0) ? events : nullptr;
        proc->process(pData);
        if (outL) outL->insert(outL->end(), bL.begin(), bL.end());
        if (outR) outR->insert(outR->end(), bR.begin(), bR.end());
    }
}

static float peakOf(const std::vector<float>& v) {
    float m = 0.0f;
    for (float x : v) m = std::max(m, std::fabs(x));
    return m;
}

static bool hasNaN(const std::vector<float>& v) {
    for (float x : v) if (!std::isfinite(x)) return true;
    return false;
}

static bool saveWindowBmp(HWND hwnd, int w, int h, const char* filename) {
    HDC hdcWin = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h; // bottom-up for standard BMP file
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((w * 3 + 3) & ~3);
    int imgSize = stride * h;
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm) return false;
    SelectObject(hdcMem, hbm);
    SendMessageW(hwnd, WM_PAINT, 0, 0);
    BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);

    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imgSize;

    std::ofstream out(filename, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    out.write(reinterpret_cast<const char*>(&bmi.bmiHeader), sizeof(bmi.bmiHeader));
    out.write(reinterpret_cast<const char*>(bits), imgSize);
    out.close();

    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWin);
    return true;
}

static void writeWavFile(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    int numSamples = static_cast<int>(L.size());
    int dataBytes = numSamples * 2 * sizeof(int16_t);
    std::ofstream f(path, std::ios::binary);
    auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };

    f.write("RIFF", 4);
    w32(36 + dataBytes);
    f.write("WAVEfmt ", 8);
    w32(16);
    w16(1); // PCM
    w16(2); // Stereo
    w32(sr);
    w32(sr * 4);
    w16(4);
    w16(16);
    f.write("data", 4);
    w32(dataBytes);
    for (int i = 0; i < numSamples; ++i) {
        int16_t sL = static_cast<int16_t>(std::clamp(L[i], -1.0f, 1.0f) * 32000.0f);
        int16_t sR = static_cast<int16_t>(std::clamp(R[i], -1.0f, 1.0f) * 32000.0f);
        f.write(reinterpret_cast<const char*>(&sL), 2);
        f.write(reinterpret_cast<const char*>(&sR), 2);
    }
}

static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static PixelLabs::StateSnapshot snapshotOf(IComponent* comp) {
    MemStream st;
    comp->getState(&st);
    PixelLabs::StateSnapshot s = {};
    if (st.data.size() >= sizeof(s)) std::memcpy(&s, st.data.data(), sizeof(s));
    return s;
}

// Peak of one held note at a given velocity (fresh voices, tails flushed).
static float notePeak(IAudioProcessor* proc, int pitch, float velocity) {
    SimpleEventList on;
    Event e = {};
    e.type = Event::kNoteOnEvent;
    e.noteOn.pitch = static_cast<int16>(pitch);
    e.noteOn.velocity = velocity;
    e.noteOn.noteId = 500;
    on.addEvent(e);
    std::vector<float> L, R;
    renderBlocks(proc, 30, 512, &L, &R, &on);
    SimpleEventList off;
    Event o = {};
    o.type = Event::kNoteOffEvent;
    o.noteOff.pitch = static_cast<int16>(pitch);
    o.noteOff.noteId = 500;
    off.addEvent(o);
    renderBlocks(proc, 1, 512, nullptr, nullptr, &off);
    renderBlocks(proc, 300, 512, nullptr, nullptr);
    // RMS of the held part (peaks wander with the random start phases)
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = 512 * 6; i < L.size(); ++i) { sum += static_cast<double>(L[i]) * L[i]; ++n; }
    return n ? static_cast<float>(std::sqrt(sum / n)) : 0.0f;
}

static std::string g_outDir = "build\\test-output";
static std::string outPath(const char* name) { return g_outDir + "\\" + name; }

// Usage: test_vst3_host.exe [path\to\PixelLABS.vst3 dll] [output dir]
int main(int argc, char** argv) {
    std::string vstPath = (argc > 1) ? argv[1] : "dist\\PixelLABS.vst3\\Contents\\x86_64-win\\PixelLABS.vst3";
    if (argc > 2) g_outDir = argv[2];
    CreateDirectoryA(g_outDir.c_str(), nullptr);
    // Keep the test's sound library away from the user's real one
    const std::string libDir = g_outDir + "\\test-library";
    const std::string libPath = libDir + "\\PixelLABS_Library.txt";
    CreateDirectoryA(libDir.c_str(), nullptr);
    DeleteFileA(libPath.c_str());
    SetEnvironmentVariableA("PIXELLABS_LIBRARY_DIR", libDir.c_str());
    const char* libFile = libPath.c_str();

    HMODULE hMod = LoadLibraryA(vstPath.c_str());
    if (!hMod) {
        std::printf("FAIL: Could not load VST3 DLL\n");
        return 1;
    }

    using InitFn = bool (PLUGIN_API*)();
    using GetFactoryFn = IPluginFactory* (PLUGIN_API*)();

    auto initDll = reinterpret_cast<InitFn>(GetProcAddress(hMod, "InitDll"));
    auto getFactory = reinterpret_cast<GetFactoryFn>(GetProcAddress(hMod, "GetPluginFactory"));
    if (!initDll || !getFactory || !initDll()) {
        std::printf("FAIL: Missing VST3 entrypoints\n");
        return 1;
    }

    IPluginFactory* factory = getFactory();
    PClassInfo info = {};
    factory->getClassInfo(0, &info);
    std::printf("Loaded VST3 Class: '%s' Category: '%s'\n", info.name, info.category);

    IComponent* comp = nullptr;
    if (factory->createInstance(reinterpret_cast<FIDString>(info.cid),
                                reinterpret_cast<FIDString>(IComponent::iid),
                                reinterpret_cast<void**>(&comp)) != kResultOk || !comp) {
        std::printf("FAIL: Could not create IComponent\n");
        return 1;
    }
    comp->initialize(nullptr);

    IAudioProcessor* proc = nullptr;
    IEditController* ctrl = nullptr;
    comp->queryInterface(IAudioProcessor::iid, reinterpret_cast<void**>(&proc));
    comp->queryInterface(IEditController::iid, reinterpret_cast<void**>(&ctrl));
    if (!proc || !ctrl) {
        std::printf("FAIL: Missing IAudioProcessor or IEditController\n");
        return 1;
    }

    std::printf("Parameters registered: %d\n", ctrl->getParameterCount());
    ProcessSetup setup{ kSimple, kSample32, 512, 48000.0 };
    proc->setupProcessing(setup);
    comp->setActive(1);
    proc->setProcessing(1);

    // 1. Verify Host MIDI NoteOn / NoteOff pipeline
    {
        SimpleEventList evList;
        Event on = {};
        on.type = Event::kNoteOnEvent;
        on.noteOn.pitch = 69; // A4
        on.noteOn.velocity = 0.85f;
        on.noteOn.noteId = 1;
        evList.addEvent(on);

        std::vector<float> tL(512, 0.0f), tR(512, 0.0f);
        float* chans[2] = { tL.data(), tR.data() };
        AudioBusBuffers outBus = { 2, 0, { chans } };
        ProcessData pData = {};
        pData.symbolicSampleSize = kSample32;
        pData.numSamples = 512;
        pData.numOutputs = 1;
        pData.outputs = &outBus;
        pData.inputEvents = &evList;
        proc->process(pData);

        Event off = {};
        off.type = Event::kNoteOffEvent;
        off.noteOff.pitch = 69;
        off.noteOff.noteId = 1;
        SimpleEventList offList;
        offList.addEvent(off);
        pData.inputEvents = &offList;
        proc->process(pData);
    }

    CHECK(ctrl->getParameterCount() == PixelLabs::kNumParams, "37 parameters registered");
    CHECK(ctrl->getParamNormalized(PixelLabs::kParamSustain) < 0.5, "a fresh instance starts with the pedal up");

    // 2. Every factory preset renders a held chord without NaNs or silence
    {
        const int numPresets = PixelLabs::kNumFactoryPresetsForTest;
        int silent = 0, broken = 0;
        for (int i = 0; i < numPresets; ++i) {
            ctrl->setParamNormalized(PixelLabs::kParamPreset, static_cast<double>(i) / (numPresets - 1));
            SimpleEventList ev;
            const int chord[3] = { 60, 64, 67 };
            for (int k = 0; k < 3; ++k) {
                Event on = {};
                on.type = Event::kNoteOnEvent;
                on.noteOn.pitch = static_cast<int16>(chord[k]);
                on.noteOn.velocity = 0.9f;
                on.noteOn.noteId = 100 + k;
                ev.addEvent(on);
            }
            std::vector<float> L, R;
            renderBlocks(proc, 40, 512, &L, &R, &ev);
            SimpleEventList offs;
            for (int k = 0; k < 3; ++k) {
                Event off = {};
                off.type = Event::kNoteOffEvent;
                off.noteOff.pitch = static_cast<int16>(chord[k]);
                off.noteOff.noteId = 100 + k;
                offs.addEvent(off);
            }
            renderBlocks(proc, 1, 512, nullptr, nullptr, &offs);
            renderBlocks(proc, 400, 512, nullptr, nullptr); // let tails die between presets
            const float pk = peakOf(L);
            String128 name = {};
            ctrl->getParamStringByValue(PixelLabs::kParamPreset, static_cast<double>(i) / (numPresets - 1), name);
            char an[129] = {};
            for (int c = 0; c < 128 && name[c]; ++c) an[c] = static_cast<char>(name[c]);
            std::printf("    preset %02d %-24s peak %.3f\n", i + 1, an, pk);
            if (hasNaN(L) || hasNaN(R)) ++broken;
            if (pk < 0.02f) ++silent;
        }
        CHECK(broken == 0, "no NaN/Inf output in any preset");
        CHECK(silent == 0, "every preset makes sound");
    }

    const double kLast = PixelLabs::kNumFactoryPresetsForTest - 1;

    // 2b. Keyboard touch: velocity sensitivity and curve
    {
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 8.0 / kLast); // Pulse Hero Lead: velocity only sets level
        // Dry and below the output limiter, so the level is linear in the voice gain
        for (int id : { PixelLabs::kParamReverb, PixelLabs::kParamDelay, PixelLabs::kParamChorus, PixelLabs::kParamVibrato })
            ctrl->setParamNormalized(id, 0.0);
        ctrl->setParamNormalized(PixelLabs::kParamExpression, 0.45);
        const float hardDef = notePeak(proc, 64, 1.0f), softDef = notePeak(proc, 64, 0.3f);
        const double expect = 0.35 + 0.65 * 0.3; // the original fixed response
        std::printf("    default touch: soft/hard %.3f (expect ~%.3f)\n", softDef / hardDef, expect);
        CHECK(std::fabs(softDef / hardDef - expect) < 0.08, "default touch keeps the original velocity response");

        ctrl->setParamNormalized(PixelLabs::kParamVelSens, 1.0);
        const float softFull = notePeak(proc, 64, 0.3f), hardFull = notePeak(proc, 64, 1.0f);
        std::printf("    100%% sens: soft/hard %.3f\n", softFull / hardFull);
        CHECK(softFull / hardFull < 0.42, "full sensitivity makes soft notes clearly quieter");

        ctrl->setParamNormalized(PixelLabs::kParamTouch, PixelLabs::listToNorm(PixelLabs::kTouchHeavy, PixelLabs::kNumTouchCurves));
        const float softHeavy = notePeak(proc, 64, 0.5f);
        ctrl->setParamNormalized(PixelLabs::kParamTouch, PixelLabs::listToNorm(PixelLabs::kTouchLight, PixelLabs::kNumTouchCurves));
        const float softLight = notePeak(proc, 64, 0.5f);
        CHECK(softLight > softHeavy * 1.5f, "light touch is louder than heavy touch at the same velocity");

        ctrl->setParamNormalized(PixelLabs::kParamTouch, PixelLabs::listToNorm(PixelLabs::kTouchOff, PixelLabs::kNumTouchCurves));
        const float offSoft = notePeak(proc, 64, 0.1f), offHard = notePeak(proc, 64, 1.0f);
        std::printf("    touch off: soft/hard %.3f\n", offSoft / offHard);
        CHECK(std::fabs(offSoft / offHard - 1.0f) < 0.05f, "touch off plays every velocity at the same level");

        ctrl->setParamNormalized(PixelLabs::kParamPreset, 20.0 / kLast);
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamTouch)) < 1e-6, "loading a preset keeps the touch setting");
        ctrl->setParamNormalized(PixelLabs::kParamTouch, PixelLabs::listToNorm(PixelLabs::kTouchNormal, PixelLabs::kNumTouchCurves));
        ctrl->setParamNormalized(PixelLabs::kParamVelSens, PixelLabs::kDefaultVelSens);
    }

    // 3. State round trip (PXL3), v2 projects (PXL2) and v1 projects (PXL1)
    {
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 30.0 / kLast);
        ctrl->setParamNormalized(PixelLabs::kParamPhaser, 0.66);
        MemStream st;
        comp->getState(&st);
        CHECK(st.data.size() == sizeof(PixelLabs::StateSnapshot), "getState writes a PXL3 snapshot");
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 0.0);
        st.pos = 0;
        comp->setState(&st);
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPhaser) - 0.66) < 1e-4, "PXL3 restores FX params");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPreset) - 30.0 / kLast) < 1e-4, "PXL3 restores preset slot");
        {
            const PixelLabs::StateSnapshot s = snapshotOf(comp);
            CHECK(s.soundKind == 0 && s.soundId == 30 && std::strcmp(s.soundName, PixelLabs::factoryPreset(30).name) == 0,
                  "PXL3 remembers which sound is loaded");
            CHECK(std::fabs(s.refParams[PixelLabs::kParamPhaser] - PixelLabs::factoryPreset(30).values[PixelLabs::kParamPhaser]) < 1e-6,
                  "the edit stays an edit after reload (stored values kept apart)");
        }

        PixelLabs::StateSnapshotV2 v2 = {};
        v2.magic = 0x50584C32;
        v2.version = 2;
        for (int i = 0; i < PixelLabs::kNumParamsV2; ++i) v2.params[i] = PixelLabs::factoryPreset(20).values[i];
        v2.params[PixelLabs::kParamPreset] = 20.0 / 46.0; // slot 21 of the old 47-preset bank
        v2.params[PixelLabs::kParamDelay] = 0.91;
        v2.eraTab = 3;
        MemStream old2;
        old2.write(&v2, sizeof(v2), nullptr);
        old2.pos = 0;
        comp->setState(&old2);
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPreset) - 20.0 / kLast) < 1e-4, "v2 preset slot maps to the same sound");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamDelay) - 0.91) < 1e-4, "v2 knob values survive");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamVelSens) - PixelLabs::kDefaultVelSens) < 1e-4,
              "v2 projects get the default touch");

        PixelLabs::StateSnapshotV1 v1 = {};
        v1.magic = 0x50584C31;
        v1.version = 1;
        for (double& d : v1.params) d = 0.5;
        v1.params[PixelLabs::kParamPreset] = 3.0 / 7.0;   // v1 slot 4 (Comet Zap Bass)
        v1.params[PixelLabs::kParamWaveform] = 1.0;       // v1 "16-Bit FM"
        v1.params[PixelLabs::kParamReverb] = 0.77;
        v1.activeMacroIndex = 1;                           // v1 crush
        MemStream old;
        old.write(&v1, sizeof(v1), nullptr);
        old.pos = 0;
        comp->setState(&old);
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPreset) - 3.0 / kLast) < 1e-4, "v1 preset slot maps to the same sound");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamWaveform) - 8.0 / 19.0) < 1e-4, "v1 FM waveform maps to 16-Bit FM");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamReverb) - 0.77) < 1e-4, "v1 knob values survive");
        CHECK(ctrl->getParamNormalized(PixelLabs::kParamSustain) < 0.5, "loading a project never leaves the pedal held");
        st.pos = 0;
        comp->setState(&st); // back to the PXL2 snapshot (AUTO skin)
    }

    // 4. Editor: era demos, skins and screenshots
    IPlugView* view = ctrl->createView("editor");
    CHECK(view != nullptr, "editor view created");
    std::vector<float> fullL, fullR;
    if (view) {
        ViewRect r = {};
        view->getSize(&r);
        std::printf("Created IPlugView editor: %dx%d\n", r.getWidth(), r.getHeight());

        HWND hostWnd = CreateWindowExW(0, L"STATIC", L"VST3HostTest", WS_POPUP | WS_VISIBLE,
                                       100, 100, r.getWidth(), r.getHeight(), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        view->attached(hostWnd, "HWND");
        HWND childWnd = GetWindow(hostWnd, GW_CHILD);
        CHECK(childWnd != nullptr, "editor window attached");

        auto click = [&](int x, int y) {
            SendMessageW(childWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
            SendMessageW(childWnd, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        };

        // A new instance opens in the GILDED skin
        CHECK(snapshotOf(comp).theme == PixelLabs::kThemeGilded, "a new instance opens with the GILDED skin");
        SendMessageW(childWnd, WM_TIMER, 1001, 0);
        saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath("ui_gilded.bmp").c_str());
        click(PixelLabs::PixelLabsView_kThemeX, PixelLabs::PixelLabsView_kThemeY); // GILDED -> AUTO
        CHECK(snapshotOf(comp).theme == PixelLabs::kThemeAuto, "the skin button wraps round to AUTO");

        // One preset per era -> its demo song. Skin is AUTO so each shot shows the era skin.
        struct Shot { int preset; const char* wav; const char* bmp; double seconds; };
        const Shot shots[] = {
            { 0,  "demo_80s_pixel_sunrise.wav",   "ui_80s_chip.bmp",  24.0 },
            { 14, "demo_90s_pocket_adventure.wav","ui_90s_chip.bmp",  20.0 },
            { 19, "demo_synth_neon_drive.wav",   "ui_synth_80s.bmp", 20.0 },
            { 27, "demo_medieval_greensleeves.wav","ui_medieval.bmp", 24.0 },
        };
        const int blockSamples = 1200; // 25 ms
        for (const Shot& sh : shots) {
            ctrl->setParamNormalized(PixelLabs::kParamPreset, sh.preset / kLast);
            click(PixelLabs::PixelLabsView_kDemoX, PixelLabs::PixelLabsView_kDemoY); // demo on
            std::vector<float> L, R;
            const int blocks = static_cast<int>(sh.seconds * 48000.0 / blockSamples);
            for (int b = 0; b < blocks; ++b) {
                renderBlocks(proc, 1, blockSamples, &L, &R);
                if (childWnd && b == blocks / 3) {
                    SendMessageW(childWnd, WM_MOUSEMOVE, 0, MAKELPARAM(700, 360)); // hover a slider
                    SendMessageW(childWnd, WM_TIMER, 1001, 0);
                    saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath(sh.bmp).c_str());
                }
            }
            click(PixelLabs::PixelLabsView_kDemoX, PixelLabs::PixelLabsView_kDemoY); // demo off
            renderBlocks(proc, 160, blockSamples, &L, &R); // tail
            const float pk = peakOf(L);
            std::printf("    %-34s %.1fs peak %.3f\n", sh.wav, L.size() / 48000.0, pk);
            CHECK(pk > 0.05f && !hasNaN(L), "demo song renders");
            writeWavFile(outPath(sh.wav).c_str(), L, R, 48000);
            fullL.insert(fullL.end(), L.begin(), L.end());
            fullR.insert(fullR.end(), R.begin(), R.end());
        }

        // Drum kit + on-screen keyboard: click C2 (kick) on the keyboard
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 46.0 / kLast);
        SendMessageW(childWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 580));
        std::vector<float> kL, kR;
        renderBlocks(proc, 8, 512, &kL, &kR);
        SendMessageW(childWnd, WM_TIMER, 1001, 0);
        saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath("ui_sfx_kit.bmp").c_str());
        SendMessageW(childWnd, WM_LBUTTONUP, 0, MAKELPARAM(24, 580));
        CHECK(peakOf(kL) > 0.05f, "clicking an on-screen key plays the kick");

        // Cycle skins (AUTO -> NES -> ... -> PAPER) and save the NEON and PAPER looks
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 23.0 / kLast);
        for (int t = 1; t <= 6; ++t) {
            click(PixelLabs::PixelLabsView_kThemeX, PixelLabs::PixelLabsView_kThemeY);
            if (t == 6) {
                SendMessageW(childWnd, WM_TIMER, 1001, 0);
                saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath("ui_skin_paper.bmp").c_str());
            }
        }
        click(PixelLabs::PixelLabsView_kThemeX, PixelLabs::PixelLabsView_kThemeY); // PAPER -> GILDED for the library shots

        // ---- 5. Library: save as, unsaved-changes prompt, favourites, banks ----
        using V = PixelLabs::PixelLabsView;
        auto frame = [&]() { SendMessageW(childWnd, WM_TIMER, 1001, 0); };
        auto clickF = [&](int x, int y) { frame(); click(x, y); frame(); };
        auto type = [&](const char* s) { for (const char* p = s; *p; ++p) SendMessageW(childWnd, WM_CHAR, static_cast<WPARAM>(*p), 0); };
        auto key = [&](int vk) { SendMessageW(childWnd, WM_KEYDOWN, vk, 0); SendMessageW(childWnd, WM_KEYUP, vk, 0); };
        auto shot = [&](const char* file) { frame(); saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath(file).c_str()); };
        const int actionCy = V::kActionY + V::kActionH / 2;
        auto confirmBtn = [&](int i, int n) { clickF(V::confirmBtnX(i, n) + V::kConfirmBtnW / 2, V::kConfirmBtnY + 15); };
        {
            ctrl->setParamNormalized(PixelLabs::kParamPreset, 19.0 / kLast); // Neon Sunset Lead
            ctrl->setParamNormalized(PixelLabs::kParamDelay, 0.90);           // tweak an effect
            ctrl->setParamNormalized(PixelLabs::kParamCrush, 0.40);
            clickF(V::kSaveAsBtnX + V::kSaveAsBtnW / 2, actionCy);               // SAVE AS -> name dialog
            key(VK_DELETE);                                                   // clear the suggestion
            type("my neon");
            shot("ui_save_as.bmp");
            key(VK_RETURN);
            frame();
            std::string lib = readFile(libFile);
            CHECK(lib.find("\tMY NEON\t") != std::string::npos, "SAVE AS writes the edited sound to the library file");
            CHECK(lib.find(" 5=900000 ") != std::string::npos, "the saved sound keeps the changed delay");
            PixelLabs::StateSnapshot s = snapshotOf(comp);
            CHECK(s.soundKind == 1 && std::strcmp(s.soundName, "MY NEON") == 0, "the saved copy becomes the loaded sound");
            const uint32_t myId = s.soundId;

            // Edit it again, then switch: the editor must ask first
            ctrl->setParamNormalized(PixelLabs::kParamChorus, 0.05);
            clickF(236 + 392 - 15, 28);                                       // next sound arrow
            shot("ui_unsaved_prompt.bmp");
            s = snapshotOf(comp);
            CHECK(s.soundKind == 1 && s.soundId == myId, "switching waits while the unsaved prompt is open");
            confirmBtn(0, 4);                                                 // SAVE (overwrite), then switch
            s = snapshotOf(comp);
            lib = readFile(libFile);
            CHECK(s.soundKind == 0, "after SAVE the next sound loads");
            CHECK(lib.find(" 4=50000 ") != std::string::npos, "SAVE overwrote the sound with the new chorus");

            // Edit the factory sound and switch with DON'T SAVE
            ctrl->setParamNormalized(PixelLabs::kParamReverb, 0.99);
            const uint32_t before = snapshotOf(comp).soundId;
            clickF(236 + 15, 28);                                             // previous sound arrow
            confirmBtn(1, 3);                                                 // DON'T SAVE
            s = snapshotOf(comp);
            CHECK(s.soundId != before || s.soundKind != 0, "DON'T SAVE switches without saving");
            CHECK(readFile(libFile) == lib, "DON'T SAVE leaves the library alone");

            // Same switch, but SAVE AS from the prompt: name it, then the switch happens
            ctrl->setParamNormalized(PixelLabs::kParamDrive, 0.70);
            clickF(236 + 392 - 15, 28);
            if (s.soundKind == 1) confirmBtn(1, 4); else confirmBtn(0, 3);    // SAVE AS...
            key(VK_DELETE);
            type("crunchy");
            clickF(V::kNameOkX + 70, V::kNameBtnY + 15);                      // SAVE button
            lib = readFile(libFile);
            CHECK(lib.find("\tCRUNCHY\t") != std::string::npos, "SAVE AS from the prompt stores the sound");
            CHECK(snapshotOf(comp).soundKind == 0, "...and then switches to the next sound");

            // Favourite the loaded sound with the header star
            clickF(V::kHeaderStarX, V::kHeaderStarY);
            CHECK(readFile(libFile).find("FAV\tF") != std::string::npos, "header star adds a favorite");

            // MY SOUNDS tab lists both saved sounds; load MY NEON from it
            clickF(V::tabX(PixelLabs::kTabUser) + 20, V::kTabY + 12);
            shot("ui_my_sounds.bmp");
            clickF(V::kRowX + 60, V::kRowsTop + 9);                           // first row
            s = snapshotOf(comp);
            CHECK(s.soundKind == 1 && std::strcmp(s.soundName, "MY NEON") == 0, "clicking a saved sound loads it");
            CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamChorus) - 0.05) < 1e-3, "...with its saved effects");

            // BANKS: make a bank, then put the loaded sound in it with + BANK
            clickF(V::tabX(PixelLabs::kTabBanks) + 20, V::kTabY + 12);
            clickF(V::kRowX + 60, V::kRowsTop + 9);                           // "+ NEW BANK"
            key(VK_DELETE);
            type("live set");
            key(VK_RETURN);
            frame();
            CHECK(readFile(libFile).find("BANK\t") != std::string::npos &&
                  readFile(libFile).find("\tLIVE SET\t") != std::string::npos, "a named bank is created");
            clickF(V::kBankBtnX + V::kBankBtnW / 2, actionCy);                // + BANK -> picker
            shot("ui_bank_picker.bmp");
            clickF(V::kPickX + 100, V::kPickRowsTop + 10);                    // first bank: LIVE SET
            clickF(V::kPickDoneX + 55, V::kPickBtnY + 15);                    // DONE
            lib = readFile(libFile);
            const size_t bankLine = lib.find("\tLIVE SET\t");
            CHECK(bankLine != std::string::npos && lib.find("U", bankLine) != std::string::npos,
                  "+ BANK adds the sound to the chosen bank");
            shot("ui_banks.bmp");

            // Clicking keycaps types too; host key path (onKeyDown) works as well
            clickF(V::kBankBtnX + V::kBankBtnW / 2, actionCy);                // picker again
            clickF(V::kPickX + 100, V::kPickRowsTop + 10 + V::kPickRowH);     // "+ NEW BANK..."
            key(VK_DELETE);
            clickF(V::kNameDlgX + 42 + 0 * 40 + 18, V::kNameDlgY + 110 + 26 + 11); // Q
            clickF(V::kNameDlgX + 42 + 0 * 40 + 18, V::kNameDlgY + 110 + 11);      // 1
            Sleep(60);
            view->onKeyDown('z', 0, 0);                                       // host key path
            SendMessageW(childWnd, WM_CHAR, 'z', 0);                          // the same keystroke echoed natively: dropped
            Sleep(60);
            key(VK_RETURN);
            frame();
            CHECK(readFile(libFile).find("\tQ1Z\t") != std::string::npos, "keycaps and host keys type into the name box");
            clickF(V::kPickDoneX + 55, V::kPickBtnY + 15);                    // close the picker

            // A project with a saved sound comes back with its name
            MemStream proj;
            comp->getState(&proj);
            ctrl->setParamNormalized(PixelLabs::kParamPreset, 0.0);
            proj.pos = 0;
            comp->setState(&proj);
            s = snapshotOf(comp);
            CHECK(s.soundKind == 1 && std::strcmp(s.soundName, "MY NEON") == 0, "project reload keeps the saved sound's name");

            // Computer-keyboard keys type into a dialog instead of playing notes
            renderBlocks(proc, 500, 512, nullptr, nullptr);                   // let every tail die
            clickF(V::kSaveAsBtnX + V::kSaveAsBtnW / 2, actionCy);
            key(VK_DELETE);
            std::vector<float> tailL, qL, qR;
            renderBlocks(proc, 20, 512, &tailL, nullptr);                     // echo tail still ringing
            SendMessageW(childWnd, WM_KEYDOWN, 'A', 0);
            renderBlocks(proc, 20, 512, &qL, &qR);
            SendMessageW(childWnd, WM_KEYUP, 'A', 0);
            std::printf("    typing in a dialog: tail %.5f -> %.5f\n", peakOf(tailL), peakOf(qL));
            CHECK(peakOf(qL) <= peakOf(tailL) * 1.2f + 1e-4f, "keys pressed in a dialog do not play notes");
            const std::string libBefore = readFile(libFile);
            key(VK_ESCAPE);
            frame();
            CHECK(snapshotOf(comp).soundKind == 1 && readFile(libFile) == libBefore, "ESC closes the dialog without saving");

            // Delete a saved sound from MY SOUNDS (hover shows the tools, then confirm)
            clickF(V::tabX(PixelLabs::kTabUser) + 20, V::kTabY + 12);
            SendMessageW(childWnd, WM_MOUSEMOVE, 0, MAKELPARAM(V::kRowX + 60, V::kRowsTop + V::kRowH + 9));
            clickF(V::kRowX + V::kRowW - 22, V::kRowsTop + V::kRowH + 9);    // X on row 2 (CRUNCHY)
            confirmBtn(0, 2);                                                 // DELETE
            lib = readFile(libFile);
            CHECK(lib.find("\tCRUNCHY\t") == std::string::npos && lib.find("\tMY NEON\t") != std::string::npos,
                  "deleting removes only that sound");
        }

        view->removed();
        view->release();
        DestroyWindow(hostWnd);
    }

    std::printf("Rendered %.2fs of demo audio, peak %.4f\n", fullL.size() / 48000.0, peakOf(fullL));
    writeWavFile(outPath("PixelLABS_Preview_AllEras.wav").c_str(), fullL, fullR, 48000);

    proc->release();
    ctrl->release();
    comp->terminate();
    comp->release();
    factory->release();
    FreeLibrary(hMod);
    if (g_failures == 0) std::printf("ALL VST3 HOST TESTS PASSED!\n");
    else std::printf("%d CHECK(S) FAILED\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
