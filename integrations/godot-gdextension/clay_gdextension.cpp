#include "gdextension_interface_minimal.hpp"

#include <clay/engine_c.h>

namespace {

struct Api {
    GDExtensionInterfaceClassdbRegisterExtensionClass3 register_class{};
    GDExtensionInterfaceClassdbConstructObject construct_object{};
    GDExtensionInterfaceObjectSetInstance set_instance{};
    GDExtensionInterfaceMemAlloc mem_alloc{};
    GDExtensionInterfaceMemFree mem_free{};
    GDExtensionStringNameNewWithLatin1Chars string_name_new{};
    GDExtensionPtrDestructor string_name_destructor{};
};

Api api;
GDExtensionClassLibraryPtr class_library = nullptr;

constexpr std::int32_t kNotificationProcess = 17;

void noop_string_name_destructor(GDExtensionTypePtr) {}

struct ClayRuntimeNode {
    cl_engine_runtime *runtime{};
};

GDExtensionObjectPtr create_instance(void *) {
    GDExtensionStringName parent_name{};
    api.string_name_new(&parent_name, "Node", 0);
    const GDExtensionObjectPtr object = api.construct_object(&parent_name);
    api.string_name_destructor(&parent_name);
    if (object == nullptr) {
        return nullptr;
    }
    auto *instance = static_cast<ClayRuntimeNode *>(
        api.mem_alloc(sizeof(ClayRuntimeNode)));
    if (instance == nullptr) {
        return nullptr;
    }
    instance->runtime = cl_engine_runtime_create(320, 180, 0);
    if (instance->runtime == nullptr) {
        api.mem_free(instance);
        return nullptr;
    }
    GDExtensionStringName class_name{};
    api.string_name_new(&class_name, "ClayRuntimeNode", 0);
    api.set_instance(object, &class_name, instance);
    api.string_name_destructor(&class_name);
    return object;
}

void free_instance(void *, GDExtensionClassInstancePtr instance) {
    if (instance != nullptr) {
        auto *node = static_cast<ClayRuntimeNode *>(instance);
        cl_engine_runtime_destroy(node->runtime);
        api.mem_free(instance);
    }
}

void notification(GDExtensionClassInstancePtr instance, std::int32_t what,
                  GDExtensionBool) {
    if (what != kNotificationProcess || instance == nullptr) return;
    auto *node = static_cast<ClayRuntimeNode *>(instance);
    if (node->runtime != nullptr) {
        (void)cl_engine_runtime_step(node->runtime, 1.0 / 60.0);
    }
}

void initialize(void *, GDExtensionInitializationLevel level) {
    if (level != GDEXTENSION_INITIALIZATION_SCENE ||
        api.register_class == nullptr) {
        return;
    }
    GDExtensionStringName class_name{};
    GDExtensionStringName parent_name{};
    api.string_name_new(&class_name, "ClayRuntimeNode", 0);
    api.string_name_new(&parent_name, "Node", 0);
    const GDExtensionClassCreationInfo3 info{
        .is_virtual = 0,
        .is_abstract = 0,
        .is_exposed = 1,
        .is_runtime = 1,
        .set_func = nullptr,
        .get_func = nullptr,
        .get_property_list_func = nullptr,
        .free_property_list_func = nullptr,
        .property_can_revert_func = nullptr,
        .property_get_revert_func = nullptr,
        .validate_property_func = nullptr,
        .notification_func = notification,
        .to_string_func = nullptr,
        .reference_func = nullptr,
        .unreference_func = nullptr,
        .create_instance_func = create_instance,
        .free_instance_func = free_instance,
        .recreate_instance_func = nullptr,
        .get_virtual_func = nullptr,
        .get_virtual_call_data_func = nullptr,
        .call_virtual_with_data_func = nullptr,
        .get_rid_func = nullptr,
        .class_userdata = nullptr,
    };
    api.register_class(class_library, &class_name, &parent_name, &info);
    api.string_name_destructor(&class_name);
    api.string_name_destructor(&parent_name);
}

void deinitialize(void *, GDExtensionInitializationLevel) {}

template <typename Function>
Function get_proc(GDExtensionInterfaceGetProcAddress get_proc_address,
                  const char *name) {
    return reinterpret_cast<Function>(get_proc_address(name));
}

} // namespace

extern "C" CLAY_GDE_EXPORT GDExtensionBool clay_gdextension_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {
    if (p_get_proc_address == nullptr || p_library == nullptr ||
        r_initialization == nullptr) {
        return 0;
    }

    class_library = p_library;
    api.register_class = get_proc<GDExtensionInterfaceClassdbRegisterExtensionClass3>(
        p_get_proc_address, "classdb_register_extension_class3");
    api.construct_object = get_proc<GDExtensionInterfaceClassdbConstructObject>(
        p_get_proc_address, "classdb_construct_object");
    api.set_instance = get_proc<GDExtensionInterfaceObjectSetInstance>(
        p_get_proc_address, "object_set_instance");
    api.mem_alloc = get_proc<GDExtensionInterfaceMemAlloc>(p_get_proc_address,
                                                            "mem_alloc");
    api.mem_free = get_proc<GDExtensionInterfaceMemFree>(p_get_proc_address,
                                                          "mem_free");
    api.string_name_new = get_proc<GDExtensionStringNameNewWithLatin1Chars>(
        p_get_proc_address, "string_name_new_with_latin1_chars");
    const auto get_destructor = get_proc<GDExtensionVariantGetPtrDestructor>(
        p_get_proc_address, "variant_get_ptr_destructor");
    api.string_name_destructor = get_destructor == nullptr
                                     ? noop_string_name_destructor
                                     : get_destructor(
                                           GDEXTENSION_VARIANT_TYPE_STRING_NAME);
    if (api.register_class == nullptr || api.construct_object == nullptr ||
        api.set_instance == nullptr || api.mem_alloc == nullptr ||
        api.mem_free == nullptr || api.string_name_new == nullptr) {
        api = {};
    }


    r_initialization->minimum_initialization_level =
        GDEXTENSION_INITIALIZATION_SCENE;
    r_initialization->userdata = nullptr;
    r_initialization->initialize = initialize;
    r_initialization->deinitialize = deinitialize;
    return 1;
}
