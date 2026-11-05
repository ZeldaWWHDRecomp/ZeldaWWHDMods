#!/usr/bin/env python3
"""Test, compile and validate the source-only mod, then produce a .wwhdmod."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import zipfile

MOD = Path(__file__).resolve().parents[1]
HERE = MOD / "src"
REPO = Path(os.environ["WWHD_SDK"]).resolve()


def run(args):
    subprocess.run([str(x) for x in args], cwd=REPO, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=MOD.parent / "build/enemy-hp-bars")
    parser.add_argument("--tests-only", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = shlex.split(os.environ.get("CC", "cc"))
    run(cc + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-I", HERE,
              HERE / "core.c", MOD / "tests/core_test.c", "-o", out / "core-test"])
    run([out / "core-test"])
    manifest = json.loads((MOD / "manifest.json").read_text())
    ids = [option["id"] for option in manifest["options"]]
    assert len(ids) == len(set(ids)) == 7
    for option in manifest["options"]:
        assert option["live"] and option["menu"]
        if option["type"] == "enum":
            assert option["default"] in option["choices"]
        elif option["type"] == "number":
            assert option["min"] <= option["default"] <= option["max"]
    if args.tests_only:
        return
    clang = os.environ.get("WWHD_PPC_CLANG", "clang")
    lld = os.environ.get("WWHD_PPC_LLD", "ld.lld")
    # The runtime's MUSTTAIL dispatch requires Clang (as in install-time setup).
    host = shlex.split(os.environ.get("WWHD_HOST_CC", clang))
    objects = []
    for stem in ("mod",):
        obj = out / (stem + ".o")
        run([clang, "--target=powerpc-unknown-eabi", "-mcpu=750", "-std=c11", "-O2",
             "-ffreestanding", "-fno-builtin", "-nostdlib", "-fno-jump-tables",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
             "-I", REPO / "runtime/guest/include", "-c", HERE / (stem + ".c"), "-o", obj])
        objects.append(obj)
    package = out / "package"
    package.mkdir(exist_ok=True)
    run([lld, "-m", "elf32ppc", "-r", *objects, "-o", package / "mod.elf"])
    for name in ("manifest.json", "README.md"):
        (package / name).write_bytes((MOD / name).read_bytes())
    sys.path.insert(0, str(REPO / "tools/guestmod"))
    import build_guest_mod as builder
    from builds import by_name
    for region in ("USA", "EU"):
        build = by_name(region)
        translated = builder.translator_for((package / "mod.elf").read_bytes(), 0x7F000000, build)
        assert all(h[0] != 1 for h in translated.hooks), "display mod must not replace game functions"
        source = out / ("translated-" + region + ".c")
        source.write_text(translated.emit(manifest["id"]))
        module = out / ("translated-" + region + ".so")
        run(host + ["-O1", "-fPIC", "-shared", "-ffp-contract=off", "-fno-strict-aliasing", "-w",
                    "-I", REPO / "runtime/include", source, "-o", module])
        addresses = {"PLAY": 0x025200D4, "LINK": 0x0240EBB0, "MOBLIN": 0x021C9B1C,
                     "DELETE": 0x021CC44C, "OCTOROK": 0x023C1380, "LOAD": 0x02721908}
        (out / "test_addresses.h").write_text("".join(
            f"#define GAME_{name} 0x{build.code(address):08X}u\n" for name, address in addresses.items()))
        run(host + ["-O1", "-I", REPO / "runtime/include", "-I", out,
                    MOD / "tests/adapter_test.c", "-ldl", "-lm", "-o", out / "adapter-test"])
        run([out / "adapter-test", module])
        print(region, "translation and synthetic runtime passed", flush=True)
    archive = out / ("enemy-hp-bars-" + manifest["version"] + ".wwhdmod")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for name in ("manifest.json", "mod.elf", "README.md"):
            z.write(package / name, name)
    print(archive)


if __name__ == "__main__":
    main()
