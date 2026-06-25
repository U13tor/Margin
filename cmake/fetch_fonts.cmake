# cmake/fetch_fonts.cmake
#
# M4-C2: best-effort configure-time fetch of Inter (400/500/600) + JetBrains
# Mono (400/500) TTFs into src/resources/fonts/. Downloaded once, cached on
# subsequent configures. CLAUDE.md §5 bans *runtime* network — build-time
# fetch is permitted; the resulting .ttf bytes are bundled via fonts.qrc and
# shipped inside the executable.
#
# Failure mode: if any download fails, a WARNING is logged and that file is
# simply absent from disk. fonts.qrc is generated dynamically (in the build
# tree) to list only the files that actually landed, so AUTORCC never fails
# on a missing reference. At runtime, HostCore::bootstrap calls
# QFontDatabase::addApplicationFont for every face; missing faces return -1
# and Theme.fontSans/fontMono fall back to the OS via the name-list
# (PingFang SC → Cascadia Code → Consolas → system default).
#
# Idempotent: re-running cmake with the fonts already present is a no-op.
# Force a re-fetch by deleting src/resources/fonts/*.ttf.

set(_fonts_dir "${CMAKE_SOURCE_DIR}/src/resources/fonts")
file(MAKE_DIRECTORY "${_fonts_dir}")

# Fetch a single URL into dest if dest does not already exist. Best-effort:
# logs WARNING on failure, leaves dest absent.
function(_margin_fetch_one url dest)
    if(EXISTS "${dest}")
        return()
    endif()
    message(STATUS "fonts: fetching ${url}")
    file(DOWNLOAD "${url}" "${dest}.tmp"
         STATUS _stat INACTIVITY_TIMEOUT 30 TIMEOUT 300)
    list(GET _stat 0 _code)
    if(_code EQUAL 0)
        file(RENAME "${dest}.tmp" "${dest}")
    else()
        file(REMOVE "${dest}.tmp")
        list(GET _stat 1 _err)
        message(WARNING "fonts: download failed (code=${_code} msg='${_err}'): ${url}")
    endif()
endfunction()

# --- Inter v4.1 -----------------------------------------------------------
# 3 static TTFs (Regular/Medium/SemiBold) live inside the 33MB release zip at
# extras/ttf/. Single-file static URLs aren't published by rsms, so we accept
# the one-time 33MB download for the official artifact + LICENSE.
set(_inter_ttf_names Inter-Regular.ttf Inter-Medium.ttf Inter-SemiBold.ttf)
set(_inter_all_present TRUE)
foreach(_f ${_inter_ttf_names})
    if(NOT EXISTS "${_fonts_dir}/${_f}")
        set(_inter_all_present FALSE)
    endif()
endforeach()
if(NOT _inter_all_present)
    _margin_fetch_one(
        "https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip"
        "${_fonts_dir}/.Inter-4.1.zip")
    if(EXISTS "${_fonts_dir}/.Inter-4.1.zip")
        file(ARCHIVE_EXTRACT INPUT "${_fonts_dir}/.Inter-4.1.zip"
             DESTINATION "${_fonts_dir}/.inter-extract")
        foreach(_f ${_inter_ttf_names})
            if(EXISTS "${_fonts_dir}/.inter-extract/extras/ttf/${_f}")
                file(RENAME "${_fonts_dir}/.inter-extract/extras/ttf/${_f}"
                             "${_fonts_dir}/${_f}")
            endif()
        endforeach()
        if(EXISTS "${_fonts_dir}/.inter-extract/LICENSE.txt")
            file(RENAME "${_fonts_dir}/.inter-extract/LICENSE.txt"
                         "${_fonts_dir}/LICENSE-Inter.txt")
        endif()
        file(REMOVE_RECURSE "${_fonts_dir}/.inter-extract")
        file(REMOVE "${_fonts_dir}/.Inter-4.1.zip")
    endif()
endif()

# --- JetBrains Mono 2.304 -------------------------------------------------
# Static single-file TTFs are available directly from the JetBrains/JetBrainsMono
# GitHub repo. Two weights per the M4-C2 user decision (Regular 400 + Medium 500).
_margin_fetch_one(
    "https://github.com/JetBrains/JetBrainsMono/raw/master/fonts/ttf/JetBrainsMono-Regular.ttf"
    "${_fonts_dir}/JetBrainsMono-Regular.ttf")
_margin_fetch_one(
    "https://github.com/JetBrains/JetBrainsMono/raw/master/fonts/ttf/JetBrainsMono-Medium.ttf"
    "${_fonts_dir}/JetBrainsMono-Medium.ttf")
_margin_fetch_one(
    "https://raw.githubusercontent.com/JetBrains/JetBrainsMono/master/OFL.txt"
    "${_fonts_dir}/LICENSE-JetBrainsMono.txt")

# --- Generate fonts.qrc ---------------------------------------------------
# Built into the build tree so it can be added to qt_add_executable sources
# without polluting the source tree. Lists only files actually on disk — if a
# fetch failed above, the entry is skipped, and HostCore::bootstrap's
# addApplicationFont call for that path will return -1 at runtime (no crash).
set(_qrc_entries "")
set(_all_font_files
    Inter-Regular.ttf
    Inter-Medium.ttf
    Inter-SemiBold.ttf
    JetBrainsMono-Regular.ttf
    JetBrainsMono-Medium.ttf)
foreach(_f ${_all_font_files})
    if(EXISTS "${_fonts_dir}/${_f}")
        string(APPEND _qrc_entries "        <file alias=\"${_f}\">${_fonts_dir}/${_f}</file>\n")
    else()
        message(STATUS "fonts: ${_f} not present — skipping qrc entry; runtime will fall back")
    endif()
endforeach()

file(WRITE "${CMAKE_BINARY_DIR}/fonts.qrc"
"<!DOCTYPE RCC>
<RCC version=\"1.0\">
    <qresource prefix=\"/fonts\">
${_qrc_entries}    </qresource>
</RCC>
")

set(MARGIN_FONTS_QRC "${CMAKE_BINARY_DIR}/fonts.qrc" CACHE INTERNAL "Path to generated fonts.qrc")
