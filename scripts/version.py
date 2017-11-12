#!/usr/bin/env python3

import os
import sys
from subprocess import check_output
from collections import OrderedDict

assert(len(sys.argv) == 5)

root = sys.argv[1]
inpath = sys.argv[2]
buildtype = sys.argv[3].strip()
v_fallback = sys.argv[4].strip()

try:
    git = check_output(['git', 'describe', '--tags', '--match', 'v[0-9]*[!asz]'], cwd=root, universal_newlines=True)
    version_str = git.strip()
except:
    print("Warning: git not found or not a git repository; using fallback version {0}".format(v_fallback), file=sys.stderr)
    version_str = v_fallback

version_str = version_str.lstrip("v")
vparts = version_str.split("-")[0].split(".")
vextra = version_str.replace("+", "-").split("-")
if len(vextra) > 1:
    vextra = vextra[1]
else:
    vextra = "0"

if len(vparts) > 3:
    print("Error: Too many dot-separated elements in version string '{0}'. Please use the following format: [v]major[.minor[.patch]][-tweak[-extrainfo]]".format(version_str), file=sys.stderr)
    sys.exit(1)
elif len(vparts[0]) == 0 or not vextra.isnumeric():
    print("Error: Invalid version string '{0}'. Please use the following format: [v]major[.minor[.patch]][-tweak[-extrainfo]]".format(version_str), file=sys.stderr)
    sys.exit(1)

version = [("${TAISEI_VERSION_MAJOR}", 0),
           ("${TAISEI_VERSION_MINOR}", 0),
           ("${TAISEI_VERSION_PATCH}", 0),
           ("${TAISEI_VERSION_TWEAK}", vextra),
           ("${TAISEI_VERSION}", version_str),
           ("${TAISEI_VERSION_FULL_STR}", "Taisei v" + version_str),
           ("${MESON_BUILD_TYPE}", buildtype)]

for p in vparts:
    c = version.pop(0)
    version.append((c[0], int(p)))

with open(inpath, "r") as infile:
    template = infile.read()

    for k, v in version:
        template = template.replace(k, str(v))

    print(template)
