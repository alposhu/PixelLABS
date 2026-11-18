// Pixel LABS installer: a single self-contained exe that carries the VST3 plugin
// as a resource and installs it into the standard VST3 folder.
//
//   PixelLABS-Setup.exe               interactive
//   PixelLABS-Setup.exe /S            silent, current user
//   PixelLABS-Setup.exe /S /allusers  silent, all users (needs admin)
//   PixelLABS-Setup.exe /uninstall    remove (add /S for silent, /allusers for the machine-wide copy)
//   PixelLABS-Setup.exe /S /dir=PATH  copy the bundle into a custom VST3 folder (no uninstall entry)
//
// Exit codes: 0 ok, 1 error, 2 cancelled.

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#include "../src/plugin/Version.h"
#include "resource.h"

namespace {

const wchar_t* kProductName = L"Pixel LABS";
const wchar_t* kUninstallKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PixelLABS";

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
const wchar_t* kVersion = WIDEN(PIXELLABS_VERSION_STR);
const wchar_t* kVendor = WIDEN(PIXELLABS_VENDOR);

struct Options {
    bool silent = false;
    bool allUsers = false;
    bool uninstall = false;
    std::wstring customDir;
};

std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p))) out = p;
    CoTaskMemFree(p);
    return out;
}

std::wstring vst3Root(bool allUsers) {
    // Per-user: %LOCALAPPDATA%\Programs\Common\VST3, all users: %CommonProgramFiles%\VST3
    if (allUsers) return knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3";
    return knownFolder(FOLDERID_UserProgramFilesCommon) + L"\\VST3";
}

std::wstring appDir(bool allUsers) {
    if (allUsers) return knownFolder(FOLDERID_ProgramFiles) + L"\\PixelLABS";
    return knownFolder(FOLDERID_UserProgramFiles) + L"\\PixelLABS";
}

std::wstring bundleDir(const std::wstring& root) { return root + L"\\PixelLABS.vst3"; }
std::wstring binaryPath(const std::wstring& root) {
    return bundleDir(root) + L"\\Contents\\x86_64-win\\PixelLABS.vst3";
}

bool fileExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool isElevated() {
    HANDLE token = nullptr;
    TOKEN_ELEVATION elev = {};
    DWORD len = 0;
    bool result = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &len)) result = elev.TokenIsElevated != 0;
        CloseHandle(token);
    }
    return result;
}

std::wstring selfPath() {
    wchar_t buf[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return buf;
}

void message(const Options& o, const std::wstring& text, bool error) {
    if (o.silent) return;
    MessageBoxW(nullptr, text.c_str(), kProductName, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

// Writes `data` to `path`. A DAW that has the plugin loaded keeps the old file locked;
// Windows still allows renaming a loaded DLL, so the old copy is moved out of the way.
bool writePluginFile(const std::wstring& path, const void* data, DWORD size, std::wstring& err) {
    if (fileExists(path) && !DeleteFileW(path.c_str())) {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring aside = std::wstring(tmp) + L"PixelLABS_old_" + std::to_wstring(GetTickCount64()) + L".vst3";
        if (!MoveFileExW(path.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            err = L"The existing plugin is in use and could not be replaced.\nClose your DAW and run the installer again.";
            return false;
        }
        MoveFileExW(aside.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); // best effort cleanup
    }
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        err = L"Could not write:\n" + path;
        return false;
    }
    DWORD written = 0;
    const bool ok = WriteFile(f, data, size, &written, nullptr) && written == size;
    CloseHandle(f);
    if (!ok) err = L"Writing the plugin failed:\n" + path;
    return ok;
}

void registerUninstall(bool allUsers, const std::wstring& uninstaller, const std::wstring& location, DWORD sizeKb) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, kUninstallKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    auto setStr = [&](const wchar_t* name, const std::wstring& v) {
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()),
                       static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t)));
    };
    auto setDword = [&](const wchar_t* name, DWORD v) {
        RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
    };
    const std::wstring scope = allUsers ? L" /allusers" : L"";
    setStr(L"DisplayName", L"Pixel LABS VST3 Instrument");
    setStr(L"DisplayVersion", kVersion);
    setStr(L"Publisher", kVendor);
    setStr(L"DisplayIcon", uninstaller);
    setStr(L"InstallLocation", location);
    setStr(L"UninstallString", L"\"" + uninstaller + L"\" /uninstall" + scope);
    setStr(L"QuietUninstallString", L"\"" + uninstaller + L"\" /uninstall /S" + scope);
    setDword(L"NoModify", 1);
    setDword(L"NoRepair", 1);
    setDword(L"EstimatedSize", sizeKb);
    RegCloseKey(key);
}

