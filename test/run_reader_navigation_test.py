#!/usr/bin/env python3
"""Build the production input mapper with a fake HAL, on Windows or Unix."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    tests = root / "test" / "reader_navigation"
    compiler = os.environ.get("CXX") or next(
        (path for name in ("c++", "g++", "clang++") if (path := shutil.which(name))),
        None,
    )
    if compiler is None:
        raise SystemExit("Set CXX to a C++ compiler (GCC or Clang).")

    build = root / "build" / "reader_navigation"
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as directory:
        directory = Path(directory)
        # Compile an unchanged copy so its quoted CrossPointSettings.h include
        # resolves to the stub, not the Arduino-dependent header beside src/.
        source = directory / "MappedInputManager.cpp"
        shutil.copyfile(root / "src" / "MappedInputManager.cpp", source)
        binary = directory / ("InputTransitionTest.exe" if os.name == "nt" else "InputTransitionTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
             "-I", str(tests / "stubs"), "-I", str(root / "src"),
             str(source), str(tests / "InputTransitionTest.cpp"), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)

        # Stage only platform/leaf-screen stubs; keep the production manager,
        # Activity base, RenderLock, result handling and input mapper unchanged.
        manager = directory / "ActivityManager.cpp"
        shutil.copyfile(root / "src" / "activities" / "ActivityManager.cpp", manager)
        for header in re.findall(r'#include "((?:boot_sleep|browser|home|network|reader|settings|util)/[^\"]+)"',
                                 manager.read_text(encoding="utf-8")):
            name = Path(header).stem
            stub = directory / header
            stub.parent.mkdir(parents=True, exist_ok=True)
            stub.write_text(
                '#pragma once\n#include "activities/Activity.h"\n'
                f'class {name} : public Activity {{ public:\n'
                f'template<class... Args> {name}(GfxRenderer& r, MappedInputManager& m, Args&&...) '
                f': Activity("{name}", r, m) {{}}\n}};\n', encoding="utf-8")
        for header, content in {
            "HalPowerManager.h": "struct HalPowerManager { struct Lock {}; };",
            "SdCardFontGlobals.h": "inline void ensureSdFontLoaded() {}",
            "OrientationHelper.h":
                '#include "activities/Activity.h"\n'
                'namespace OrientationHelper { inline void applyOrientation(GfxRenderer&, MappedInputManager&, Activity*) {} }',
            "OpdsServerStore.h":
                '#include <vector>\nstruct StubOpdsStore { std::vector<int> getServers() const { return {}; } };\n'
                'inline StubOpdsStore stubOpdsStore;\n#define OPDS_STORE stubOpdsStore',
        }.items():
            (directory / header).write_text("#pragma once\n" + content + "\n", encoding="utf-8")

        stack_binary = directory / ("NavigationStackTest.exe" if os.name == "nt" else "NavigationStackTest")
        subprocess.run(
            [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
             "-Wno-unused-variable", "-I", str(directory), "-I", str(tests / "stubs"),
             "-I", str(root / "src"), "-I", str(root / "src" / "activities"),
             str(source), str(manager), str(root / "src" / "activities" / "Activity.cpp"),
             str(tests / "NavigationStackTest.cpp"), "-o", str(stack_binary)],
            check=True,
        )
        subprocess.run([str(stack_binary)], check=True)


if __name__ == "__main__":
    main()
