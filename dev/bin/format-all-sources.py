#!/usr/bin/python3

import sys, pathlib, subprocess, platform
import importlib; utils = importlib.import_module("rapid-vulkan-utils")

# get the root directory of the code base
this_script = pathlib.Path(__file__)
script_dir = this_script.parent.absolute()
sdk_root_dir = script_dir.parent.parent
print(sdk_root_dir)

# Gather all GIT managed source files
all_files = subprocess.check_output(["git", "ls-files", "*.h", "*.hpp", "*.inl", "*.c", "*.cpp", "*.java", "*.glsl", "*.frag", "*.vert", "*.comp"], cwd=sdk_root_dir).decode("utf-8").splitlines()

# Remove all 3rd party sources
def is_our_source(x):
     if x.find("3rdparty") >= 0: return False
     if x.find("3rd-party") >= 0: return False
     return True
our_sources = [x for x in all_files if is_our_source(x)]

# determine clang-format-14 binary name
if "Darwin" == platform.system():
     clang_format = script_dir / "clang-format" / "clang-format-22.1.0-apple"
elif "Windows" == platform.system():
     clang_format = script_dir / "clang-format" / "clang-format-22.1.0.exe"
else:
     clang_format = script_dir / "clang-format" / "clang-format-22.1.0-x64-linux"

# run clang-format on all of them
for x in our_sources:
    cmdline = [str(clang_format.absolute()), "-i", x]
    print(' '.join(cmdline))
    subprocess.check_call(cmdline, cwd=sdk_root_dir)
