#pragma once

#include "../plugin/PixelLabsIds.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace PixelLabs {

// Where a sound comes from. A detached sound has a name and values but no library entry
// (it was deleted, or the project came from a machine with a different library).
enum SoundKind : int32_t { kSoundFactory = 0, kSoundUser = 1, kSoundDetached = 2 };

struct SoundKey {
    int32_t kind = kSoundFactory;
    uint32_t id = 0;             // factory slot, or user sound id

    bool operator==(const SoundKey& o) const { return kind == o.kind && id == o.id; }
    bool operator!=(const SoundKey& o) const { return !(*this == o); }

    static SoundKey factory(int slot) { SoundKey k; k.kind = kSoundFactory; k.id = static_cast<uint32_t>(slot); return k; }
    static SoundKey user(uint32_t id) { SoundKey k; k.kind = kSoundUser; k.id = id; return k; }

    std::string toString() const;                       // "F12" / "U305419896"
    static bool fromString(const std::string& s, SoundKey& out);
};

struct UserSound {
    uint32_t id = 0;
    std::string name;
    std::string category;
    int era = 0;
    int basePreset = 0;            // factory slot it was made from
    float values[kNumParams] = {};
};

struct SoundBank {
    uint32_t id = 0;
    std::string name;
    std::vector<SoundKey> items;
};

// A copy of the library for drawing (the editor refreshes it when the revision changes).
struct LibrarySnapshot {
    uint64_t revision = 0;
    std::vector<UserSound> sounds;
    std::vector<SoundKey> favorites;
    std::vector<SoundBank> banks;

    const UserSound* findSound(uint32_t id) const;
    const SoundBank* findBank(uint32_t id) const;
    bool isFavorite(const SoundKey& k) const;
};

// One library per user, shared by every plugin instance and saved to
// %APPDATA%\PixelLABS\PixelLABS_Library.txt after every change
// (PIXELLABS_LIBRARY_DIR overrides the folder, e.g. for tests).
class SoundLibrary {
public:
    static constexpr size_t kMaxNameLen = 24;

    static SoundLibrary& instance();

    uint64_t revision();               // also picks up changes another process saved
    LibrarySnapshot snapshot();
    bool getSound(uint32_t id, UserSound& out);
    bool hasSound(uint32_t id);

    uint32_t addSound(const std::string& name, const std::string& category, int era, int basePreset,
                      const float* values);
    bool updateSound(uint32_t id, const float* values);
    bool renameSound(uint32_t id, const std::string& name);
    bool deleteSound(uint32_t id);     // also removes it from favorites and banks

    bool isFavorite(const SoundKey& k);
    bool setFavorite(const SoundKey& k, bool on);

    uint32_t createBank(const std::string& name);
    bool renameBank(uint32_t id, const std::string& name);
    bool deleteBank(uint32_t id);
    bool bankContains(uint32_t bankId, const SoundKey& k);
    bool addToBank(uint32_t bankId, const SoundKey& k);
    bool removeFromBank(uint32_t bankId, const SoundKey& k);

    // "NAME" -> "NAME 2" when the name is taken (ignoring `exceptId`)
    std::string uniqueSoundName(const std::string& wanted, uint32_t exceptId = 0);
    std::string uniqueBankName(const std::string& wanted, uint32_t exceptId = 0);
    bool soundNameTaken(const std::string& name, uint32_t exceptId = 0);

    // Upper-cases, keeps only characters the pixel font can draw, trims, limits the length.
    static std::string sanitizeName(const std::string& raw);
    static bool isNameChar(char c);

    std::wstring filePath();
    bool lastSaveFailed();

private:
    SoundLibrary();
    void ensureLoadedLocked();
    void loadLocked();
    bool saveLocked();
    void changedLocked();               // bump revision + save
    void pollFileLocked();
    uint32_t newIdLocked();
    bool soundNameTakenLocked(const std::string& name, uint32_t exceptId) const;
    bool bankNameTakenLocked(const std::string& name, uint32_t exceptId) const;
    UserSound* findSoundLocked(uint32_t id);
    SoundBank* findBankLocked(uint32_t id);

    std::mutex m_;
    bool loaded_ = false;
    bool saveFailed_ = false;
    uint64_t revision_ = 1;
    uint64_t fileStamp_ = 0;            // last write time of the file as we last saw it
    uint64_t lastPollMs_ = 0;
    uint32_t rng_ = 0;
    std::wstring dir_, path_;
    std::vector<UserSound> sounds_;
    std::vector<SoundKey> favorites_;
    std::vector<SoundBank> banks_;
};

} // namespace PixelLabs
