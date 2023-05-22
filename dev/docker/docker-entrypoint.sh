#!/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
#export JAVA_HOME=/opt/android/studio/jre
#export ANDROID_SDK_ROOT=/opt/android/sdk
#PATH=$PATH:/opt/android/sdk/platform-tools/
cd ${dir}/../..
./env.sh $@
