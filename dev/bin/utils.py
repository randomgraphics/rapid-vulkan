#!/usr/bin/python3

import os
from re import search
import sys, subprocess, pathlib, pprint, termcolor

class FatalError (RuntimeError):
    def __init__(self, message):
        super(FatalError, self).__init__(message)

def rip(message, exit_code = -1):
    print("\n\n[  FATAL] {0}\n\n".format(message))
    sys.exit(exit_code)

def loge(message):
    print(termcolor.colored("[  ERROR] {0}".format(message), "red"))

def logw(message):
    print(termcolor.colored("[WARNING] {0}".format(message), "yellow"))

def logi(message):
    print("[   INFO] {0}".format(message))

def kill_android_proc(procname, adb = ['adb']):
    try:
        proc = subprocess.check_output(adb + ["shell", f"ps -A|grep {procname}"]).decode("utf-8").splitlines()
        for p in proc:
            items = p.split()
            if len(items) > 1:
                pid = items[1]
                logi(f"kill remote process {pid}")
                subprocess.run(adb + ["shell", f"kill -9 {pid}"])
    except subprocess.CalledProcessError:
        # Ignore subprocess.CalledProcessError exception. It happens when the process in question is not running.
        pass

def run_cmd(args, capture_output = False, check=True, verbose=False):
    if verbose: print(" ".join(args))
    r = subprocess.run(args, check=check, capture_output=capture_output)
    if capture_output:
        return r.stdout.decode("utf-8").strip()
    else:
        return r.returncode

class AndroidDebugBridge:
    cmd = None
    verbose = False

    def __init__(self, device_serial_number = None):
        self.cmd = ["adb"]
        if "usb" == device_serial_number:
            self.cmd += ["-d"]
        elif not device_serial_number is None:
            self.cmd += ["-s", device_serial_number]
        # verify the selected device is connected
        logi("Waiting for device to be connected...")
        subprocess.check_output(self.cmd + ["wait-for-device"])
        logi("Device connected.")

    def run(self, args, capture_output = False, check = True, verbose = False):
        if isinstance(args, str): args = args.split()
        if check is None: check = self.check
        if verbose is None: verbose = self.verbose
        return run_cmd(self.cmd + args, capture_output, check, verbose or self.verbose)

    def capture_output(self, args, check = True, verbose = False):
        return self.run(args, capture_output = True, check = check, verbose = verbose)

    def shell(self, command, capture_output = False, check = True, verbose = False):
        return self.run(["shell", command], capture_output, check, verbose)

def compare_file_timestamp(path, latest, chosen):
    if not path.exists(): return latest, chosen
    timestamp = path.stat().st_mtime
    # print(f"{path} : {timestamp}, {timestamp} < {latest} = {timestamp < latest}")
    if timestamp < latest: return latest, chosen
    return timestamp, path

def get_root_folder():
    return pathlib.Path(__file__).resolve().parent.parent.parent.absolute()

def get_cmake_build_type(variant, build_dir, for_android = False):
    # determine build type
    build_type = str(variant).lower()
    if "d" == build_type or "debug" == build_type:
        suffix = ".d"
        build_type = "Debug"
    elif "p" == build_type or "profile" == build_type:
        suffix = ".p"
        build_type = "RelWithDebInfo"
    elif "r" == build_type or "release" == build_type:
        suffix = ".r"
        build_type = "Release"
    elif "c" == build_type or "clean" == build_type:
        # return [None, None] indicating a clear action.
        return [None, None]
    else:
        rip(f"[ERROR] unrecognized build variant : {variant}.")

    # determine build folder
    build_dir = pathlib.Path(build_dir)
    if not build_dir.is_absolute():
        build_dir = get_root_folder() / build_dir
    if for_android:
        build_dir = build_dir / ("android" + suffix)
    elif os.name == "nt":
        build_dir = build_dir / ("mswin" + suffix)
    else:
        build_dir = build_dir / ("posix" + suffix)

    #done
    return [build_type, build_dir]

# def search_for_the_latest_binary_ex(path_template):
#     candidates = [
#         path_template.format(variant = ""),
#         path_template.format(variant = "Debug"),
#         path_template.format(variant = "RelWithDebInfo"),
#         path_template.format(variant = "Release"),
#     ]
#     # Loop through all candidates
#     latest = 0
#     chosen = None
#     sdk_root_dir = get_sdk_root_folder()
#     searched = []
#     for c in candidates:
#         # print(f"latest : {latest}")
#         p = pathlib.Path(c)
#         if not p.is_absolute(): p = sdk_root_dir / p
#         searched.append(p)
#         searched.append(p.with_suffix(".exe"))
#         latest, chosen = compare_file_timestamp(p, latest, chosen)
#         latest, chosen = compare_file_timestamp(p.with_suffix(".exe"), latest, chosen)
#     return chosen, searched

# def search_for_the_latest_binary(path_template):
#     chosen, searched = search_for_the_latest_binary_ex(path_template)
#     if chosen is None:
#         pp = pprint.PrettyPrinter(indent=4)
#         print(f"[ERROR] binary _NOT_ found: {path_template}. The following locations are searched:\n{pp.pformat(searched)}")
#     return chosen

# def run_the_latest_binary(path_template, argv, check = True):
#     chosen, searched = search_for_the_latest_binary_ex(path_template)
#     if chosen is None:
#         pp = pprint.PrettyPrinter(indent=4)
#         print(f"[ERROR] binary _NOT_ found: {path_template}. The following locations are searched:\n{pp.pformat(searched)}")
#         sys.exit(1)

#     # Invoke the binary
#     cmdline = [str(chosen)] + argv
#     print(' '.join(cmdline))
#     return subprocess.run(cmdline, check=check)
