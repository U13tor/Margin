# Layer 4 smoke test (M0-C9): launch the real Margin binary headless via a
# Python runner and assert startup + clean shutdown. LABEL "smoke" so
# `ctest -L smoke` runs it on CI.
#
# Windows-only for now: the macOS host aborts at bootstrap because
# Keyring::getOrCreateMasterKey hits the Keychain qFatal stub (C6 deferred,
# see docs/12-deferred-items.md C6). The macOS smoke gate lights up when C6
# lands — same Win-only pattern as test_keyring.
#
# Python3 is optional locally (scripts/build-smoke.sh step 5 also smoke-tests
# in bash); the CI Windows runner always has it.

if(WIN32)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(
            NAME smoke_startup_shutdown
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/smoke_runner.py"
                    "$<TARGET_FILE:Margin>"
        )
        set_tests_properties(smoke_startup_shutdown PROPERTIES LABELS "smoke")
    else()
        message(WARNING
            "Python3 not found; skipping Layer-4 smoke test (CI requires it). "
            "Local smoke coverage still runs via scripts/build-smoke.sh step 5.")
    endif()
endif()
