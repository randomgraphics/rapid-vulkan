#!/usr/bin/python3
import sys
import utils
if len(sys.argv) < 2:
    print("Usage: run-sample.py <sample-name> [args]")
    sys.exit(-1)
utils.run_the_latest_binary("dev/sample/{variant}/" + sys.argv[1], sys.argv[2:])
