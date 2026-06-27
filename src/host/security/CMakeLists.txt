# src/host/security/CMakeLists.txt
#
# Host-internal crypto primitives. NOT exposed to plugins — only the
# CryptoService interface in plugin_api is. Margin::Keyring owns master-key
# lifecycle (Win DPAPI / Mac Keychain) and the AES-256-GCM field primitive.
# CryptoServicePool derives per-plugin keys via HKDF and caches wrappers.
#
# Spec: docs/07-privacy-security.md §4, docs/05-host-services.md §8.
#
# D5-1 (Qt 6.7 baseline + drop OpenSSL): HKDF is implemented on
# QCryptographicHash::hmac (RFC 5869), AES-256-GCM uses Windows CNG (BCrypt).
# Qt 6.7 has no native AES-GCM API — see docs/12-deferred-items.md D5-1.

find_package(Qt6 6.7 REQUIRED COMPONENTS Core)

add_library(margin_security STATIC
    Keyring.cpp
    CryptoServiceImpl.cpp
    CryptoServicePool.cpp
)

target_link_libraries(margin_security
    PUBLIC  margin_plugin_api
    PRIVATE Qt6::Core
            margin_paths
)

if(WIN32)
    # CryptProtectData / CryptUnprotectData (DPAPI) in Crypt32.lib;
    # BCrypt* / AES-GCM via CNG in bcrypt.lib (Windows SDK).
    target_link_libraries(margin_security PRIVATE Crypt32 bcrypt)
endif()
