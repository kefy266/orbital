#!/usr/bin/env python3
"""Generates docs/index.html (the GitHub Pages site) from docs/template.html,
filling the store showcase from store/index.json."""
import html
import json
import pathlib

root = pathlib.Path(__file__).resolve().parent.parent
items = json.loads((root / "store" / "index.json").read_text(encoding="utf-8"))["items"]

ICONS = {
    "skill": '<svg viewBox="0 0 24 24"><path d="M12 3.5c.6 4.4 2.4 6.3 7 7-4.6.7-6.4 2.6-7 7-.6-4.4-2.4-6.3-7-7 4.6-.7 6.4-2.6 7-7z"/></svg>',
    "plugin": '<svg viewBox="0 0 24 24"><rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><rect x="13" y="13" width="7" height="7" rx="1.5"/><path d="M16.5 4v6M13.5 7h6"/></svg>',
}

cards = []
for item in items:
    kind = "Eklenti" if item["type"] == "plugin" else "Skill"
    tags = "".join(f'<span class="tag">{html.escape(t)}</span>' for t in item.get("tags", [])[:2])
    cards.append(
        f'<article class="store-card" data-type="{item["type"]}">'
        f'<div class="store-icon">{ICONS[item["type"]]}</div>'
        f'<div><h4>{html.escape(item["name"])}</h4><p class="meta">{kind} · v{html.escape(item["version"])}</p></div>'
        f'<p class="desc">{html.escape(item["description"])}</p>'
        f'<div class="tags">{tags}</div>'
        f"</article>"
    )

template = (root / "docs" / "template.html").read_text(encoding="utf-8")
out = template.replace("{{STORE_CARDS}}", "\n".join(cards)).replace("{{STORE_COUNT}}", str(len(items)))
(root / "docs" / "index.html").write_text(out, encoding="utf-8")
print(f"docs/index.html: {len(items)} store items")
