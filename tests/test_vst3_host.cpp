#include "vst3/vst3_abi.h"
#include "../src/plugin/PixelLabsIds.h"

namespace PixelLabs {
// Mirrors of editor/preset constants (the test only links against the DLL).
static constexpr int kNumFactoryPresetsForTest = 47;
static constexpr int PixelLabsView_kDemoX = 700, PixelLabsView_kDemoY = 26;
static constexpr int PixelLabsView_kThemeX = 800, PixelLabsView_kThemeY = 26;
}
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <string>

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

static std::string g_outDir = "build\\test-output";
static std::string outPath(const char* name) { return g_outDir + "\\" + name; }

// Usage: test_vst3_host.exe [path\to\PixelLABS.vst3 dll] [output dir]
int main(int argc, char** argv) {
    std::string vstPath = (argc > 1) ? argv[1] : "dist\\PixelLABS.vst3\\Contents\\x86_64-win\\PixelLABS.vst3";
    if (argc > 2) g_outDir = argv[2];
    CreateDirectoryA(g_outDir.c_str(), nullptr);
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

    CHECK(ctrl->getParameterCount() == PixelLabs::kNumParams, "35 parameters registered");

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

    // 3. State round trip (PXL2) and v1 project migration (PXL1)
    {
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 30.0 / 46.0);
        ctrl->setParamNormalized(PixelLabs::kParamPhaser, 0.66);
        MemStream st;
        comp->getState(&st);
        CHECK(st.data.size() == sizeof(PixelLabs::StateSnapshot), "getState writes a PXL2 snapshot");
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 0.0);
        st.pos = 0;
        comp->setState(&st);
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPhaser) - 0.66) < 1e-4, "PXL2 restores new FX params");
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPreset) - 30.0 / 46.0) < 1e-4, "PXL2 restores preset slot");

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
        CHECK(std::fabs(ctrl->getParamNormalized(PixelLabs::kParamPreset) - 3.0 / 46.0) < 1e-4, "v1 preset slot maps to the same sound");
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
            ctrl->setParamNormalized(PixelLabs::kParamPreset, sh.preset / 46.0);
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
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 1.0);
        SendMessageW(childWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 580));
        std::vector<float> kL, kR;
        renderBlocks(proc, 8, 512, &kL, &kR);
        SendMessageW(childWnd, WM_TIMER, 1001, 0);
        saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath("ui_sfx_kit.bmp").c_str());
        SendMessageW(childWnd, WM_LBUTTONUP, 0, MAKELPARAM(24, 580));
        CHECK(peakOf(kL) > 0.05f, "clicking an on-screen key plays the kick");

        // Cycle skins (AUTO -> NES -> ... -> PAPER) and save the NEON and PAPER looks
        ctrl->setParamNormalized(PixelLabs::kParamPreset, 23.0 / 46.0);
        for (int t = 1; t <= 6; ++t) {
            click(PixelLabs::PixelLabsView_kThemeX, PixelLabs::PixelLabsView_kThemeY);
            if (t == 6) {
                SendMessageW(childWnd, WM_TIMER, 1001, 0);
                saveWindowBmp(childWnd, r.getWidth(), r.getHeight(), outPath("ui_skin_paper.bmp").c_str());
            }
        }
        click(PixelLabs::PixelLabsView_kThemeX, PixelLabs::PixelLabsView_kThemeY); // back to AUTO

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
