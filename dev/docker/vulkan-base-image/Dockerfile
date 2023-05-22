ARG BASE_DIST
ARG CUDA_VERSION
ARG DEBIAN_FRONTEND=noninteractive
FROM nvidia/cuda:12.1.1-base-ubuntu22.04
LABEL description="randomgraphics/vulkan"

# Download and install wget and GL dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    libglvnd0 \
    libgl1 \
    libglx0 \
    libegl1  \
    libgles2  \
    libxcb1-dev

# Install Vulkan SDK
RUN wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | tee /etc/apt/trusted.gpg.d/lunarg.asc
RUN wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.243-jammy.list https://packages.lunarg.com/vulkan/1.3.243/lunarg-vulkan-1.3.243-jammy.list
RUN apt-get update && apt-get install -y vulkan-sdk && rm -rf /var/lib/apt/lists/*


# Setup the required capabilities for the container runtime    
ENV NVIDIA_DRIVER_CAPABILITIES compute,graphics,utility

# Licensed under the NGC Deep Learning Container License
COPY NGC-DL-CONTAINER-LICENSE.txt /