// MARGIN_ABI_VERSION - ABI contract between Host and plugin DLLs.
// Spec: docs/04-plugin-spec.md §2 (version negotiation).
//
// At load time, the manifest.json `abi_version` field (string) must equal
// MARGIN_ABI_VERSION here (string ==). Any mismatch => plugin rejected.
//
// When to bump:
//   - PluginInterface vtable layout changes (add/remove/reorder virtuals)
//   - PluginContext struct field reorder or type change
//     (appending a tail field = minor bump; changing an existing field =
//     major bump)
//   - Any Result<T,E> / HostServices ABI surface change
//
// M0-M3 allows breaking changes; v1.0 freezes ABI, breaking => major bump.
//
// CMake also injects the same macro via target_compile_definitions on
// margin_plugin_api; this header is SSOT, the CMake injection is so the
// host-side manifest validation logic can take a string literal.

#pragma once

#define MARGIN_ABI_VERSION "0.2.0"
