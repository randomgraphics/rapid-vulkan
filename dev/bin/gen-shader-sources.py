#!/usr/bin/python3

import sys, pathlib, subprocess, argparse, os, utils

def bin2h(input, output):
    with open(input, "rb") as f:
        data = f.read()
    with open(output, "wt") as f:
        varName = pathlib.Path(input).stem.replace("-", "_").replace(".", "_")
        f.write(f"#pragma once\nstatic const unsigned char {varName}[] = {{\n")
        for i in range(0, len(data), 16):
            f.write("    ")
            for j in range(0, 16):
                if i + j < len(data):
                    f.write(f"0x{data[i+j]:02x}, ")
            f.write("\n")
        f.write("};\n")

def recompile():
    # search for shader sources
    patterns = args.files
    if 0 == len(patterns): patterns = ["*.frag", "*.vert", "*.comp"]
    sources = []
    for p in patterns:
        sources += list(source_dir.glob(p))
    if 0 == len(sources):
        utils.logw(f"no shader source found in {source_dir}. Please run this script in the folder where shaders are located.")
        sys.exit(-1)

    # Search for glslc
    if sys.platform == "win32" :
        if os.environ.get("VULKAN_SDK") != None:
            glslc = pathlib.Path(os.environ.get("VULKAN_SDK")) / "bin/glslc.exe"
        else:
            utils.loge("environment variable VULKAN_SDK not found.")
            sys.exit(-1)
    else:
        # on other platform, assume glslc is on path.
        glslc = "glslc"

    # compile shaders one by one
    subproc = {}
    for s in sources:
        print(s.name)
        # compile shader into .spv
        output = str(output_dir / s.name) + ".spv"
        cmdline = [str(glslc), str(s), "-o", output, "-g", "-O"]
        print(' '.join(cmdline))
        subproc[s] = subprocess.Popen(cmdline)
        # convert .spv to .h
        bin2h(output, output + ".h")
    for s in subproc:
        p = subproc[s]
        p.wait()
        if p.returncode != 0: utils.loge(f"failed to compile {s}. ReturnCode = {p.returncode}")

def clean():
    print(f"Delete all .spriv files in {output_dir}")
    outputs = output_dir.glob("*.spirv")
    count = 0
    for o in outputs:
        o.unlink()
        ++count
    print(f"{count} files deleted.")

ap = argparse.ArgumentParser("compile", description="Compile shader sources in current folder to SPIR-V.")
ap.add_argument("-c", "--clean", action="store_true", help="Clear shader output folder.")
ap.add_argument("files", nargs="*", help="Specify patterns used to search for shaders sources. If not specified, then recompile all shaders in current folder.")
args = ap.parse_args()

this_script = pathlib.Path(__file__)
sdk_root_dir = this_script.parent.parent.parent.absolute()
source_dir = pathlib.Path.cwd().absolute()
output_dir = source_dir # sdk_root_dir / "build" / source_dir.relative_to(sdk_root_dir)

if args.clean:
    clean()
else:
    recompile()
