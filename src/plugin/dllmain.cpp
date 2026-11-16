#include "vst3/vst3_abi.h"
#include "PixelLabsPlugin.h"
#include "Version.h"
#include <windows.h>
#include <atomic>
#include <cstring>

namespace PixelLabs {

using namespace Steinberg;

class PixelLabsFactory : public IPluginFactory3 {
public:
    PixelLabsFactory() = default;
    virtual ~PixelLabsFactory() = default;

    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override {
        if (iidEqual(_iid, FUnknown::iid) ||
            iidEqual(_iid, IPluginFactory::iid) ||
            iidEqual(_iid, IPluginFactory2::iid) ||
            iidEqual(_iid, IPluginFactory3::iid)) {
            addRef();
            *obj = static_cast<IPluginFactory3*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef() override {
        return ++refCount_;
    }

    uint32 PLUGIN_API release() override {
        uint32 r = --refCount_;
        return r;
    }

    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PFactoryInfo));
        std::strncpy(info->vendor, PIXELLABS_VENDOR, sizeof(info->vendor) - 1);
        std::strncpy(info->url, PIXELLABS_URL, sizeof(info->url) - 1);
        std::strncpy(info->email, PIXELLABS_EMAIL, sizeof(info->email) - 1);
        info->flags = PFactoryInfo::kUnicode;
        return kResultOk;
    }

    int32 PLUGIN_API countClasses() override {
        return 1;
    }

    tresult PLUGIN_API getClassInfo(int32 index, PClassInfo* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PClassInfo));
        copyTUID(info->cid, kPixelLabsProcessorUID);
        info->cardinality = PClassInfo::kManyInstances;
        std::strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
        std::strncpy(info->name, "Pixel LABS", sizeof(info->name) - 1);
        return kResultOk;
    }

    tresult PLUGIN_API getClassInfo2(int32 index, PClassInfo2* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PClassInfo2));
        copyTUID(info->cid, kPixelLabsProcessorUID);
        info->cardinality = PClassInfo::kManyInstances;
        std::strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
        std::strncpy(info->name, "Pixel LABS", sizeof(info->name) - 1);
        info->classFlags = Vst::kSimpleModeSupported;
        std::strncpy(info->subCategories, "Instrument|Synth", sizeof(info->subCategories) - 1);
        std::strncpy(info->vendor, PIXELLABS_VENDOR, sizeof(info->vendor) - 1);
        std::strncpy(info->version, PIXELLABS_VERSION_STR, sizeof(info->version) - 1);
        std::strncpy(info->sdkVersion, "VST 3.7.12", sizeof(info->sdkVersion) - 1);
        return kResultOk;
    }

    tresult PLUGIN_API getClassInfoUnicode(int32 index, PClassInfoW* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PClassInfoW));
        copyTUID(info->cid, kPixelLabsProcessorUID);
        info->cardinality = PClassInfo::kManyInstances;
        std::strncpy(info->category, "Audio Module Class", sizeof(info->category) - 1);
        Vst::strToU16(info->name, "Pixel LABS", 64);
        info->classFlags = Vst::kSimpleModeSupported;
        std::strncpy(info->subCategories, "Instrument|Synth", sizeof(info->subCategories) - 1);
        Vst::strToU16(info->vendor, PIXELLABS_VENDOR, 64);
        Vst::strToU16(info->version, PIXELLABS_VERSION_STR, 64);
        Vst::strToU16(info->sdkVersion, "VST 3.7.12", 64);
        return kResultOk;
    }

    tresult PLUGIN_API setHostContext(FUnknown* /*context*/) override {
        return kResultOk;
    }

    tresult PLUGIN_API createInstance(FIDString cid, FIDString _iid, void** obj) override {
        if (!cid || !_iid || !obj) return kInvalidArgument;
        *obj = nullptr;
        const auto* reqCid = reinterpret_cast<const uint8*>(cid);
        const auto* reqIid = reinterpret_cast<const uint8*>(_iid);

        if (iidEqual(reqCid, kPixelLabsProcessorUID)) {
            auto* plugin = new PixelLabsPlugin();
            tresult res = plugin->queryInterface(reqIid, obj);
            plugin->release();
            return res;
        }
        return kNoInterface;
    }

private:
    std::atomic<uint32_t> refCount_{1};
};

} // namespace PixelLabs

extern "C" {

__declspec(dllexport) Steinberg::IPluginFactory* PLUGIN_API GetPluginFactory() {
    static PixelLabs::PixelLabsFactory sFactory;
    sFactory.addRef();
    return &sFactory;
}

__declspec(dllexport) bool PLUGIN_API InitDll() {
    return true;
}

__declspec(dllexport) bool PLUGIN_API ExitDll() {
    return true;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD /*fdwReason*/, LPVOID /*lpvReserved*/) {
    return TRUE;
}
