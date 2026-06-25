#!/usr/bin/env python3
"""scripts/migrate_i18n.py — PR1 i18n migration helper.

Reads an "old" .ts file (single context, hand-curated translations) and a
"new" .ts file (lupdate output with correct per-QML-file contexts, empty
translations), then backfills translations into the new file wherever the
<source> string matches.

Usage:
    # Backup before lupdate clobbers the hand-curated .ts:
    cp i18n/host_zh_CN.ts i18n/host_zh_CN.ts.bak

    # Run lupdate to regenerate contexts:
    lupdate src/resources/host.qrc src/ui/ui.qrc \
            -ts i18n/host_zh_CN.ts -no-obsolete -source-language en \
            -target-language zh_CN

    # Migrate translations back:
    python scripts/migrate_i18n.py \
        --old i18n/host_zh_CN.ts.bak \
        --new i18n/host_zh_CN.ts

    # Review <translation type="unfinished"> entries, then remove .bak.

Match policy: exact source-string match, case-sensitive. If the same source
appears N times in the new .ts (e.g. "OK" in 5 QML files), all N copies get
the backup translation. Marked type="unfinished" so a human reviews context
fit before promoting to finished.
"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def parse_translations_by_source(ts_path: Path) -> dict[str, str]:
    """Return {source: translation} from a .ts file.

    Context is ignored — the old .ts had a single <name>Margin</name>
    context and we want source-string matching across the whole file.
    If the same source appears multiple times, last-wins (the hand-curated
    .ts didn't have this issue; new .ts may, but we read old .ts only).
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()
    pairs: dict[str, str] = {}
    for msg in root.iter("message"):
        src_el = msg.find("source")
        tr_el = msg.find("translation")
        if src_el is None or tr_el is None:
            continue
        src = "".join(src_el.itertext())
        # Skip type="unfinished" with empty body — nothing to migrate.
        tr_type = tr_el.get("type", "")
        tr_text = "".join(tr_el.itertext())
        if tr_type == "unfinished" and not tr_text:
            continue
        if src and (tr_text or tr_type == "obsolete"):
            pairs[src] = tr_text
    return pairs


def backfill(ts_path: Path, backup: dict[str, str]) -> tuple[int, int]:
    """Backfill translations into ts_path in place.

    Returns (matched, total) counts for reporting.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()
    matched = 0
    total = 0
    for msg in root.iter("message"):
        src_el = msg.find("source")
        tr_el = msg.find("translation")
        if src_el is None or tr_el is None:
            continue
        total += 1
        src = "".join(src_el.itertext())
        if src in backup:
            tr_el.text = backup[src]
            # Clear any child elements (lupdate writes <translation/> as
            # empty self-closing when no translation exists)
            for child in list(tr_el):
                tr_el.remove(child)
            tr_el.set("type", "unfinished")
            # Prepend a comment so reviewer knows this came from migration
            existing_comment = msg.find("comment")
            if existing_comment is None:
                comment_el = ET.SubElement(msg, "comment")
                comment_el.text = "migrated from pre-PR1 host_zh_CN.ts; review context fit"
            matched += 1
    tree.write(ts_path, encoding="utf-8", xml_declaration=True)
    return matched, total


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old", required=True, type=Path,
                        help="Backup .ts file with hand-curated translations")
    parser.add_argument("--new", required=True, type=Path,
                        help="New lupdate-generated .ts file (modified in place)")
    args = parser.parse_args()

    if not args.old.exists():
        print(f"error: --old not found: {args.old}", file=sys.stderr)
        return 1
    if not args.new.exists():
        print(f"error: --new not found: {args.new}", file=sys.stderr)
        return 1

    backup = parse_translations_by_source(args.old)
    print(f"loaded {len(backup)} source→translation pairs from {args.old}")

    matched, total = backfill(args.new, backup)
    pct = (matched / total * 100) if total else 0
    print(f"matched {matched}/{total} ({pct:.0f}%) messages in {args.new}")
    print(f"review remaining {total - matched} <translation type=\"unfinished\"> entries"
          + ("" if matched == total else " manually"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
