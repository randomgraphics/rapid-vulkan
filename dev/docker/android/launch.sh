#!/bin/bash
set -e
dir="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
sdkroot="$(cd $(dirname "${BASH_SOURCE[0]}")/../../..;pwd)"

# parse command line arguments
append_to_env_params=0
run_as="-u $UID:$GID"
gpu="--gpus all"
entry_point="entrypoint.sh"
interactive="-it"
docker_login="registry.gitlab.com"
image=`cat $dir/tag.txt`
usb=
for param in "$@"
do
  case "$param" in
    -r | --root)
        # run as root user
        echo "Enter the container as root user."
        run_as=
        ;;
    --ng)
        # disable GPU support
        echo "Disable GPU support"
        gpu=
        ;;
    --usb)
        echo
        echo "Enable USB access inside docker. Note that this will disable ADB access to USB devices outside of the docker."
        if [ $(type -P adb) ]; then
          echo "Kill any existing ADB server...."
          adb kill-server 2>/dev/null
        else
          echo
          echo "[WARNING] adb command not found. Please run `adb kill-server` to kill any exisitng ADB server before running this script."
          adb kill-server
        fi
        usb="--privileged -v /dev/bus/usb:/dev/bus/usb"
        ;;
    # --linux-ci)
    #     # run docker for Linux CI
    #     echo "Run docker for CI"
    #     entry_point="../ci/docker-entrypoint-linux-ci.sh"
    #     interactive=
    #     ;;
    # --lint)
    #     # run dokcer for code lint
    #     echo "Run docker for code lint"
    #     entry_point="../ci/docker-entrypoint-lint.sh"
    #     interactive=
    #     gpu=
    #     ;;
    --)
        # any parameters after this ara forward to entry point script.
        append_to_env_params=1
        ;;
    *)
        if [[ ${append_to_env_params} == 1 ]]; then
            env_params="${env_params} $param"
        else
            # unrecognized parameter
            echo "[ERROR] Unrecognized parameter: ${param}"
            exit -1
        fi
  esac
  shift
done

# If GPU is enabled, then try randomizing the GPU selection to evenly distribute workload to the system.
if [ -n "${gpu}" ]; then
    # retrieve number of GPUs, assuming NVIDIA card.
    N=$(nvidia-smi -L|wc -l)
    S=$RANDOM
    set +e
    let "S %=${N}"
    set -e
    gpu="--gpus device=${S}"
fi

echo
echo "Launching PhysRay Docker environment..."
echo "  HOST        = $(hostname)"
echo "  USER        = $(whoami)"
echo "  HOME        = ${HOME}"
echo "  HTTP_PROXY  = ${HTTP_PROXY}"
echo "  HTTPS_PROXY = ${HTTPS_PROXY}"
echo "  GPU         = ${gpu}"
echo "  env_params  = ${env_params}"
echo

# Check if the docker image exists. If not, build it.
if [[ "$(docker images -q $image 2> /dev/null)" == "" ]]; then
    echo "Docker image ${image} not found. Lets' build it."
    $dir/build.sh
else
    echo "Docker image found: ${image}"
fi

echo
echo "----"
echo "Note that this docker requires nvidia container. If you see error message"
echo "like: \"could not select device driver...\", it is most likely due to"
echo "incorrect nvida container configuration. Re-run install-docker-engine.sh"
echo "to refresh your nvidia-container installation should fix it. Or, you can"
echo "also run this script with \"--ng\" argument to disable GPU support. With"
echo "this option, you can still build everything as normal. Just can't run any"
echo "of the GPU based programs within the docker."
echo "----"
echo

# Launch the docker
docker run --rm \
    -e HTTP_PROXY=${HTTP_PROXY} \
    -e HTTPS_PROXY=${HTTPS_PROXY} \
    -e HOST_USER_HOME=${HOME} \
    -v "/etc/group:/etc/group:ro" \
    -v "/etc/passwd:/etc/passwd:ro" \
    -v "/etc/shadow:/etc/shadow:ro" \
    -v "${HOME}:${HOME}" \
	-v "${sdkroot}:${sdkroot}" \
	-w "${sdkroot}" \
    ${usb} \
    ${gpu} \
    ${run_as} \
    ${interactive} \
    $image ${dir}/${entry_point} ${env_params}
