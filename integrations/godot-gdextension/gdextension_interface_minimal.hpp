#ifndef CLAY_GODOT_GDEXTENSION_INTERFACE_MINIMAL_HPP
#define CLAY_GODOT_GDEXTENSION_INTERFACE_MINIMAL_HPP

#include <cstdint>
#include <cstddef>

using GDExtensionBool = std::uint8_t;
using GDExtensionClassLibraryPtr = void *;
using GDExtensionObjectPtr = void *;
using GDExtensionClassInstancePtr = void *;
using GDExtensionTypePtr = void *;
using GDExtensionInterfaceFunctionPtr = void (*)();
using GDExtensionInterfaceGetProcAddress =
    GDExtensionInterfaceFunctionPtr (*)(const char *);

struct GDExtensionStringName {
    std::uint8_t data[8]{};
};
using GDExtensionConstStringNamePtr = const GDExtensionStringName *;
using GDExtensionUninitializedStringNamePtr = GDExtensionStringName *;

enum GDExtensionVariantType : std::uint32_t {
    GDEXTENSION_VARIANT_TYPE_STRING_NAME = 21,
};

using GDExtensionPtrDestructor = void (*)(GDExtensionTypePtr);
using GDExtensionVariantGetPtrDestructor =
    GDExtensionPtrDestructor (*)(GDExtensionVariantType);
using GDExtensionStringNameNewWithLatin1Chars =
    void (*)(GDExtensionUninitializedStringNamePtr, const char *, GDExtensionBool);
using GDExtensionClassCreateInstance = GDExtensionObjectPtr (*)(void *);
using GDExtensionClassFreeInstance =
    void (*)(void *, GDExtensionClassInstancePtr);

struct GDExtensionClassCreationInfo2 {
    GDExtensionBool is_virtual;
    GDExtensionBool is_abstract;
    GDExtensionBool is_exposed;
    void *set_func;
    void *get_func;
    void *get_property_list_func;
    void *free_property_list_func;
    void *property_can_revert_func;
    void *property_get_revert_func;
    void *validate_property_func;
    void *notification_func;
    void *to_string_func;
    void *reference_func;
    void *unreference_func;
    GDExtensionClassCreateInstance create_instance_func;
    GDExtensionClassFreeInstance free_instance_func;
    void *recreate_instance_func;
    void *get_virtual_func;
    void *get_virtual_call_data_func;
    void *call_virtual_with_data_func;
    void *get_rid_func;
    void *class_userdata;
};

struct GDExtensionClassCreationInfo3 {
    GDExtensionBool is_virtual;
    GDExtensionBool is_abstract;
    GDExtensionBool is_exposed;
    GDExtensionBool is_runtime;
    void *set_func;
    void *get_func;
    void *get_property_list_func;
    void *free_property_list_func;
    void *property_can_revert_func;
    void *property_get_revert_func;
    void *validate_property_func;
    void *notification_func;
    void *to_string_func;
    void *reference_func;
    void *unreference_func;
    GDExtensionClassCreateInstance create_instance_func;
    GDExtensionClassFreeInstance free_instance_func;
    void *recreate_instance_func;
    void *get_virtual_func;
    void *get_virtual_call_data_func;
    void *call_virtual_with_data_func;
    void *get_rid_func;
    void *class_userdata;
};

using GDExtensionInterfaceClassdbRegisterExtensionClass2 = void (*)(
    GDExtensionClassLibraryPtr, GDExtensionConstStringNamePtr,
    GDExtensionConstStringNamePtr, const GDExtensionClassCreationInfo2 *);
using GDExtensionInterfaceClassdbRegisterExtensionClass3 = void (*)(
    GDExtensionClassLibraryPtr, GDExtensionConstStringNamePtr,
    GDExtensionConstStringNamePtr, const GDExtensionClassCreationInfo3 *);
using GDExtensionInterfaceClassdbConstructObject =
    GDExtensionObjectPtr (*)(GDExtensionConstStringNamePtr);
using GDExtensionInterfaceObjectSetInstance = void (*)(
    GDExtensionObjectPtr, GDExtensionConstStringNamePtr,
    GDExtensionClassInstancePtr);
using GDExtensionInterfaceMemAlloc = void *(*)(std::size_t);
using GDExtensionInterfaceMemFree = void (*)(void *);

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
