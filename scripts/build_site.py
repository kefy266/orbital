#!/usr/bin/env python3
"""Generates docs/index.html (the GitHub Pages site) from docs/template.html,
filling the store showcase from store/index.json."""
import html
import json
import pathlib
import shutil

root = pathlib.Path(__file__).resolve().parent.parent
items = json.loads((root / "store" / "index.json").read_text(encoding="utf-8"))["items"]

ICONS = {
    "skill": '<svg class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke-width="1.8" stroke="currentColor" aria-hidden="true"><path stroke-linejoin="round" d="M12 3.5c.6 4.4 2.4 6.3 7 7-4.6.7-6.4 2.6-7 7-.6-4.4-2.4-6.3-7-7 4.6-.7 6.4-2.6 7-7z"/></svg>',
    "plugin": '<svg class="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke-width="1.8" stroke="currentColor" aria-hidden="true"><rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><rect x="13" y="13" width="7" height="7" rx="1.5"/><path stroke-linecap="round" d="M16.5 4v6M13.5 7h6"/></svg>',
}
# Ürün tasarım dili: skill amber, eklenti mavi yumuşak ton karosu
TON = {"skill": "bg-amber-50 text-amber-600 ring-amber-100", "plugin": "bg-blue-50 text-blue-600 ring-blue-100"}

cards = []
for item in items:
    kind = "Eklenti" if item["type"] == "plugin" else "Skill"
    tags = "".join(f'<span class="cip bg-white text-slate-500 ring-slate-200">{html.escape(t)}</span>' for t in item.get("tags", [])[:2])
    cards.append(
        f'<article class="arac-kart" data-tur="{item["type"]}">'
        f'<div class="flex items-center gap-3"><span class="ton h-10 w-10 {TON[item["type"]]}">{ICONS[item["type"]]}</span>'
        f'<div class="min-w-0"><h3 class="truncate font-semibold text-slate-900">{html.escape(item["name"])}</h3>'
        f'<p class="text-xs text-slate-500">{kind} · v{html.escape(item["version"])}</p></div></div>'
        f'<p class="mt-3 flex-1 text-sm leading-6 text-slate-500">{html.escape(item["description"])}</p>'
        f'<div class="mt-4 flex flex-wrap gap-1.5">{tags}</div>'
        f"</article>"
    )

template = (root / "docs" / "template.html").read_text(encoding="utf-8")
out = template.replace("{{STORE_CARDS}}", "\n".join(cards)).replace("{{STORE_COUNT}}", str(len(items)))
(root / "docs" / "index.html").write_text(out, encoding="utf-8")
print(f"docs/index.html: {len(items)} store items")

# Sync store catalog to docs/store for GitHub Pages hosting
docs_store = root / "docs" / "store"
if docs_store.exists():
    shutil.rmtree(docs_store)
shutil.copytree(root / "store", docs_store)
print(f"docs/store: synced {len(items)} items to GitHub Pages")
