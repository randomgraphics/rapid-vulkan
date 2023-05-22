#!/usr/bin/python3
import sys, subprocess, os
import importlib; utils = importlib.import_module("rapid-vulkan-utils")

def run_style_check():
    # uncomment to print clang-format version
    # subprocess.run(["clang-format-14", "--version"])

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
        utils.loge("The following changes are violating coding style standard:")
        print(format_diff)
        sys.exit(-1)

    # Done
    print("Style check done. All good!")

run_style_check()
utils.run_the_latest_binary("dev/test/{variant}/rapid-vulkan-test", sys.argv[1:])
