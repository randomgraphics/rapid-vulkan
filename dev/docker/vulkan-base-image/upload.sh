#!/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
tag=`cat $dir/tag.txt`
docker login
docker push $tag