#!/usr/bin/python3
import sys, subprocess, os, re, argparse
import importlib; utils = importlib.import_module("rapid-vulkan-utils")

def get_header_revision(content):
    m = re.search(r"#define RAPID_VULKAN_HEADER_REVISION (\d+)", content)
    return int(m.group(1)) if m else 0

def check_header_revision():
    print("Checking public header revision...", end="")
    sdk_root_dir = utils.get_root_folder()
    git_remote = subprocess.check_output(["git", "remote"], cwd=sdk_root_dir).decode(sys.stdout.encoding).strip()
    header_path = "inc/rapid-vulkan/rapid-vulkan.h"
    header_diff = subprocess.check_output(["git", "diff", "-U0", "--no-color", git_remote + "/main", "--", header_path], cwd=sdk_root_dir).decode(sys.stdout.encoding).strip()
    if 0 == len(header_diff):
        print("OK.")
        return

    # get header revision of the local file
    with open(sdk_root_dir / header_path, "r") as f: local_revision = get_header_revision(f.read())
    # print(f"Local header revision: {local_revision}")

    # get header revision of the remote file
    remote_header = subprocess.check_output(["git", "show", git_remote + "/main:" + str(header_path)], cwd=sdk_root_dir).decode(sys.stdout.encoding).strip()
    remote_revision = get_header_revision(remote_header)
    # print(f"Remote header revision: {remote_revision}")

    # check if the header revision is increased
    if local_revision <= remote_revision:
        utils.rip("The header revision is not increased. Please increase the header revision.")
    else:
        print("OK")

def run_style_check():
    print("Checking code styles...", end="")

    # get changes from git.
    sdk_root_dir = utils.get_root_folder()
    git_remote = subprocess.check_output(["git", "remote"], cwd=sdk_root_dir).decode(sys.stdout.encoding).strip()
    diff = subprocess.check_output(["git", "diff", "-U0", "--no-color", git_remote + "/main", "--", ":!*3rd-party*"], cwd=sdk_root_dir)

    # determine the clang-format-diff command line
    if "nt" == os.name:
        diff_script = sdk_root_dir / "dev/bin/clang-format-diff-win.py"
        clang_format = sdk_root_dir / "dev/bin/clang-format-14.exe"
        cmdline = ["python.exe", str(diff_script.absolute()), "-p1", "-binary", str(clang_format.absolute())]
    else:
        cmdline = ["clang-format-diff-14", "-p1"]

    # check coding style of the diff
    format_diff = subprocess.check_output(cmdline, input=diff, cwd=sdk_root_dir).decode("utf-8")
    if len(format_diff) > 0:
        utils.rip(f"The following changes are violating coding style standard:\n{format_diff}")

    # Done
    print("OK.")

# main
ap = argparse.ArgumentParser()
ap.add_argument("-l", action="store_true", help="Run code lint only. Skip test.")
ap.add_argument("test_args", nargs="*")
args = ap.parse_args()
check_header_revision()
run_style_check()
if not args.l: utils.run_the_latest_binary("dev/test/{variant}/rapid-vulkan-test", args.test_args)
