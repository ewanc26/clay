#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gdextension_interface_minimal.hpp"

extern "C" GDExtensionBool clay_gdextension_init(
    GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr,
    GDExtensionInitialization *);

static GDExtensionInterfaceFunctionPtr stub_get_proc(const char *) {
    return nullptr;
}

TEST_CASE("GDExtension bootstrap fills the Godot initialization contract") {
    GDExtensionInitialization initialization{};
    CHECK(clay_gdextension_init(stub_get_proc, reinterpret_cast<void *>(1),
                                &initialization) != 0);
    CHECK(initialization.minimum_initialization_level ==
          GDEXTENSION_INITIALIZATION_SCENE);
    CHECK(initialization.userdata == nullptr);
    CHECK(initialization.initialize != nullptr);
    CHECK(initialization.deinitialize != nullptr);
}

TEST_CASE("GDExtension bootstrap rejects incomplete entry arguments") {
    GDExtensionInitialization initialization{};
    CHECK(clay_gdextension_init(nullptr, reinterpret_cast<void *>(1),
                                &initialization) == 0);
    CHECK(clay_gdextension_init(stub_get_proc, nullptr, &initialization) == 0);
    CHECK(clay_gdextension_init(stub_get_proc, reinterpret_cast<void *>(1),
                                nullptr) == 0);
}
