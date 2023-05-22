#/bin/bash
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
base=`cat $dir/vulkan-base-image/tag.txt`
image=`cat $dir/tag.txt`

echo
echo Build rapid-vulkan docker image...
echo  HTTP_PROXY  = ${HTTP_PROXY}
echo  HTTPS_PROXY = ${HTTPS_PROXY}
echo  http_proxy  = ${http_proxy}
echo  https_proxy = ${https_proxy}
echo

effective_http_proxy=${http_proxy}
if [ -z "${effective_http_proxy}" ]; then effective_http_proxy=${HTTP_PROXY}; fi

effective_https_proxy=${https_proxy}
if [ -z "${effective_https_proxy}" ]; then effective_https_proxy=${HTTPS_PROXY}; fi

# build the final docker image
DOCKER_BUILDKIT=1 docker build \
    --build-arg http_proxy=${effective_http_proxy} \
    --build-arg https_proxy=${effective_https_proxy} \
    --build-arg base=${base} \
    -t $image ${dir}
