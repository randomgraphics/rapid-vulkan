#pragma once

#include "3rd-party/rdc/renderdoc_app.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/// A simple utility class to do in-application programmatic RenderDoc capture.
class RenderDocCapture {
    RENDERDOC_API_1_4_1 * _api = nullptr;

public:
    RenderDocCapture() {
        pRENDERDOC_GetAPI getApi = nullptr;
#ifdef _WIN32
        // At init, on windows
        HMODULE mod = GetModuleHandleA("renderdoc.dll");
        if (!mod) {
            logi("renderdoc.dll is not loaded. RDC capture functionalities are disabled.");
            return;
        }
        getApi = (pRENDERDOC_GetAPI) GetProcAddress(mod, "RENDERDOC_GetAPI");
#else
        // At init, on linux/android.
        // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
        const char * path = PH_ANDROID ? "libVkLayer_GLES_RenderDoc.so" : "librenderdoc.so";
        void *       mod  = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        if (!mod) {
            logi("%s is not loaded. RDC capture functionalities are disabled.", path);
            return;
        }
        getApi = (pRENDERDOC_GetAPI) dlsym(mod, "RENDERDOC_GetAPI");
#endif
        if (!getApi) {
            loge("Failed to retrieve RENDERDOC_GetAPI pointer from rdc module");
            return;
        }
        int ret = getApi(eRENDERDOC_API_Version_1_5_0, (void **) &_api);
        if (1 != ret) {
            loge("RENDERDOC_GetAPI() failed.");
            _api = nullptr;
            return;
        }
        logi("RDC API is loaded.");
    }

    bool good() const { return !!_api; }

    operator bool() const { return good(); }

    /// This method is to start a frame capture.
    /// You can specify NULL, NULL for the device to capture on if you have only one device and
    /// either no windows at all or only one window, and it will capture from that device.
    /// See the RenderDoc documentation of StartFrameCapture() for more details.
    void begin(const char * capturePathTemplate, RENDERDOC_DevicePointer device = nullptr, RENDERDOC_WindowHandle window = nullptr) {
        if (!_api) {
            logi("Can't start RDC capture: the API is not loaded.");
            return;
        }
        _api->SetCaptureFilePathTemplate(capturePathTemplate);
        _api->StartFrameCapture(device, window);
        logi("RDC capture started....");
    }

    void end() {
        if (!_api) {
            logi("Can't stop RDC capture: the API is not loaded.");
            return;
        }
        // stop the capture
        _api->EndFrameCapture(NULL, NULL);
        logi("RDC capture ended.");
    }

    void logi(...) {}

    void loge(...) {}
};
