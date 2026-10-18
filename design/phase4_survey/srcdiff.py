#!/usr/bin/env python3
"""Compare source files referenced by upstream CMake vs a Bazel BUILD file.

usage: srcdiff.py <module_root> <BUILD.bazel>
"""
import os
import re
import sys

root, build = sys.argv[1], sys.argv[2]
EXT = (".cpp", ".mm", ".c", ".cc", ".cxx")
SKIP = ("/tests/", "/examples/", "/doc/", "/manual/", "/benchmarks/")

cm = {}
for dp, dn, fn in os.walk(root):
    rel = os.path.relpath(dp, root)
    if any(s in "/" + rel + "/" for s in SKIP):
        continue
    for f in fn:
        if f != "CMakeLists.txt" and not f.endswith(".cmake"):
            continue
        p = os.path.join(dp, f)
        ctx = ""
        with open(p, errors="replace") as fh:
            for ln, line in enumerate(fh, 1):
                s = line.split("#")[0]
                m = re.search(r"(qt_internal_extend_target|qt_internal_add_\w+|qt_add_\w+|add_library|add_executable|qt_internal_add_simd_part)\s*\(\s*([^\s)]*)(.*)", s)
                if m:
                    ctx = m.group(1) + "(" + m.group(2) + " " + m.group(3).strip()[:120]
                for tok in re.findall(r"[\w./${}+-]+", s):
                    if tok.endswith(EXT) and "${" not in tok:
                        full = os.path.normpath(os.path.join(rel, tok))
                        cm.setdefault(full, (os.path.relpath(p, root), ln, ctx))

text = open(build).read()
bz = set(re.findall(r'"([^"\s]+\.(?:cpp|mm|c|cc|cxx))"', text))

missing = sorted(k for k in cm if k not in bz)
by = {}
for k in missing:
    f, ln, ctx = cm[k]
    by.setdefault((f, ctx), []).append((ln, k))
print("## CMake sources missing from BUILD (%d)" % len(missing))
for (f, ctx), items in sorted(by.items()):
    print("- %s :: %s" % (f, ctx))
    for ln, k in items:
        print("    L%d %s" % (ln, k))
extra = sorted(k for k in bz if k not in cm)
print("\n## BUILD sources not referenced by CMake (%d)" % len(extra))
for k in extra:
    print("  " + k)
