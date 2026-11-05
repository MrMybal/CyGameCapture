#!/usr/bin/env python3
"""Rebuild the committed, offline EN/FR user guide. Python standard library only."""
import html
import json
from pathlib import Path
import shutil
import re

ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "Docs" / "UserGuide"

FRENCH_CONFIG = {
    "Language": "Langue de l’overlay : en ou fr ; modifiable près du logo.",
    "SenderName": "Nom de base personnalisé des émetteurs Spout.",
    "ViewToggleKey": "Code de touche Windows pour la vue en jeu ; 121 = F10, 0 désactive.",
    "PreserveSourceAlpha": "Valeur initiale des nouveaux flux couleur : 0 opaque, 1 alpha d’origine. Le HUD garde son masque.",
    "StandaloneWindow": "Afficher l’inspecteur dans sa fenêtre lorsque l’overlay est ouvert.",
    "FlushAfterCopy": "Soumettre immédiatement après copie ; ajoute du travail CPU.",
    "Verbose": "Journaliser chaque création/destruction de ressource pour diagnostic.",
    "AutoCapture": "Amorçage/test : sélectionner par taille:format, par #ID ou #ID@écriture. Séparer les flux par ;. Suffixes :depth et :hud. Exemple : #5@2;#5:hud. Les identifiants peuvent changer au redémarrage.",
    "DumpBuffers": "À cette frame, écrire une fois la liste des buffers dans ReShade.log.",
    "AIClient": "Client IA : auto, claude, codex ou opencode. Auto choisit le premier installé.",
    "AIModel": "Modèle transmis au client ; vide pour son modèle par défaut.",
    "AIClientPath": "Chemin complet de l’exécutable du client si la détection échoue.",
    "AILanguage": "Langue des réponses IA : fr ou en ; vide pour suivre l’overlay.",
    "AISearchOnStart": "Test : lancer une analyse à cette frame, au plus tôt à la frame 60.",
    "OpenOverlayOnStart": "Test : ouvrir l’overlay une fois le jeu stabilisé.",
    "DepthFarPlane": "Distance du plan lointain, mappée sur le blanc par la linéarisation.",
    "DepthReversed": "1 lorsque le plan proche est encodé à 1 dans la profondeur du jeu.",
    "DepthLogarithmic": "1 pour une profondeur logarithmique.",
    "DepthUpsideDown": "1 si la profondeur est inversée verticalement par rapport à la couleur.",
    "HudThreshold": "En dessous de cet écart, considérer le pixel comme scène seule.",
    "HudSoftness": "Écart à partir duquel le masque HUD devient opaque.",
    "HudKeepFinalColour": "Garder les couleurs finales dans le HUD plutôt que la différence brute."
}


def config_table(lang):
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    table = readme.split("## Configuration", 1)[1].split("\n## ", 1)[0]
    rows = []
    for line in table.splitlines():
        match = re.match(r"\| `([^`]+)`\s*\| (.*?) \| (.*?) \|$", line)
        if not match:
            continue
        key, default, explanation = match.groups()
        if lang == "fr":
            explanation = FRENCH_CONFIG[key]
            default = default.replace("*(empty)*", "vide").replace("*(the overlay's language)*", "langue de l’overlay")
        # The source table uses Markdown code and emphasis; render just those inline forms.
        def inline(value):
            value = html.escape(value)
            value = re.sub(r"`([^`]+)`", r"<code>\1</code>", value)
            return re.sub(r"\*([^*]+)\*", r"<em>\1</em>", value)
        rows.append(f"<tr><td><code>{key}</code></td><td>{inline(default)}</td><td>{inline(explanation)}</td></tr>")
    labels = ("Key", "Default", "Meaning") if lang == "en" else ("Clé", "Défaut", "Signification")
    return '<div class="table-scroll"><table><thead><tr>' + ''.join(f'<th>{label}</th>' for label in labels) + '</tr></thead><tbody>' + ''.join(rows) + '</tbody></table></div>'


