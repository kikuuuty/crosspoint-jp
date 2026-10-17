#!/usr/bin/env python3
"""Exercise the real chapter parser, Expat, CSS and text layout with fake device I/O."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test/chapter_html_slim_parser"
    cxx = os.environ.get("CXX") or next(
        (p for name in ("c++", "g++", "clang++") if (p := shutil.which(name))), None)
    cc = os.environ.get("CC") or next(
        (p for name in ("cc", "gcc", "clang") if (p := shutil.which(name))), None)
    if not cxx or not cc:
        raise SystemExit("Set CC/CXX to C and C++ compilers (GCC or Clang).")
    build = root / "build/chapter_html_slim_parser"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as temp:
        temp = Path(temp)
        # Keep the production chapter/CSS/layout sources and headers unchanged.
        # Only the EPUB archive boundary is substituted; its real implementation
        # pulls in device networking/storage irrelevant to XHTML callback tests.
        staged = temp / "Epub"
        shutil.copytree(root / "lib/Epub/Epub", staged / "Epub")
        shutil.copyfile(tests / "stubs/Epub.h", staged / "Epub.h")
        objects = []
        for name in ("xmlparse", "xmlrole", "xmltok"):
            obj = temp / f"{name}.o"
            subprocess.run([cc, "-DXML_STATIC", "-DXML_GE=1", "-DXML_CONTEXT_BYTES=1024",
                            "-D_CRT_SECURE_NO_WARNINGS", "-DBYTEORDER=1234", "-c",
                            str(root / f"lib/expat/{name}.c"), "-o", str(obj)], check=True)
            objects.append(str(obj))
        binary = temp / ("ChapterParserTest.exe" if os.name == "nt" else "ChapterParserTest")
        args = [cxx, "-std=c++20", "-fno-exceptions", "-fno-rtti", "-fno-access-control",
                "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-unused-function",
                "-Wno-reorder", "-Wno-parentheses", "-D_CRT_SECURE_NO_WARNINGS", "-DXML_STATIC"]
        if "clang" in subprocess.check_output([cxx, "--version"], text=True).lower():
            args += ["-Wno-unused-private-field"]
        for include in [tests / "stubs", root / "test/ruby/stubs", staged, root / "lib/GfxRenderer",
                        root / "lib/EpdFont", root / "lib/Utf8", root / "lib/Serialization",
                        root / "lib/FsHelpers", root / "lib/expat"]:
            args += ["-I", str(include)]
        args += [str(tests / "ChapterHtmlSlimParserTest.cpp"), str(tests / "ParserDependencies.cpp")]
        args += [str(staged / f"Epub/{name}.cpp") for name in
                 ("parsers/ChapterHtmlSlimParser", "ParsedText", "Page", "htmlEntities", "blocks/TextBlock",
                  "blocks/TableRowBlock", "css/CssParser")]
        args += [str(root / "lib/Utf8/Utf8.cpp"), *objects, "-o", str(binary)]
        subprocess.run(args, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
