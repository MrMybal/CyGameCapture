#!/usr/bin/env python3
"""Validate offline guide links, language parity and exact assets in shipped folders/ZIPs."""
import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "Docs" / "UserGuide"


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__(convert_charrefs=True)
        self.ids, self.links, self.chapters, self.images = set(), [], [], []
        self.language = None
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "html":
            self.language = a.get("lang")
        if "id" in a:
            assert a["id"] not in self.ids, f'Duplicate ID: {a["id"]}'
            self.ids.add(a["id"])
        if tag == "section":
            self.chapters.append(a["id"])
        if tag == "img":
            assert "alt" in a, "Image missing alternative text"
            self.images.append(a["src"])
        for name in ("href", "src"):
            if name in a:
                self.links.append((tag, a[name]))


def validate_folder(folder):
    pages = {}
    for name, language in (("index.html", "en"), ("en.html", "en"), ("fr.html", "fr")):
        text = (folder / name).read_text(encoding="utf-8")
        assert "{{" not in text, f"Unexpanded template in {name}"
        assert not re.search(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", text), f"Control character in {name}"
        page = pages[name] = Page(text)
        assert page.language == language
        assert len(page.chapters) == 17
        assert len(page.images) == 7  # logo + six actual application screenshots
        assert text.count('class="diagram"') == 3
    assert pages["en.html"].chapters == pages["fr.html"].chapters
    assert (folder / "index.html").read_bytes() == (folder / "en.html").read_bytes(), "Default must be English"
    for name, page in pages.items():
        for tag, link in page.links:
            target = urlsplit(link)
            if target.scheme:
                assert tag == "a" and target.scheme == "https", f"Unexpected external resource: {link}"
                continue
            assert not target.netloc and not target.query
            path = unquote(target.path) or name
            resolved = (folder / path).resolve()
            assert resolved.is_relative_to(folder.resolve()), f"Resource escapes guide: {link}"
            assert resolved.is_file(), f"Missing resource: {resolved}"
            if target.fragment:
                assert path in pages and unquote(target.fragment) in pages[path].ids, f"Missing anchor: {link}"
    css = (folder / "style.css").read_text(encoding="utf-8")
    assert "@import" not in css and "url(" not in css, "Unexpected CSS network dependency"
    print(f"PASS {folder}: EN/FR, 17 matching chapters, 6 screenshots, 3 diagrams, offline links")


def runtime_files():
    files = [CANONICAL / name for name in ("index.html", "en.html", "fr.html", "style.css", "manual.js")]
    return files + sorted((CANONICAL / "images").glob("*.png"))


def compare_folder(folder):
    for source in runtime_files():
        target = folder / source.relative_to(CANONICAL)
        assert target.read_bytes() == source.read_bytes(), f"Stale or missing asset: {target}"
    validate_folder(folder)


def validate_zip(path):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        guides = [n[:-len("index.html")] for n in names if n.endswith("/UserGuide/index.html")]
        assert guides, f"No manual in {path}"
        for guide in guides:
            for source in runtime_files():
                entry = guide + source.relative_to(CANONICAL).as_posix()
                assert archive.read(entry) == source.read_bytes(), f"Stale/missing ZIP asset: {entry}"
        if "ReShade-Addon" in path.name:
            assert any(g.endswith("/Documentation/UserGuide/") for g in guides)
        elif "OBS-Plugin" in path.name:
            assert any(g.endswith("/CyGameCaptureOBS/data/Documentation/UserGuide/") for g in guides)
        elif "UnrealPlugin" in path.name:
            assert any(g.endswith("/Docs/UserGuide/") for g in guides)
        elif "Complete" in path.name:
            assert len(guides) >= 10, "Missing component manuals in complete package"
            readme_name = next(n for n in names if n.endswith('/Documentation/CyGameCapture - README.md'))
            readme = archive.read(readme_name).decode('utf-8-sig')
            for language in ('en', 'fr'):
                assert f'](UserGuide/{language}.html)' in readme, 'Packaged README must use local guide links'
            for name in ("m1_buffer_inspector.png", "m1_capture_tab_two_streams.png", "m1_player_view_with_hud.png", "m1_spout_receiver_clean_scene.png"):
                assert any(n.endswith("/Documentation/Docs/Images/" + name) for n in names), "Missing README image"
    print(f"PASS {path.name}: {len(guides)} exact offline guide copies")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    args = parser.parse_args()
    validate_folder(CANONICAL)
    compare_folder(ROOT / "CyGameCaptureUE" / "Docs" / "UserGuide")
    for path in args.paths:
        if path.suffix.lower() == ".zip":
            validate_zip(path)
        elif (path / "index.html").is_file():
            compare_folder(path)
        elif list(path.glob("*.zip")):
            for archive in sorted(path.glob("*.zip")):
                validate_zip(archive)
        else:
            manuals = list(path.rglob("UserGuide/index.html"))
            assert manuals, f"No manual in {path}"
            for index in manuals:
                compare_folder(index.parent)
            readme = path / 'Documentation' / 'CyGameCapture - README.md'
            if readme.is_file():
                text = readme.read_text(encoding='utf-8-sig')
                assert '](UserGuide/en.html)' in text and '](UserGuide/fr.html)' in text


if __name__ == "__main__":
    main()
