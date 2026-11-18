#pragma once

#include "../library/SoundLibrary.h"
#include <string>

namespace PixelLabs {

// The sound the plugin has loaded, plus its stored values: comparing those with the live
// parameters tells the editor whether the sound has unsaved edits.
struct CurrentSoundInfo {
    SoundKey key;
    std::string name;
    std::string category;
    int era = 0;
    int basePreset = 0;
    float ref[kNumParams] = {};
};

// What the editor asks of the plugin. Everything runs on the UI thread.
class IEditorBridge {
public:
    virtual ~IEditorBridge() = default;
    virtual void editorSetParam(int id, float value) = 0;
    virtual void editorLoadFactory(int slot) = 0;
    virtual bool editorLoadUser(uint32_t id) = 0;
    virtual CurrentSoundInfo editorCurrentSound() = 0;
    virtual void editorSetCurrentSound(const CurrentSoundInfo& info) = 0; // after save / rename / delete
};

} // namespace PixelLabs
