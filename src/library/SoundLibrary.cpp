#include "SoundLibrary.h"
#include "../presets/FactoryPresets.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace PixelLabs {

// =============================================================================
// Keys and snapshot helpers
// =============================================================================

std::string SoundKey::toString() const {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%c%lu", kind == kSoundUser ? 'U' : 'F', static_cast<unsigned long>(id));
    return buf;
}

bool SoundKey::fromString(const std::string& s, SoundKey& out) {
    if (s.size() < 2 || (s[0] != 'F' && s[0] != 'U')) return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(s.c_str() + 1, &end, 10);
    if (!end || *end != '\0') return false;
    out.kind = (s[0] == 'U') ? kSoundUser : kSoundFactory;
    out.id = static_cast<uint32_t>(v);
    if (out.kind == kSoundFactory && out.id >= static_cast<uint32_t>(kNumFactoryPresets)) return false;
    if (out.kind == kSoundUser && out.id == 0) return false;
    return true;
}

const UserSound* LibrarySnapshot::findSound(uint32_t id) const {
    for (const auto& s : sounds) if (s.id == id) return &s;
    return nullptr;
}

const SoundBank* LibrarySnapshot::findBank(uint32_t id) const {
    for (const auto& b : banks) if (b.id == id) return &b;
    return nullptr;
}

bool LibrarySnapshot::isFavorite(const SoundKey& k) const {
    return std::find(favorites.begin(), favorites.end(), k) != favorites.end();
}

// =============================================================================
// Names
// =============================================================================

bool SoundLibrary::isNameChar(char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') return true;
    return std::strchr("-.'&!#+()_,:/?*=%", c) != nullptr && c != '\0';
}

std::string SoundLibrary::sanitizeName(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        if (!isNameChar(c)) continue;
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue; // no leading / double spaces
        out.push_back(c);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.size() > kMaxNameLen) {
        out.resize(kMaxNameLen);
        while (!out.empty() && out.back() == ' ') out.pop_back();
    }
    return out;
}

static std::string withNumber(const std::string& base, int n) {
    const std::string suffix = " " + std::to_string(n);
    std::string b = base;
    if (b.size() + suffix.size() > SoundLibrary::kMaxNameLen) b.resize(SoundLibrary::kMaxNameLen - suffix.size());
    while (!b.empty() && b.back() == ' ') b.pop_back();
    return b + suffix;
}

// Splits "NEON LEAD 2" into "NEON LEAD" and 2 (n = 1 when there is no number).
static void splitNumber(const std::string& name, std::string& base, int& n) {
    base = name;
    n = 1;
    const size_t sp = name.find_last_of(' ');
    if (sp == std::string::npos || sp + 1 >= name.size()) return;
    for (size_t i = sp + 1; i < name.size(); ++i) if (name[i] < '0' || name[i] > '9') return;
    if (name.size() - sp - 1 > 4) return;
    base = name.substr(0, sp);
    n = std::atoi(name.c_str() + sp + 1);
}

// =============================================================================
// Library
// =============================================================================

SoundLibrary& SoundLibrary::instance() {
    static SoundLibrary lib;
    return lib;
}

SoundLibrary::SoundLibrary() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD n = GetEnvironmentVariableW(L"PIXELLABS_LIBRARY_DIR", buf, MAX_PATH * 2);
    if (n > 0 && n < MAX_PATH * 2) {
        dir_ = buf;
    } else {
        n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH * 2);
        if (n > 0 && n < MAX_PATH * 2) dir_ = std::wstring(buf) + L"\\PixelLABS";
        else dir_ = L".\\PixelLABS";
    }
    while (!dir_.empty() && (dir_.back() == L'\\' || dir_.back() == L'/')) dir_.pop_back();
    path_ = dir_ + L"\\PixelLABS_Library.txt";
    rng_ = static_cast<uint32_t>(GetTickCount64()) ^ (GetCurrentProcessId() * 2654435761u)
         ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    if (rng_ == 0) rng_ = 0x9E3779B9u;
}

std::wstring SoundLibrary::filePath() {
    std::lock_guard<std::mutex> lock(m_);
    return path_;
}

bool SoundLibrary::lastSaveFailed() {
    std::lock_guard<std::mutex> lock(m_);
    return saveFailed_;
}

static uint64_t fileStampOf(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
    return (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) | fad.ftLastWriteTime.dwLowDateTime;
}

void SoundLibrary::ensureLoadedLocked() {
    if (loaded_) return;
    loaded_ = true;
    loadLocked();
    lastPollMs_ = GetTickCount64();
}

