#pragma once

#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "Main.hpp"

class Hooks {
   private:
    static inline std::vector<void (*)()> installFuncs;

   public:
    static inline void AddInstallFunc(void (*installFunc)()) { installFuncs.push_back(installFunc); }

    static inline void Install() {
        LOG_INFO("Installing hooks...");
        for (auto& func : installFuncs)
            func();
        LOG_INFO("Installed all hooks!");
    }
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

#define AUTO_INSTALL_ORIG(name_) \
    AUTO_HOOK_FUNCTION(Hook_##name_) { ::Hooking::InstallOrigHook<Hook_##name_>(logger); }

#define AUTO_INSTALL(name_) \
    AUTO_HOOK_FUNCTION(Hook_##name_) { ::Hooking::InstallHook<Hook_##name_>(logger); }

#define MAKE_AUTO_HOOK_MATCH(name_, mPtr, retval, ...)                                                                                              \
    struct Hook_##name_ {                                                                                                                           \
        using funcType = retval (*)(__VA_ARGS__);                                                                                                   \
        static_assert(MATCH_HOOKABLE_ASSERT(mPtr));                                                                                                 \
        static_assert(std::is_same_v<funcType, ::Hooking::InternalMethodCheck<decltype(mPtr)>::funcType>, "Hook method signature does not match!"); \
        constexpr static const char* name() { return #name_; }                                                                                      \
        static const MethodInfo* getInfo() { return ::il2cpp_utils::il2cpp_type_check::MetadataGetter<mPtr>::methodInfo(); }                        \
        static funcType* trampoline() { return &name_; }                                                                                            \
        static inline retval (*name_)(__VA_ARGS__) = nullptr;                                                                                       \
        static funcType hook() { return &::Hooking::HookCatchWrapper<&hook_##name_, funcType>::wrapper; }                                           \
        static retval hook_##name_(__VA_ARGS__);                                                                                                    \
    };                                                                                                                                              \
    AUTO_INSTALL(name_)                                                                                                                             \
    retval Hook_##name_::hook_##name_(__VA_ARGS__)

#define MAKE_AUTO_ORIG_HOOK_MATCH(name_, mPtr, retval, ...)                                                                                         \
    struct Hook_##name_ {                                                                                                                           \
        using funcType = retval (*)(__VA_ARGS__);                                                                                                   \
        static_assert(MATCH_HOOKABLE_ASSERT(mPtr));                                                                                                 \
        static_assert(std::is_same_v<funcType, ::Hooking::InternalMethodCheck<decltype(mPtr)>::funcType>, "Hook method signature does not match!"); \
        constexpr static const char* name() { return #name_; }                                                                                      \
        static const MethodInfo* getInfo() { return ::il2cpp_utils::il2cpp_type_check::MetadataGetter<mPtr>::methodInfo(); }                        \
        static funcType* trampoline() { return &name_; }                                                                                            \
        static inline retval (*name_)(__VA_ARGS__) = nullptr;                                                                                       \
        static funcType hook() { return &::Hooking::HookCatchWrapper<&hook_##name_, funcType>::wrapper; }                                           \
        static retval hook_##name_(__VA_ARGS__);                                                                                                    \
    };                                                                                                                                              \
    AUTO_INSTALL_ORIG(name_)                                                                                                                        \
    retval Hook_##name_::hook_##name_(__VA_ARGS__)

void Camera_Pause();
void Camera_Unpause();

namespace UnityEngine {
    class Camera;
}
extern UnityEngine::Camera* mainCamera;
