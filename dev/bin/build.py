#!/usr/bin/python3

from ctypes import util
import sys, subprocess, os, pathlib, argparse, shutil, glob

import importlib; utils = importlib.import_module("rapid-vulkan-utils")

# Run cmake command. the args is list of arguments.
def cmake(build_dir, cmdline):
    if not isinstance(cmdline, list):
        cmdline = str(cmdline).split()
    cmd = ["cmake"] + cmdline
    print(' '.join(cmd))
    try:
        subprocess.check_call(cmd, cwd=build_dir)
    except subprocess.CalledProcessError as err:
        print(f"[ERROR] cmake failed. ErrorCode = {err.returncode}")
        sys.exit(err.returncode)

def git(cmdline):
    if not isinstance(cmdline, list):
        cmdline = str(cmdline).split()
    cmd = ["git"] + cmdline
    print(' '.join(cmd))
    try:
        subprocess.check_call(cmd, cwd=sdk_root_dir)
    except:
        print("[ERROR] GIT process exits with non-zero exit code.")
        sys.exit(1)

def update_submodules():
    submodules = [
        # list all submodules here to automatically fetch them as part of the build process.
        "dev/3rd-party/glfw",
    ]
    for s in submodules:
        dir = sdk_root_dir / s
        if not dir.is_dir():
            utils.rip(f"{dir} not found. your working directory might be corrupted. Please consider re-cloning.")
        items = dir.iterdir()
        if len(list(items)) == 0 :
            git("submodule update --init")
            break

def get_android_path(name):
    if os.environ.get(name) is None: utils.rip(f"{name} environment variable not found.")
    p = pathlib.Path(os.getenv(name))
    if not p.is_dir(): utils.rip(f"{p} folder not found.")
    return p

def cmake_config(args, build_dir, build_type):
    update_submodules()
    os.makedirs(build_dir, exist_ok=True)
    config = f"-S {sdk_root_dir} -B {build_dir} -DCMAKE_BUILD_TYPE={build_type}"
    if args.android_build:
        # Support only arm64 for now
        sdk = get_android_path('ANDROID_SDK_ROOT')
        ndk = get_android_path('ANDROID_NDK_HOME')
        utils.logi(f"Using Android SDK: {sdk}")
        utils.logi(f"Using Android NDK: {ndk}")
        if 'nt' == os.name:
            ninja = sdk / "cmake/3.22.1/bin/ninja.exe"
            if not ninja.exists(): utils.rip(f"{ninja} not found. Please install cmake 3.18+ via Android SDK Manager." )
        else:
            ninja = "ninja"
        toolchain = ndk / "build/cmake/android.toolchain.cmake"
        config += f" \
            -GNinja \
            -DCMAKE_MAKE_PROGRAM={ninja} \
            -DCMAKE_SYSTEM_NAME=Android \
            -DANDROID_NDK={ndk} \
            -DCMAKE_ANDROID_NDK={ndk} \
            -DCMAKE_TOOLCHAIN_FILE={toolchain} \
            -DANDROID_NATIVE_API_LEVEL=29 \
            -DCMAKE_SYSTEM_VERSION=29 \
            -DANDROID_PLATFORM=android-29 \
            -DANDROID_ABI={android_abi} \
            -DCMAKE_ANDROID_ARCH_ABI={android_abi} \
            "
    elif 'nt' != os.name:
        config += " -DCMAKE_C_COMPILER=clang-14 -DCMAKE_CXX_COMPILER=clang++-14"
        if not args.use_makefile: config += " -GNinja"
    cmake(build_dir, config)

# ==========
# main
# ==========

# parse command line arguments
ap = argparse.ArgumentParser()
ap.add_argument("-a", dest="android_build", action="store_true", help="Build for Android")
ap.add_argument("-b", dest="build_dir", default="build", help="Build output folder.")
ap.add_argument("-c", dest="config_only", action="store_true", help="Run CMake config only. Skip cmake build.")
ap.add_argument("-C", dest="skip_config", action="store_true", help="Skip CMake config. Run build process only.")
ap.add_argument("-m", dest="use_makefile", action="store_true", help="Use OS's default makefile instead of Ninja")
ap.add_argument("variant", help="Specify build variant. Acceptable values are: d(ebug)/p(rofile)/r(elease)/c(lean). "
                                         "Note that all parameters alert this one will be considered \"extra\" and passed to CMake directly.")
ap.add_argument("extra", nargs=argparse.REMAINDER, help="Extra arguments passing to cmake.")
args = ap.parse_args()
#print(args.extra)

# get the root directory of the code base
sdk_root_dir = utils.get_root_folder()
# print(f"Repository root folder = {sdk_root_dir}")

android_abi = "arm64-v8a" if args.android_build else None

# get cmake build variant and build folder
build_type, build_dir = utils.get_cmake_build_type(args.variant, args.build_dir, android_abi)

if build_type is None:
    if os.name == "nt":
        os.system('taskkill /f /im java.exe 2>nul')
    else:
        # TODO: kill java process the Linux way.
        pass
    folders = glob.glob(str(sdk_root_dir / "build*"))
    for x in folders:
        print(f"rm {x}")
        shutil.rmtree(x)
else:
    if not args.skip_config:
        cmake_config(args, build_dir, build_type)
    if not args.config_only:
        jobs = ["-j8"] # limit to 8 cores.
        cmake(build_dir, ["--build", "."] + jobs + ["--config", build_type] + args.extra)
        # if args.install_destination_folder:
        #     inst_dir = str(pathlib.Path(args.install_destination_folder).absolute())
        #     cmake(build_dir, ["--install", ".", "--config", build_type, "--prefix", inst_dir] + args.extra)