void SoundLibrary::pollFileLocked() {
    const uint64_t now = GetTickCount64();
    if (now - lastPollMs_ < 1500) return;
    lastPollMs_ = now;
    const uint64_t stamp = fileStampOf(path_);
    if (stamp != 0 && stamp != fileStamp_) {
        loadLocked();
        ++revision_;
    }
}

uint64_t SoundLibrary::revision() {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    pollFileLocked();
    return revision_;
}

LibrarySnapshot SoundLibrary::snapshot() {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    LibrarySnapshot s;
    s.revision = revision_;
    s.sounds = sounds_;
    s.favorites = favorites_;
    s.banks = banks_;
    return s;
}

UserSound* SoundLibrary::findSoundLocked(uint32_t id) {
    for (auto& s : sounds_) if (s.id == id) return &s;
    return nullptr;
}

SoundBank* SoundLibrary::findBankLocked(uint32_t id) {
    for (auto& b : banks_) if (b.id == id) return &b;
    return nullptr;
}

bool SoundLibrary::getSound(uint32_t id, UserSound& out) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    if (const UserSound* s = findSoundLocked(id)) { out = *s; return true; }
    return false;
}

bool SoundLibrary::hasSound(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    return findSoundLocked(id) != nullptr;
}

uint32_t SoundLibrary::newIdLocked() {
    for (;;) {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        const uint32_t id = rng_ & 0x3FFFFFFFu; // fits the editor's packed hit codes
        if (id == 0 || findSoundLocked(id) || findBankLocked(id)) continue;
        return id;
    }
}

void SoundLibrary::changedLocked() {
    ++revision_;
    saveFailed_ = !saveLocked();
}

bool SoundLibrary::soundNameTakenLocked(const std::string& name, uint32_t exceptId) const {
    for (const auto& s : sounds_) if (s.id != exceptId && s.name == name) return true;
    return false;
}

bool SoundLibrary::bankNameTakenLocked(const std::string& name, uint32_t exceptId) const {
    for (const auto& b : banks_) if (b.id != exceptId && b.name == name) return true;
    return false;
}

bool SoundLibrary::soundNameTaken(const std::string& name, uint32_t exceptId) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    return soundNameTakenLocked(sanitizeName(name), exceptId);
}

std::string SoundLibrary::uniqueSoundName(const std::string& wanted, uint32_t exceptId) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    std::string name = sanitizeName(wanted);
    if (name.empty()) name = "MY SOUND";
    if (!soundNameTakenLocked(name, exceptId)) return name;
    std::string base;
    int n = 1;
    splitNumber(name, base, n);
    for (int i = std::max(2, n + 1); i < 10000; ++i) {
        const std::string c = withNumber(base, i);
        if (!soundNameTakenLocked(c, exceptId)) return c;
    }
    return name;
}

std::string SoundLibrary::uniqueBankName(const std::string& wanted, uint32_t exceptId) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    std::string name = sanitizeName(wanted);
    if (name.empty()) name = "MY BANK";
    if (!bankNameTakenLocked(name, exceptId)) return name;
    std::string base;
    int n = 1;
    splitNumber(name, base, n);
    for (int i = std::max(2, n + 1); i < 10000; ++i) {
        const std::string c = withNumber(base, i);
        if (!bankNameTakenLocked(c, exceptId)) return c;
    }
    return name;
}

uint32_t SoundLibrary::addSound(const std::string& name, const std::string& category, int era, int basePreset,
                                const float* values) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    UserSound s;
    s.id = newIdLocked();
    s.name = sanitizeName(name);
    if (s.name.empty()) s.name = "MY SOUND";
    s.category = sanitizeName(category).substr(0, 12);
    if (s.category.empty()) s.category = "USER";
    s.era = std::clamp(era, 0, kNumEras - 1);
    s.basePreset = std::clamp(basePreset, 0, kNumFactoryPresets - 1);
    for (int i = 0; i < kNumParams; ++i) s.values[i] = std::clamp(values[i], 0.0f, 1.0f);
    sounds_.push_back(s);
    changedLocked();
    return s.id;
}

bool SoundLibrary::updateSound(uint32_t id, const float* values) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    UserSound* s = findSoundLocked(id);
    if (!s) return false;
    for (int i = 0; i < kNumParams; ++i) s->values[i] = std::clamp(values[i], 0.0f, 1.0f);
    changedLocked();
    return true;
}

bool SoundLibrary::renameSound(uint32_t id, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    UserSound* s = findSoundLocked(id);
    const std::string clean = sanitizeName(name);
    if (!s || clean.empty()) return false;
    s->name = clean;
    changedLocked();
    return true;
}