int install(const Options& o) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PLUGIN_BINARY), RT_RCDATA);
    HGLOBAL h = res ? LoadResource(nullptr, res) : nullptr;
    const void* data = h ? LockResource(h) : nullptr;
    const DWORD size = res ? SizeofResource(nullptr, res) : 0;
    if (!data || size == 0) {
        message(o, L"This installer is damaged (plugin payload missing).", true);
        return 1;
    }

    const std::wstring root = o.customDir.empty() ? vst3Root(o.allUsers) : o.customDir;
    const std::wstring target = binaryPath(root);
    const std::wstring targetDir = target.substr(0, target.find_last_of(L'\\'));
    const int mk = SHCreateDirectoryExW(nullptr, targetDir.c_str(), nullptr);
    if (mk != ERROR_SUCCESS && mk != ERROR_ALREADY_EXISTS && mk != ERROR_FILE_EXISTS) {
        message(o, L"Could not create the folder:\n" + targetDir, true);
        return 1;
    }
    std::wstring err;
    if (!writePluginFile(target, data, size, err)) {
        message(o, err, true);
        return 1;
    }

    if (o.customDir.empty()) {
        // Keep a copy of this exe as the uninstaller and register it with Windows.
        const std::wstring dir = appDir(o.allUsers);
        SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
        const std::wstring uninstaller = dir + L"\\PixelLABS-Uninstall.exe";
        if (_wcsicmp(uninstaller.c_str(), selfPath().c_str()) != 0) {
            CopyFileW(selfPath().c_str(), uninstaller.c_str(), FALSE);
        }
        registerUninstall(o.allUsers, uninstaller, bundleDir(root), size / 1024 + 1);
    }

    message(o, std::wstring(L"Pixel LABS ") + kVersion + L" is installed.\n\n" + bundleDir(root) +
               L"\n\nRestart your DAW (or re-scan plug-ins) and load \"Pixel LABS\" as an instrument.", false);
    return 0;
}

