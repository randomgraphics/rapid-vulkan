#/bin/bash
set -e # exit on error

echo "This script is to install native build dependencies on Ubuntu 20.04/22.04 LTS for non-docker based build."
echo
echo "Docker support is on its way."
echo
echo "If you wish to continue, press ENTER. Or else, press CTRL-C to quit."
read

# add vulkan sdk source
# sudo apt-get update
# sudo apt-get install -y wget gnupg2

# check OS distribution
release=$(lsb_release -rs)
if [ $(echo "$release >= 22" | bc) -eq 1 ]; then
    dist="ubuntu_22_04"
elif [ $(echo "$release >= 20" | bc) -eq 1 ]; then
    dist="ubuntu_20_04"
else
    echo "[ERROR] Unrecognized OS version ..."
    exit -1
fi
echo dist=$dist

# setup vulkan-sdk source
if [ "ubuntu_22_04" == $dist ]; then
    # For Ubuntu 22.04+
    echo "Add LunarG SDK source to apt registry for Ubuntu 22.04 ..."
    wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.239-jammy.list https://packages.lunarg.com/vulkan/1.3.239/lunarg-vulkan-1.3.239-jammy.list
elif [ "ubuntu_20_04" == $dist ]; then
    # For Ubuntu 20.04
    echo "Add LunarG SDK source to apt registry for Ubuntu 20.04 ..."
    wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.239-focal.list https://packages.lunarg.com/vulkan/1.3.239/lunarg-vulkan-1.3.239-focal.list
else
    echo "[ERROR] Unrecognized OS version ..."
    exit -1
fi

# Add clang-14 to apt repository for Ubuntu 20.04.
if [ "ubuntu_20_04" == $dist ]; then
    echo "Add clang 14 suite to apt registry for Ubuntu 20.04 ..."
    wget -P /tmp https://apt.llvm.org/llvm.sh
    chmod +x /tmp/llvm.sh
    sudo /tmp/llvm.sh 14
    rm /tmp/llvm.sh
fi

# install build dependencies 
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    git-lfs \
    ninja-build \
    vulkan-sdk \
    libglfw3-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libdw-dev \
    libunwind-dev \
    libdwarf-dev \
    binutils-dev \
    clang-14 \
    clang-format-14

# install python modules
python3 -m pip install --upgrade pip
python3 -m pip install --upgrade termcolor