bool SoundLibrary::deleteSound(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    const auto it = std::find_if(sounds_.begin(), sounds_.end(), [&](const UserSound& s) { return s.id == id; });
    if (it == sounds_.end()) return false;
    sounds_.erase(it);
    const SoundKey key = SoundKey::user(id);
    favorites_.erase(std::remove(favorites_.begin(), favorites_.end(), key), favorites_.end());
    for (auto& b : banks_) b.items.erase(std::remove(b.items.begin(), b.items.end(), key), b.items.end());
    changedLocked();
    return true;
}

bool SoundLibrary::isFavorite(const SoundKey& k) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    return std::find(favorites_.begin(), favorites_.end(), k) != favorites_.end();
}

bool SoundLibrary::setFavorite(const SoundKey& k, bool on) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    if (k.kind == kSoundDetached) return false;
    if (k.kind == kSoundUser && !findSoundLocked(k.id)) return false;
    const auto it = std::find(favorites_.begin(), favorites_.end(), k);
    if (on == (it != favorites_.end())) return on;
    if (on) favorites_.push_back(k);
    else favorites_.erase(it);
    changedLocked();
    return on;
}

uint32_t SoundLibrary::createBank(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    SoundBank b;
    b.id = newIdLocked();
    b.name = sanitizeName(name);
    if (b.name.empty()) b.name = "MY BANK";
    banks_.push_back(b);
    changedLocked();
    return b.id;
}

bool SoundLibrary::renameBank(uint32_t id, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    SoundBank* b = findBankLocked(id);
    const std::string clean = sanitizeName(name);
    if (!b || clean.empty()) return false;
    b->name = clean;
    changedLocked();
    return true;
}

bool SoundLibrary::deleteBank(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    const auto it = std::find_if(banks_.begin(), banks_.end(), [&](const SoundBank& b) { return b.id == id; });
    if (it == banks_.end()) return false;
    banks_.erase(it);
    changedLocked();
    return true;
}

bool SoundLibrary::bankContains(uint32_t bankId, const SoundKey& k) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    const SoundBank* b = findBankLocked(bankId);
    return b && std::find(b->items.begin(), b->items.end(), k) != b->items.end();
}

bool SoundLibrary::addToBank(uint32_t bankId, const SoundKey& k) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    SoundBank* b = findBankLocked(bankId);
    if (!b || k.kind == kSoundDetached) return false;
    if (k.kind == kSoundUser && !findSoundLocked(k.id)) return false;
    if (std::find(b->items.begin(), b->items.end(), k) != b->items.end()) return true;
    b->items.push_back(k);
    changedLocked();
    return true;
}

bool SoundLibrary::removeFromBank(uint32_t bankId, const SoundKey& k) {
    std::lock_guard<std::mutex> lock(m_);
    ensureLoadedLocked();
    SoundBank* b = findBankLocked(bankId);
    if (!b) return false;
    const auto it = std::find(b->items.begin(), b->items.end(), k);
    if (it == b->items.end()) return false;
    b->items.erase(it);
    changedLocked();
    return true;
}

// =============================================================================
// File format (plain text, one record per line, tab separated):
//
//   PIXELLABS LIBRARY 1
//   SOUND <id> <era> <base preset> <category> <name> <param>=<value*1e6> ...
//   FAV   <key>
//   BANK  <id> <name> <key>,<key>,...
//
// Values are stored as integers so the file never depends on the decimal separator.
// =============================================================================

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t t = line.find('\t', start);
        out.push_back(line.substr(start, t == std::string::npos ? std::string::npos : t - start));
        if (t == std::string::npos) break;
        start = t + 1;
    }
    return out;
}

