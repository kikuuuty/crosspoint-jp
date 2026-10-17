#!/usr/bin/env python3
"""Host path tests, handler-wiring checks, and execution/HTML parsing of FilesPage."""

from html.parser import HTMLParser
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


def check_handler_wiring(source):
    # These are source-wiring checks, not emulation of Arduino HTTP/WS parsing.
    for name, expression in {
        "handleFileListData()": 'normalizeWebPath(server->arg("path"))',
        "handleDownload()": 'normalizeWebPath(server->arg("path"))',
        "handleUpload(UploadState& state)": 'normalizeWebPath(server->arg("path"))',
        "handleCreateFolder()": 'normalizeWebPath(server->arg("path"))',
        "handleRename()": 'normalizeWebPath(server->arg("path"))',
        "handleMove()": 'normalizeWebPath(server->arg("dest"))',
        "handleDelete()": 'normalizeWebPath(p.as<String>())',
        "onWebSocketEvent(": 'normalizeWebPath(msg.substring(secondColon + 1))',
    }.items():
        body = function(source, "void CrossPointWebServer::" + name)
        assert expression in body, f"{name} must normalize its user-supplied path"
    for name, component in {
        "handleUpload(UploadState& state)": "state.fileName",
        "handleCreateFolder()": "folderName",
        "handleRename()": "newName",
        "onWebSocketEvent(": "wsUploadFileName",
    }.items():
        body = function(source, "void CrossPointWebServer::" + name)
        guard = f"if (!FsHelpers::isSafePathComponent({component}))"
        position = body.index(guard)
        assert position < body.index("Storage."), f"{name} must reject unsafe names before storage access"
        assert "return;" in body[position:body.index("}", position)], f"{name} must stop on rejection"
    print("HTTP/WS validation wiring checks passed.", flush=True)


class FileMarkup(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.actions = []
        self.tags = []

    def handle_starttag(self, tag, attributes):
        self.tags.append(tag)
        attrs = dict(attributes)
        assert not any(key.lower().startswith("on") for key in attrs), "untrusted row must not generate event attributes"
        if tag == "button":
            self.actions.append(attrs)


def check_page(node, root, directory):
    files = [
        {"name": name, "isDirectory": folder, "isEpub": name.endswith(".epub"), "size": 123}
        for name, folder in [
            ("日本語 '& %20", True),
            ("O'Brien & <本文>.epub", False),
            ('file." autofocus onfocus="globalThis.injected=1.epub', False),
            ("file.<img src=x onerror=alert(1)>", False),
            ("literal% &quot; &#039;.epub", False),
            ("volume..2.epub", False),
        ]
    ]
    fixtures = [{"path": path, "files": files} for path in ("/", "/Books/親 '& 100% %20")]
    fixture_path = directory / "fixtures.json"
    fixture_path.write_text(json.dumps(fixtures, ensure_ascii=False), encoding="utf-8")
    output = subprocess.check_output(
        [node, str(root / "test/web_transfer/FilesPageTest.js"),
         str(root / "src/network/html/FilesPage.html"), str(fixture_path)],
        text=True, encoding="utf-8",
    )
    results = json.loads(output)
    assert len(results) == len(fixtures)
    for result in results:
        parser = FileMarkup()
        # Ignore the static select-all header, whose inline change handler has
        # no untrusted values; validate every generated file/folder row.
        rows = result["markup"].split("</tr>", 1)[1]
        parser.feed(rows)
        assert set(parser.tags) <= {"tr", "td", "input", "span", "a", "div", "button"}, "file names must not create markup"
        expected = []
        for file in result["files"]:
            path = result["path"].rstrip("/") + "/" + file["name"]
            for action in (["delete"] if file["isDirectory"] else ["move", "rename", "delete"]):
                expected.append((action, file["name"], path, str(file["isDirectory"]).lower() if action == "delete" else None))
        actual = [(a.get("data-action"), a.get("data-name"), a.get("data-path"), a.get("data-folder")) for a in parser.actions]
        assert sorted(actual) == sorted(expected), "HTML-decoded data attributes must round-trip names/paths exactly"
    print("FilesPage script/HTML tests passed (quotes, entities, Japanese, literal %, action delegation, re-hydration).")


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test/web_transfer"
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))), None,
    )
    node = shutil.which("node")
    if not compiler or not node:
        raise SystemExit("A C++20 compiler (or CXX) and Node.js are required.")
    source = (root / "src/network/CrossPointWebServer.cpp").read_text(encoding="utf-8")
    check_handler_wiring(source)
    build = root / "build/web_transfer"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        directory = Path(directory)
        wrapper = directory / "WebPath.cpp"
        wrapper.write_text('#include "FsHelpers.h"\n' + function(source, "String normalizeWebPath("), encoding="utf-8")
        binary = directory / ("WebPathTest.exe" if os.name == "nt" else "WebPathTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(tests / "stubs"), "-I", str(root / "lib/FsHelpers"),
             str(tests / "WebPathTest.cpp"), str(wrapper), str(root / "lib/FsHelpers/FsHelpers.cpp"), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
        check_page(node, root, directory)


if __name__ == "__main__":
    main()
