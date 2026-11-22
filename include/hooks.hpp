#pragma once

#include <vector>

#include "beatsaber-hook/shared/hooking.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "main.hpp"

class Hooks {
   private:
    static inline std::vector<void (*)()> installFuncs;

   public:
    static inline void AddInstallFunc(void (*installFunc)()) { installFuncs.push_back(installFunc); }

    static inline void Install() {
        logger.info("Installing hooks...");
        for (auto& func : installFuncs)
            func();
        logger.info("Installed all hooks!");
    }
};

template <auto C, class T>
struct FunctionToMethod {
    using type = T;
};
template <auto C, class R, class T, class... Ts>
requires(C((R(T::*)(Ts...)) nullptr))
struct FunctionToMethod<C, R (*)(T*, Ts...)> {
    using type = R (T::*)(Ts...);
};
template <auto C, class R, class T, class... Ts>
requires(C((R(T::*)(Ts...)) nullptr))
struct FunctionToMethod<C, R (*)(T, Ts...)> {
    using type = R (T::*)(Ts...);
};

#define AUTO_HOOK_FUNCTION(name_)                                        \
    namespace {                                                          \
        struct Auto_Install_##name_ {                                    \
            static void Install();                                       \
            Auto_Install_##name_() { ::Hooks::AddInstallFunc(Install); } \
        };                                                               \
    }                                                                    \
    static Auto_Install_##name_ Auto_Install_Instance_##name_;           \
    void Auto_Install_##name_::Install()

// beatsaber-hook 9: hooks are declared and registered through the autohooks package.
#include "autohooks/shared/hooks.hpp"

#define MAKE_AUTO_HOOK_MATCH(name_, method, retval, ...) MAKE_LATE_HOOK_MATCH(name_, method, retval, __VA_ARGS__)
#define MAKE_AUTO_HOOK_FIND(name_, info, retval, ...) MAKE_LATE_HOOK(name_, (info), retval, __VA_ARGS__)
#define MAKE_AUTO_ORIG_HOOK_MATCH(name_, method, retval, ...) MAKE_LATE_ORIG_HOOK_MATCH(name_, method, retval, __VA_ARGS__)
#define MAKE_AUTO_HOOK_ORIG_MATCH(name_, method, retval, ...) MAKE_LATE_ORIG_HOOK_MATCH(name_, method, retval, __VA_ARGS__)

// Installs the autohooks hooks together with this mod's other registered install functions.
inline bool const PortAutoHooksRegistered = (::Hooks::AddInstallFunc(+[] { INSTALL_LATE_HOOKS(); }), true);
