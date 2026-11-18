#pragma once

#include "vst3/vst3_abi.h"
#include "../plugin/PixelLabsIds.h"
#include "EditorBridge.h"
#include <windows.h>
#include <atomic>
#include <string>
#include <vector>

namespace PixelLabs {

struct PixelCanvas;
struct Palette;

class PixelLabsView : public Steinberg::IPlugView {
public:
    // Logical canvas; the window is this times the zoom factor.
    static constexpr int kBaseWidth  = 960;
    static constexpr int kBaseHeight = 600;

    // Layout anchors (logical pixels) - also used by the test host to click things.
    static constexpr int kDemoBtnX = 700, kDemoBtnY = 26;
    static constexpr int kThemeBtnX = 800, kThemeBtnY = 26;
    static constexpr int kKeyboardTop = 446, kKeyboardBottom = 592;
    static constexpr int kKeyboardLeft = 12, kWhiteKeyW = 26;
    static constexpr int kFirstKey = 36, kLastKey = 96; // 61 keys, C2..C7

    static constexpr int kTabY = 58, kTabH = 24;
    static int tabX(int t) { return 16 + t * 932 / kNumBrowserTabs; }
    static int tabW(int t) { return tabX(t + 1) - tabX(t) - 4; }

    static constexpr int kListX = 16, kListY = 90, kListW = 240, kListH = 310;
    static constexpr int kRowsTop = 112, kRowH = 19, kVisibleRows = 13;
    static constexpr int kRowX = 24, kRowW = 216;
    static constexpr int kStarX = kRowX + kRowW - 7;            // centre of a row's star
    static constexpr int kActionY = 368, kActionH = 24, kActionW = 72;
    static constexpr int kSaveBtnX = 22, kSaveAsBtnX = 100, kBankBtnX = 178;
    static constexpr int kHeaderStarX = 280, kHeaderStarY = 22; // centre

    static constexpr int kTouchX = 508, kTouchW = 136, kVelX = 652, kVelW = 114, kStripY = 408;

    // Dialogs
    static constexpr int kNameDlgX = 240, kNameDlgY = 135, kNameDlgW = 480, kNameDlgH = 330;
    static constexpr int kNameOkX = kNameDlgX + 320, kNameCancelX = kNameDlgX + 190, kNameBtnY = kNameDlgY + 280;
    static constexpr int kConfirmX = 220, kConfirmY = 195, kConfirmW = 520, kConfirmH = 170;
    static constexpr int kConfirmBtnY = kConfirmY + 120, kConfirmBtnW = 112, kConfirmBtnGap = 10;
    static int confirmBtnX(int i, int n) {
        return kConfirmX + kConfirmW - 20 - n * kConfirmBtnW - (n - 1) * kConfirmBtnGap + i * (kConfirmBtnW + kConfirmBtnGap);
    }
    static constexpr int kPickX = 270, kPickY = 130, kPickW = 420, kPickH = 340;
    static constexpr int kPickRowsTop = kPickY + 58, kPickRowH = 22, kPickRows = 9;
    static constexpr int kPickDoneX = kPickX + kPickW - 130, kPickBtnY = kPickY + 292;

    static int zoomPercent(int scaleIdx);

    PixelLabsView(SharedUITelemetry* telemetry, IEditorBridge* bridge);
    virtual ~PixelLabsView();

    // FUnknown
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // IPlugView
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onWheel(float distance) override;
    Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
    Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame* frame) override;
    Steinberg::tresult PLUGIN_API canResize() override;
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override;

private:
    enum HitKind {
        kHitNone = 0,
        kHitSoundRow,      // a = packed SoundKey
        kHitPresetPrev,
        kHitPresetNext,
        kHitPresetList,    // scroll area
        kHitEraTab,        // a = browser tab
        kHitMacroBtn,      // a = macro index
        kHitBigDial,
        kHitStepPrev,      // a = param id
        kHitStepNext,      // a = param id
        kHitSlider,        // a = param id
        kHitKeyboard,
        kHitDemo,
        kHitTheme,
        kHitZoom,
        kHitOctDown,
        kHitOctUp,
        // Library
        kHitStar,          // a = packed SoundKey
        kHitHeaderStar,
        kHitSoundRename,   // a = user sound id
        kHitSoundDelete,   // a = user sound id
        kHitBankRow,       // a = bank id
        kHitBankNew,
        kHitBankBack,
        kHitBankRename,    // a = bank id
        kHitBankDelete,    // a = bank id
        kHitBankRemoveItem,// a = packed SoundKey
        kHitSave,
        kHitSaveAs,
        kHitAddToBank,
        kHitTouchPrev,
        kHitTouchNext,
        // Dialogs
        kHitModalBlock,
        kHitModalButton,   // a = DialogButton
        kHitNameKey,       // a = character (8 = delete, 127 = clear)
        kHitPickerBank,    // a = bank id
        kHitPickerNew
    };
    struct Hit { int x, y, w, h, kind, a; };

    enum ModalKind { kModalNone = 0, kModalName, kModalConfirm, kModalBankPicker };
    enum NamePurpose { kNameSaveSound = 0, kNameNewBank, kNameRenameSound, kNameRenameBank };
    enum AfterSave { kAfterNothing = 0, kAfterLoadPending, kAfterBankPicker };
    enum ConfirmPurpose { kConfirmUnsaved = 0, kConfirmDeleteSound, kConfirmDeleteBank };
    enum DialogButton { kBtnOk = 0, kBtnCancel, kBtnSave, kBtnSaveAs, kBtnDiscard, kBtnDelete, kBtnDone };

