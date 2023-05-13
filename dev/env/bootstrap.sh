#/bin/bash

echo "This script is to install native build dependencies on Ubuntu 20.04/22.04 LTS for non-docker based build."
echo
echo "Docker support is on its way."
echo
echo "If you wish to continue, press ENTER. Or else, press CTRL-C to quit."
read

# add vulkan sdk source
sudo apt-get update
sudo apt-get install -y wget gnupg2

echo "Add LunarG SDK to apt registry..."
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.239-focal.list https://packages.lunarg.com/vulkan/1.3.239/lunarg-vulkan-1.3.239-focal.list

# install build dependencies 
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    git-lfs \
    clang-format-14 \
    ninja-build \
    vulkan-sdk \
    libvulkan-memory-allocator-dev \
    libglfw3-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libdw-dev \
    libunwind-dev \
    libdwarf-dev \
    binutils-dev

# install python modules
python3 -m pip install --upgrade pip
python3 -m pip install --upgrade termcolor