def diagram(kind, lang):
    french = lang == "fr"
    title = {"pipeline": ("The capture path", "Le chemin de capture"), "moment": ("A clean moment before the HUD", "Un moment propre avant le HUD"), "recording": ("Separate files from one frame", "Fichiers séparés pour une même frame")}[kind][int(french)]
    if kind == "pipeline":
        labels = ["Jeu + ReShade" if french else "Game + ReShade", "Buffer choisi" if french else "Selected buffer", "Texture Spout", "OBS / récepteur" if french else "OBS / receiver"]
        desc = "Le joueur garde son interface ; la texture sélectionnée part vers le récepteur." if french else "The player keeps the HUD while the selected texture reaches the receiver."
        boxes = ''.join(f'<rect x="{20+i*190}" y="38" width="160" height="60" rx="8"/><text x="{100+i*190}" y="73" text-anchor="middle">{label}</text>' for i,label in enumerate(labels))
        arrows = ''.join(f'<path d="M {182+i*190} 68 h 24 m -6 -5 l 6 5 l -6 5"/>' for i in range(3))
        body = boxes + arrows + f'<text x="400" y="135" text-anchor="middle" class="sub">{desc}</text>'
        height = 162
    elif kind == "moment":
        labels = ["1 · Effacement" if french else "1 · Clear", "2 · Scène propre" if french else "2 · Clean scene", "3 · HUD / menus", "4 · Présentation" if french else "4 · Present"]
        boxes = ''.join(f'<rect class="{"hud" if i == 2 else "stage"}" x="{20+i*190}" y="38" width="160" height="60" rx="8"/><text x="{100+i*190}" y="73" text-anchor="middle">{label}</text>' for i,label in enumerate(labels))
        arrows = ''.join(f'<path d="M {182+i*190} 68 h 24 m -6 -5 l 6 5 l -6 5"/>' for i in range(3))
        note = "Copie après 2 → flux propre ; le jeu poursuit vers le HUD." if french else "Copy after 2 → clean stream; the game continues drawing the HUD."
        body = boxes + arrows + f'<path d="M 290 100 v 25"/><text x="400" y="150" text-anchor="middle">{note}</text>'
        height = 178
    else:
        name = "Même frame N" if french else "Same frame N"
        labels = ["Scène → Clean.mkv" if french else "Scene → Clean.mkv", "HUD → HUD.mkv", "Profondeur → Depth.mkv" if french else "Depth → Depth.mkv"]
        body = f'<rect x="25" y="77" width="170" height="70" rx="8"/><text x="110" y="118" text-anchor="middle">{name}</text>'
        for i,label in enumerate(labels):
            y = 20 + i*72
            body += f'<rect x="360" y="{y}" width="400" height="52" rx="8"/><text x="560" y="{y+32}" text-anchor="middle">{label}</text><path d="M 197 112 H 270 V {y+26} H 356 m -6 -5 l 6 5 l -6 5"/>'
        height = 238
    return f'<svg class="diagram" role="img" aria-labelledby="{kind}-title" viewBox="0 0 800 {height}"><title id="{kind}-title">{title}</title>{body}</svg>'


def expand(body, lang):
    body = body.replace('\\n', '\n').replace('\\\\', '\\')
    for name in ("pipeline", "moment", "recording"):
        body = body.replace("{{" + name + "}}", diagram(name, lang))
    return body.replace("{{configuration}}", config_table(lang))


def build():
    source = json.loads((GUIDE / "guide.json").read_text(encoding="utf-8"))
    for lang in ("en", "fr"):
        ui = source["ui"][lang]
        chapters = source["chapters"]
        nav = "".join(f'<a href="#{c["id"]}">{html.escape(c[lang]["title"])}</a>' for c in chapters)
        sections = "".join(f'<section id="{c["id"]}" aria-labelledby="title-{c["id"]}"><h2 id="title-{c["id"]}">{html.escape(c[lang]["title"])}</h2>{expand(c[lang]["body"], lang)}</section>' for c in chapters)
        document = f'''<!doctype html>
<html lang="{lang}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark"><meta name="description" content="{html.escape(ui['description'], quote=True)}">
<title>CyGameCapture — {ui['title']}</title><link rel="icon" href="images/logo.png"><link rel="stylesheet" href="style.css"></head>
<body><a class="skip" href="#content">{ui['skip']}</a>
<header><a class="brand" href="#overview"><img src="images/logo.png" width="42" height="42" alt=""><span>CyGameCapture<small>{ui['title']} · 0.1.0</small></span></a>
<nav aria-label="{ui['tools']}"><a class="language" lang="en" href="en.html" {'aria-current="page"' if lang == 'en' else ''}>English</a><a class="language" lang="fr" href="fr.html" {'aria-current="page"' if lang == 'fr' else ''}>Français</a><button id="print" type="button" hidden>{ui['print']}</button><button id="menu" type="button" aria-controls="sidebar" aria-expanded="false" hidden>{ui['contents']}</button></nav></header>
<div class="layout"><aside id="sidebar"><div id="search-box" hidden><label for="search">{ui['search']}</label><input id="search" type="search" placeholder="{ui['placeholder']}" aria-controls="content"><p id="results" role="status" aria-live="polite" data-template="{ui['results']}"></p></div><nav aria-label="{ui['contents']}">{nav}</nav><p class="offline">{ui['offline']}</p></aside>
<main id="content"><div class="hero"><p class="eyebrow">GPU CAPTURE TOOLKIT</p><h1>{ui['title']}</h1><p>{ui['description']}</p><p class="version">{ui['version']}</p></div><p id="empty" hidden>{ui['empty']}</p>{sections}<footer>CyGameCapture · Cyberalien · {ui['footer']}</footer></main></div>
<script src="manual.js"></script></body></html>'''
        (GUIDE / f"{lang}.html").write_text(document, encoding="utf-8", newline="\n")
    shutil.copyfile(GUIDE / "en.html", GUIDE / "index.html")
    images = GUIDE / "images"
    images.mkdir(exist_ok=True)
    for image in (ROOT / "Docs" / "Images").glob("*.png"):
        shutil.copyfile(image, images / image.name)
    shutil.copyfile(ROOT / "Resources" / "Branding" / "CyGameCapture_Logo_256.png", images / "logo.png")
    # Unreal's existing /Docs/... BuildPlugin filter includes this offline copy.
    # Keep the content at one source; generated copies are checked by the validator.
    unreal = ROOT / "CyGameCaptureUE" / "Docs" / "UserGuide"
    unreal.mkdir(exist_ok=True)
    for file in ("index.html", "en.html", "fr.html", "style.css", "manual.js"):
        shutil.copyfile(GUIDE / file, unreal / file)
    shutil.copytree(images, unreal / "images", dirs_exist_ok=True)
    print("Built EN/FR user guide and Unreal copy; default language: English.")


if __name__ == "__main__":
    build()
