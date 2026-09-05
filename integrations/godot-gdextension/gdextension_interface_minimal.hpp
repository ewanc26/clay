#ifndef CLAY_GODOT_GDEXTENSION_INTERFACE_MINIMAL_HPP
#define CLAY_GODOT_GDEXTENSION_INTERFACE_MINIMAL_HPP

#include <cstdint>

using GDExtensionBool = std::uint8_t;
using GDExtensionClassLibraryPtr = void *;
using GDExtensionInterfaceFunctionPtr = void (*)();
using GDExtensionInterfaceGetProcAddress =
    GDExtensionInterfaceFunctionPtr (*)(const char *);

enum GDExtensionInitializationLevel : std::uint32_t {
    GDEXTENSION_INITIALIZATION_CORE = 0,
    GDEXTENSION_INITIALIZATION_SERVERS = 1,
    GDEXTENSION_INITIALIZATION_SCENE = 2,
    GDEXTENSION_INITIALIZATION_EDITOR = 3,
    GDEXTENSION_MAX_INITIALIZATION_LEVEL = 4,
};

using GDExtensionInitializeCallback =
    void (*)(void *, GDExtensionInitializationLevel);
using GDExtensionDeinitializeCallback =
    void (*)(void *, GDExtensionInitializationLevel);

struct GDExtensionInitialization {
    GDExtensionInitializationLevel minimum_initialization_level;
    void *userdata;
    GDExtensionInitializeCallback initialize;
    GDExtensionDeinitializeCallback deinitialize;
};

#if defined(_WIN32)
#define CLAY_GDE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define CLAY_GDE_EXPORT __attribute__((visibility("default")))
#else
#define CLAY_GDE_EXPORT
#endif

#endif
