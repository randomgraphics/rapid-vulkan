ARG base
FROM ${base}
LABEL description="rapid-vulkan"
ARG DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    git-lfs \
    openssh-client \
    ninja-build \
    vim \
    python3 \
    python3-pip \
    binutils-dev \
    clang-14 \
    clang-format-14 \
    libx11-xcb-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libxrandr-dev \
    libegl1-mesa-dev \
    libglfw3-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libdw-dev \
    libunwind-dev \
    libdwarf-dev \
    && rm -rf /var/lib/apt/lists/*

# Install pyhton packages
RUN pip3 install termcolor
