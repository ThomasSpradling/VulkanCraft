#!/usr/bin/env python3
import os, sys, argparse, subprocess
from pathlib import Path

EXTENSIONS = {".vert", ".frag", ".tesc", ".tese", ".geom", ".comp",
              ".rgen", ".rmiss", ".rchit", ".rahit", ".rint", ".rcall"}

def find_shader_files(root: Path):
    for d, _, fns in os.walk(root):
        for fn in fns:
            p = Path(d) / fn
            if p.suffix.lower() in EXTENSIONS:
                yield p

def compile_shader(src: Path, validator: str,
                   include: Path | None, dbg_flag: str) -> None:
    out = src.with_suffix(src.suffix + ".spv")
    cmd = [validator, "-V", "--target-env", "vulkan1.2"]
    if dbg_flag != "off":
        cmd.append("-" + dbg_flag)
    if include:
        cmd.append(f"-I{include}")
    cmd += [src, "-o", out]

    print(f"Compiling {src.relative_to(Path.cwd())}")
    subprocess.run([str(x) for x in cmd], check=True)

def main() -> None:
    ap = argparse.ArgumentParser(description="Recursively compile GLSL to SPIR-V")
    ap.add_argument("--debug", choices=["off", "g", "gS", "gVS"], default="off",
                    help="debug level: off, g(line), gS(+src), gVS(+NonSemantic) [default: off]")
    ap.add_argument("-I", "--include", dest="inc", help="add include path")
    ap.add_argument("-V", "--validator", default="glslangValidator",
                    help="path to glslangValidator")
    ap.add_argument("root", nargs="?", default=".", help="root dir (default: .)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if not root.is_dir(): sys.exit(f"ERROR: {root} is not a directory")
    inc = Path(args.inc).resolve() if args.inc else None
    if inc and not inc.is_dir(): sys.exit(f"ERROR: {inc} is not a directory")

    for src in find_shader_files(root):
        compile_shader(src, args.validator, inc, args.debug)

if __name__ == "__main__":
    main()
