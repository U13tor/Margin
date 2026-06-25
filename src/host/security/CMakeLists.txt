# src/host/security/CMakeLists.txt
#
# Host-internal crypto primitives. NOT exposed to plugins — only the
# CryptoService interface in plugin_api is. Margin::Keyring owns master-key
# lifecycle (Win DPAPI / Mac Keychain) and the AES-256-GCM field primitive.
# CryptoServicePool derives per-plugin keys via HKDF and caches wrappers.
#
# Spec: docs/07-privacy-security.md §4, docs/05-host-services.md §8.

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(OpenSSL REQUIRED)

add_library(margin_security STATIC
    Keyring.cpp
    CryptoServiceImpl.cpp
    CryptoServicePool.cpp
)

target_link_libraries(margin_security
    PUBLIC  margin_plugin_api
    PRIVATE Qt6::Core
            OpenSSL::Crypto
            margin_paths
)

if(WIN32)
    # CryptProtectData / CryptUnprotectData live in Crypt32.lib (Windows SDK).
    target_link_libraries(margin_security PRIVATE Crypt32)
endif()
