#pragma once

#include "Main.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

class Hooks {
   private:
    inline static std::vector<void (*)()> installFuncs;

   public:
    static void AddInstallFunc(void (*installFunc)()) { installFuncs.push_back(installFunc); }

    static inline void Install() {
        for (auto& func : installFuncs)
            func();
    }
};

#define AUTO_INSTALL_ORIG(name_)                                                                    \
    struct Auto_Hook_##name_ {                                                                      \
        static void Auto_Hook_##name_##_Install() {                                                 \
            ::Hooking::InstallOrigHook<Hook_##name_>(logger);                                       \
        }                                                                                           \
        Auto_Hook_##name_() { ::Hooks::AddInstallFunc(Auto_Hook_##name_##_Install); }               \
    };                                                                                              \
    static Auto_Hook_##name_ Auto_Hook_Instance_##name_;

#define AUTO_INSTALL(name_)                                                                         \
    struct Auto_Hook_##name_ {                                                                      \
        static void Auto_Hook_##name_##_Install() {                                                 \
            ::Hooking::InstallHook<Hook_##name_>(logger);                                           \
        }                                                                                           \
        Auto_Hook_##name_() { ::Hooks::AddInstallFunc(Auto_Hook_##name_##_Install); }               \
    };                                                                                              \
    static Auto_Hook_##name_ Auto_Hook_Instance_##name_;

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

void InstallBlitHook();

namespace UnityEngine {
    class Camera;
}
extern UnityEngine::Camera* mainCamera;
