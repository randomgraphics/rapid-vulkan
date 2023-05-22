#!/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
image=`cat $dir/tag.txt`
docker login
docker push $image
