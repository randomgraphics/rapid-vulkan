#include "rv.h"
#define RAPID_VULKAN_IMPLEMENTATION
#include <rapid-vulkan/rapid-vulkan.h>
#include "../3rd-party/backward-cpp/backward.hpp"

// ---------------------------------------------------------------------------------------------------------------------
//
std::string backtrace() {
    int indent = 0;
#if PH_ANDROID
    struct android_backtrace_state {
        void ** current;
        void ** end;

        static _Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context * context, void * arg) {
            android_backtrace_state * state = (android_backtrace_state *) arg;
            uintptr_t                 pc    = _Unwind_GetIP(context);
            if (pc) {
                if (state->current == state->end)
                    return _URC_END_OF_STACK;
                else
                    *state->current++ = reinterpret_cast<void *>(pc);
            }
            return _URC_NO_REASON;
        }

        static std::string addr2symbol(const void * addr) {
            // convert address to symbol
            Dl_info info;
            if (!dladdr(addr, &info) || !info.dli_sname) return {};
            std::string result = info.dli_sname;

            // demangle c++ syntax
            int    status = 0;
            char * symbol = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
            if (!symbol) return result;
            if (0 == status) result = symbol;
            free(symbol);

            // done
            return result;
        }
    };

    std::string prefix;
    for (int i = 0; i < indent; ++i) prefix += ' ';

    std::stringstream ss;
    ss << prefix << "android stack dump\n";

    const int max = 100;
    void *    buffer[max];

    android_backtrace_state state;
    state.current = buffer;
    state.end     = buffer + max;

    _Unwind_Backtrace(android_backtrace_state::android_unwind_callback, &state);

    int count = (int) (state.current - buffer);

    for (int idx = 0; idx < count; idx++) {
        auto addr   = buffer[idx];
        auto symbol = android_backtrace_state::addr2symbol(addr);
        if (symbol.empty()) symbol = "<no symbol>";
        ss << prefix << formatstr("%03d: 0x%p %s\n", idx, addr, symbol.c_str());
    }

    ss << prefix << "android stack dump done\n";

    return ss.str();
#elif defined(__GNUC__)
    (void) indent;
    using namespace backward;
    StackTrace st;
    st.load_here(32);
    std::stringstream ss;
    Printer           p;
    p.snippet = true; // print code snippet in debug build only.
    p.print(st, ss);
    return ss.str();
#elif defined(_MSC_VER)
    (void) indent;
    class MyStackWalker : public StackWalker {
    protected:
        void OnLoadModule(LPCSTR, LPCSTR, DWORD64, DWORD, DWORD, LPCSTR, LPCSTR, ULONGLONG) override {}
        void OnDbgHelpErr(LPCSTR, DWORD, DWORD64) override {}
        void OnSymInit(LPCSTR, DWORD, LPCSTR) override {}
        void OnOutput(LPCSTR szText) override { ss << szText; }

    public:
        using StackWalker::StackWalker;
        std::stringstream ss;
    };
    MyStackWalker sw(StackWalker::RetrieveLine | StackWalker::RetrieveSymbol);
    return sw.ShowCallstack() ? sw.ss.str() : std::string {};
#else
    (void) indent;
    return {};
#endif
}
