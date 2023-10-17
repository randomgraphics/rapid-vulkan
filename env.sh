#!/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
exec bash -rcfile ${dir}/dev/env/env.rc