    struct Modal {
        int kind = kModalNone;
        // name entry
        int namePurpose = kNameSaveSound;
        std::string text;
        std::string title, subtitle;
        uint32_t targetId = 0;          // sound / bank being renamed or deleted
        int after = kAfterNothing;
        bool backToPicker = false;      // new bank made from the bank picker
        // confirm
        int confirmPurpose = kConfirmUnsaved;
        std::string line1, line2;
        std::vector<int> buttons;
        // a sound switch that waits on the unsaved-changes answer
        bool hasPending = false;
        SoundKey pending;
        // bank picker
        SoundKey pickerKey;
        int pickerScroll = 0;
    };

    std::atomic<uint32_t> refCount_{1};
    SharedUITelemetry* telemetry_ = nullptr;
    IEditorBridge* bridge_ = nullptr;
    Steinberg::IPlugFrame* plugFrame_ = nullptr;

    HWND hwnd_ = nullptr;
    HBITMAP dibBitmap_ = nullptr;
    HDC memDC_ = nullptr;
    uint32_t* pixelBuffer_ = nullptr;

    std::vector<Hit> hits_;
    std::vector<Hit> prevHits_;  // last frame's map, for the hover readout (drawn before later hits exist)
    int hoverX_ = -1, hoverY_ = -1;
    int frame_ = 0;

    // Drag state
    int dragKind_ = kHitNone;
    int dragParam_ = -1;
    int dragStartX_ = 0, dragStartY_ = 0;
    float dragStartVal_ = 0.0f;
    Hit dragHit_ = {};
    int mouseNote_ = -1;
    int mouseVel_ = 100;

    // Browser
    int listScroll_ = 0;
    SoundKey lastKeySeen_ = { -1, 0 };
    int lastTabSeen_ = -1;
    uint32_t openBankId_ = 0;   // BANKS tab: 0 = the list of banks

    // Library + current sound, refreshed every frame
    LibrarySnapshot lib_;
    CurrentSoundInfo cur_;
    bool modified_ = false;
    Modal modal_;
    bool drawingModal_ = false;
    std::string toast_;
    int toastFrames_ = 0;

    // Computer-keyboard playing
    int kbdOctave_ = 4;          // octave of the A key
    int kbdHeld_[256] = {};      // vk -> note + 1 (0 = not held)
    uint64_t lastNativeKeyMs_ = 0, lastHostKeyMs_ = 0;

    int zoom() const;            // percent
    void createOffscreenBuffer();
    void destroyOffscreenBuffer();
    void renderUI();
    void requestResize();

    // Drawing sections
    void drawHeader(PixelCanvas& c, const Palette& p);
    void drawEraTabs(PixelCanvas& c, const Palette& p);
    void drawPresetList(PixelCanvas& c, const Palette& p);
    void drawSoundRows(PixelCanvas& c, const Palette& p, const std::vector<SoundKey>& list, int tab);
    void drawBankRows(PixelCanvas& c, const Palette& p);
    void drawActionBar(PixelCanvas& c, const Palette& p);
    void drawHero(PixelCanvas& c, const Palette& p);
    void drawSoundPanel(PixelCanvas& c, const Palette& p);
    void drawStatusStrip(PixelCanvas& c, const Palette& p);
    void drawKeyboard(PixelCanvas& c, const Palette& p);
    void drawModal(PixelCanvas& c, const Palette& p);
    void drawNameDialog(PixelCanvas& c, const Palette& p);
    void drawConfirmDialog(PixelCanvas& c, const Palette& p);
    void drawBankPicker(PixelCanvas& c, const Palette& p);

    void addHit(int x, int y, int w, int h, int kind, int a = 0);
    const Hit* hitAt(int x, int y) const;
    bool hovered(int x, int y, int w, int h) const;

    float param(int id) const;
    void setParam(int id, float v);
    void stepList(int paramId, int count, int dir);
    void stepPreset(int dir);
    int currentEra() const;
    int browserTab() const;

    // Library
    void refreshState();
    bool computeModified() const;
    std::vector<SoundKey> browseList() const;
    std::string soundName(const SoundKey& k) const;
    int soundEra(const SoundKey& k) const;
    bool soundExists(const SoundKey& k) const;
    void currentValues(float* out) const;
    void requestLoad(const SoundKey& key);
    void doLoad(const SoundKey& key);
    void saveOverwrite();
    void openSaveAs(int after);
    void openNameDialog(int purpose, const std::string& title, const std::string& subtitle,
                        const std::string& text, uint32_t targetId);
    void openConfirm(int purpose, const std::string& title, const std::string& l1, const std::string& l2,
                     std::vector<int> buttons);
    void openBankPicker(const SoundKey& key);
    void closeModal();
    void commitName();
    void pressDialogButton(int button);
    void nameInput(int ch);      // printable char, 8 = backspace, 127 = clear
    void showToast(const std::string& text);
    void releaseHeldNotes();

    void handleMouseDown(int x, int y);
    void handleMouseMove(int x, int y);
    void handleMouseUp(int x, int y);
    void handleDoubleClick(int x, int y);
    void handleWheel(int x, int y, float steps);
    bool handleKey(int vk, bool down);
    bool handleModalKey(int vk);
    void handleChar(int ch, bool fromHost);
    int keyAt(int x, int y) const;
    int velocityAt(int note, int y) const;
    void playGuiNote(int note, bool on, int velocity = 100);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace PixelLabs
