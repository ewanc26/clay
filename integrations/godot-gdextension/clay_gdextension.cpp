#include "gdextension_interface_minimal.hpp"

namespace {

void initialize(void *, GDExtensionInitializationLevel) {}

void deinitialize(void *, GDExtensionInitializationLevel) {}

} // namespace

extern "C" CLAY_GDE_EXPORT GDExtensionBool clay_gdextension_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {
    if (p_get_proc_address == nullptr || p_library == nullptr ||
        r_initialization == nullptr) {
        return 0;
    }

    r_initialization->minimum_initialization_level =
        GDEXTENSION_INITIALIZATION_SCENE;
    r_initialization->userdata = nullptr;
    r_initialization->initialize = initialize;
    r_initialization->deinitialize = deinitialize;
    return 1;
}