void removeBundle(const std::wstring& root) {
    const std::wstring bin = binaryPath(root);
    if (fileExists(bin) && !DeleteFileW(bin.c_str())) {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring aside = std::wstring(tmp) + L"PixelLABS_removed_" + std::to_wstring(GetTickCount64()) + L".vst3";
        if (MoveFileExW(bin.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            MoveFileExW(aside.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }
    const std::wstring b = bundleDir(root);
    RemoveDirectoryW((b + L"\\Contents\\x86_64-win").c_str());
    RemoveDirectoryW((b + L"\\Contents").c_str());
    RemoveDirectoryW(b.c_str());
}

int uninstall(const Options& o) {
    if (!o.silent) {
        const int r = MessageBoxW(nullptr, L"Remove the Pixel LABS VST3 plugin from this computer?",
                                  kProductName, MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return 2;
    }
    removeBundle(o.customDir.empty() ? vst3Root(o.allUsers) : o.customDir);
    RegDeleteKeyW(o.allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, kUninstallKey);

    // Delete the stored uninstaller; if that is us, let cmd remove it after we exit.
    const std::wstring dir = appDir(o.allUsers);
    const std::wstring uninstaller = dir + L"\\PixelLABS-Uninstall.exe";
    if (_wcsicmp(uninstaller.c_str(), selfPath().c_str()) == 0) {
        std::wstring cmd = L"cmd.exe /c ping 127.0.0.1 -n 3 >nul & del /f /q \"" + uninstaller + L"\" & rmdir \"" + dir + L"\"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    } else {
        DeleteFileW(uninstaller.c_str());
        RemoveDirectoryW(dir.c_str());
    }
    message(o, L"Pixel LABS has been removed.", false);
    return 0;
}

// Re-runs this exe elevated with the given arguments and returns its exit code.
int runElevated(const std::wstring& args) {
    const std::wstring self = selfPath();
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return 2; // user declined the UAC prompt
    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return static_cast<int>(code);
}

enum { kRadioUser = 1001, kRadioAll = 1002, kBtnInstall = 2001, kBtnUninstall = 2002 };

int interactive(Options& o) {
    const bool installedUser = fileExists(binaryPath(vst3Root(false)));
    const bool installedAll = fileExists(binaryPath(vst3Root(true)));

    const std::wstring userText = L"Just for me\n" + vst3Root(false);
    const std::wstring allText = L"All users (needs administrator)\n" + vst3Root(true);
    TASKDIALOG_BUTTON radios[] = { { kRadioUser, userText.c_str() }, { kRadioAll, allText.c_str() } };
    TASKDIALOG_BUTTON buttons[3];
    int nButtons = 0;
    buttons[nButtons++] = { kBtnInstall, (installedUser || installedAll) ? L"Update" : L"Install" };
    if (installedUser || installedAll) buttons[nButtons++] = { kBtnUninstall, L"Uninstall" };

    const std::wstring title = std::wstring(kProductName) + L" " + kVersion + L" Setup";
    const std::wstring content =
        L"A retro pixel-art synthesizer: 72 presets across 80s chip, 90s handheld, 80s synth, "
        L"medieval and game-SFX sounds, with 12 effects and a playable on-screen keyboard.\n\n"
        L"Choose where to install the VST3 plugin:";

    TASKDIALOGCONFIG cfg = { sizeof(cfg) };
    cfg.hInstance = GetModuleHandleW(nullptr);
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_HICON_MAIN;
    cfg.hMainIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON),
                                                  IMAGE_ICON, 32, 32, 0));
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainInstruction = L"Install the Pixel LABS VST3 instrument";
    cfg.pszContent = content.c_str();
    cfg.pRadioButtons = radios;
    cfg.cRadioButtons = 2;
    cfg.nDefaultRadioButton = (installedAll && !installedUser) ? kRadioAll : kRadioUser;
    cfg.pButtons = buttons;
    cfg.cButtons = nButtons;
    cfg.nDefaultButton = kBtnInstall;
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    cfg.pszFooter = L"Works in REAPER, Cubase, Bitwig, Ableton Live, FL Studio, Studio One and other VST3 hosts.";

    int button = 0, radio = 0;
    if (FAILED(TaskDialogIndirect(&cfg, &button, &radio, nullptr))) return 1;
    if (button != kBtnInstall && button != kBtnUninstall) return 2;

    o.allUsers = (radio == kRadioAll);
    o.uninstall = (button == kBtnUninstall);
    if (o.allUsers && !isElevated()) {
        return runElevated(o.uninstall ? L"/uninstall /allusers" : L"/allusers");
    }
    return o.uninstall ? uninstall(o) : install(o);
}

Options parseArgs() {
    Options o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool explicitAction = false;
    for (int i = 1; argv && i < argc; ++i) {
        std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/S") == 0 || _wcsicmp(a.c_str(), L"/silent") == 0) { o.silent = true; explicitAction = true; }
        else if (_wcsicmp(a.c_str(), L"/allusers") == 0) { o.allUsers = true; explicitAction = true; }
        else if (_wcsicmp(a.c_str(), L"/uninstall") == 0) { o.uninstall = true; explicitAction = true; }
        else if (_wcsnicmp(a.c_str(), L"/dir=", 5) == 0) { o.customDir = a.substr(5); explicitAction = true; }
    }
    LocalFree(argv);
    if (!explicitAction) o.silent = false;
    return o;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    Options o = parseArgs();
    const bool hasArgs = lpCmdLine && *lpCmdLine;
    int code;
    if (!hasArgs) {
        code = interactive(o);
    } else {
        while (!o.customDir.empty() && (o.customDir.back() == L'\\' || o.customDir.back() == L'/')) o.customDir.pop_back();
        if (o.allUsers && o.customDir.empty() && !isElevated()) {
            std::wstring args = std::wstring(o.uninstall ? L"/uninstall " : L"") + (o.silent ? L"/S " : L"") + L"/allusers";
            code = runElevated(args);
        } else {
            code = o.uninstall ? uninstall(o) : install(o);
        }
    }
    CoUninitialize();
    return code;
}
