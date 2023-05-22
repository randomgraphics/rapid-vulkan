#!/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
tag=`cat $dir/tag.txt`
echo "Building Vulkan Base Image. Requires docker engine version 20.10.9 or newer."
docker -v
docker build --pull -t $tag -f ${dir}/Dockerfile ${dir}