void SoundLibrary::loadLocked() {
    sounds_.clear();
    favorites_.clear();
    banks_.clear();
    fileStamp_ = fileStampOf(path_);

    FILE* f = _wfopen(path_.c_str(), L"rb");
    if (!f) return;
    std::string text;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);

    size_t pos = 0;
    std::vector<std::pair<SoundBank, std::string>> pendingBanks; // resolved once all sounds are known
    std::vector<std::string> pendingFavs;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> f2 = splitTabs(line);

        if (f2[0] == "SOUND" && f2.size() >= 7) {
            UserSound s;
            s.id = static_cast<uint32_t>(std::strtoul(f2[1].c_str(), nullptr, 10));
            s.era = std::clamp(std::atoi(f2[2].c_str()), 0, kNumEras - 1);
            s.basePreset = std::clamp(std::atoi(f2[3].c_str()), 0, kNumFactoryPresets - 1);
            s.category = sanitizeName(f2[4]).substr(0, 12);
            s.name = sanitizeName(f2[5]);
            if (s.id == 0 || s.id > 0x3FFFFFFFu || s.name.empty() || findSoundLocked(s.id)) continue;
            // Start from the base preset so parameters added in later versions get sensible values
            const FactoryPreset& base = factoryPreset(s.basePreset);
            for (int i = 0; i < kNumParams; ++i) s.values[i] = base.values[i];
            const std::string& vals = f2[6];
            size_t p = 0;
            while (p < vals.size()) {
                size_t sp = vals.find(' ', p);
                if (sp == std::string::npos) sp = vals.size();
                const std::string tok = vals.substr(p, sp - p);
                p = sp + 1;
                const size_t eq = tok.find('=');
                if (eq == std::string::npos) continue;
                const int id = std::atoi(tok.substr(0, eq).c_str());
                const long v = std::strtol(tok.c_str() + eq + 1, nullptr, 10);
                if (id >= 0 && id < kNumParams) s.values[id] = std::clamp(static_cast<float>(v) / 1000000.0f, 0.0f, 1.0f);
            }
            sounds_.push_back(s);
        } else if (f2[0] == "FAV" && f2.size() >= 2) {
            pendingFavs.push_back(f2[1]);
        } else if (f2[0] == "BANK" && f2.size() >= 3) {
            SoundBank b;
            b.id = static_cast<uint32_t>(std::strtoul(f2[1].c_str(), nullptr, 10));
            b.name = sanitizeName(f2[2]);
            if (b.id == 0 || b.id > 0x3FFFFFFFu || b.name.empty()) continue;
            pendingBanks.push_back({ b, f2.size() >= 4 ? f2[3] : std::string() });
        }
    }

    auto validKey = [&](const SoundKey& k) {
        return k.kind == kSoundFactory || (k.kind == kSoundUser && findSoundLocked(k.id));
    };
    for (const auto& s : pendingFavs) {
        SoundKey k;
        if (SoundKey::fromString(s, k) && validKey(k) &&
            std::find(favorites_.begin(), favorites_.end(), k) == favorites_.end()) favorites_.push_back(k);
    }
    for (auto& pb : pendingBanks) {
        if (findBankLocked(pb.first.id) || findSoundLocked(pb.first.id)) continue;
        const std::string& list = pb.second;
        size_t p = 0;
        while (p < list.size()) {
            size_t c = list.find(',', p);
            if (c == std::string::npos) c = list.size();
            SoundKey k;
            if (SoundKey::fromString(list.substr(p, c - p), k) && validKey(k) &&
                std::find(pb.first.items.begin(), pb.first.items.end(), k) == pb.first.items.end()) {
                pb.first.items.push_back(k);
            }
            p = c + 1;
        }
        banks_.push_back(pb.first);
    }
}

bool SoundLibrary::saveLocked() {
    CreateDirectoryW(dir_.c_str(), nullptr); // fine if it already exists

    std::string out = "PIXELLABS LIBRARY 1\n# Pixel LABS saved sounds, favorites and banks.\n";
    char num[48];
    for (const auto& s : sounds_) {
        std::snprintf(num, sizeof(num), "%lu\t%d\t%d\t", static_cast<unsigned long>(s.id), s.era, s.basePreset);
        out += "SOUND\t";
        out += num;
        out += s.category + "\t" + s.name + "\t";
        for (int i = 0; i < kNumParams; ++i) {
            std::snprintf(num, sizeof(num), "%s%d=%ld", i ? " " : "", i,
                          static_cast<long>(std::lround(s.values[i] * 1000000.0)));
            out += num;
        }
        out += "\n";
    }
    for (const auto& k : favorites_) out += "FAV\t" + k.toString() + "\n";
    for (const auto& b : banks_) {
        std::snprintf(num, sizeof(num), "%lu\t", static_cast<unsigned long>(b.id));
        out += "BANK\t";
        out += num;
        out += b.name + "\t";
        for (size_t i = 0; i < b.items.size(); ++i) {
            if (i) out += ",";
            out += b.items[i].toString();
        }
        out += "\n";
    }

    // Write a temp file, then swap it in, so a crash never leaves a half-written library
    const std::wstring tmp = path_ + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return false;
    const bool wrote = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    const bool closed = std::fclose(f) == 0;
    if (!wrote || !closed) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    fileStamp_ = fileStampOf(path_);
    return true;
}

} // namespace PixelLabs
