"""Generate a UTF-16 LE with BOM copy of LICENSE for NSIS.

NSIS 3.x Unicode installers auto-detect the 0xFF 0xFE BOM at the start of
a license text file and decode the rest as UTF-16 LE. Without the BOM,
NSIS falls back to the system ANSI codepage (CP936/GBK on Chinese Windows),
which turns any non-ASCII byte (e.g. the "息间" in Margin's LICENSE header)
into mojibake on the installer's license page.

Usage:
    python cmake/gen_nsis_license.py <input-utf8> <output-utf16le>

Called from the top-level CMakeLists.txt at configure time via
execute_process(), so the NSIS copy is regenerated whenever the source
LICENSE changes — no manual resync.

Why a generated file instead of committing one:
    LICENSE is the SSOT and already required by the install rules. A
    second hand-maintained copy would inevitably drift.
"""

from __future__ import annotations

import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        sys.stderr.write(f"usage: {argv[0]} <input-utf8> <output-utf16le>\n")
        return 2

    src = Path(argv[1])
    dst = Path(argv[2])

    text = src.read_text(encoding="utf-8")
    # "utf-16-le" alone does not prepend a BOM; we need it explicitly so
    # NSIS's loader detects the encoding.
    dst.write_bytes(b"\xff\xfe" + text.encode("utf-16-le"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
