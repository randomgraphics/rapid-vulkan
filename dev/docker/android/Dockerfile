ARG base
FROM ${base}
LABEL description="rapid-vulkan:android"
ARG DEBIAN_FRONTEND=noninteractive

# Download and install Android Studio
RUN mkdir /android/
RUN wget -q -c -O /android/studio.tar.gz https://redirector.gvt1.com/edgedl/android/studio/ide-zips/2022.2.1.19/android-studio-2022.2.1.19-linux.tar.gz
RUN cd /android;tar xzf studio.tar.gz;rm studio.tar.gz;mv android-studio studio

# Download and install command line tools
RUN apt-get update;apt-get install unzip
RUN wget -q -c -O /android/commandline-tools.zip https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
RUN cd /android;unzip commandline-tools.zip;mkdir -p sdk/cmdline-tools;mv cmdline-tools sdk/cmdline-tools/latest;rm commandline-tools.zip

# Download Android SDK components
ARG JAVA_HOME=/android/studio/jbr
RUN yes|/android/sdk/cmdline-tools/latest/bin/sdkmanager --licenses
RUN /android/sdk/cmdline-tools/latest/bin/sdkmanager --install "ndk;25.2.9519653"
RUN /android/sdk/cmdline-tools/latest/bin/sdkmanager --install "build-tools;33.0.2"
RUN /android/sdk/cmdline-tools/latest/bin/sdkmanager --install "platform-tools"
RUN /android/sdk/cmdline-tools/latest/bin/sdkmanager --install "cmake;3.22.1"

# Setup environment variables
ENV JAVA_HOME=/android/studio/jbr
ENV ANDROID_SDK_ROOT=/android/sdk
ENV ANDROID_NDK_HOME=/android/sdk/ndk/25.2.9519653
ENV PATH=${PATH}:/android/sdk/platform-tools:/android/sdk/cmdline-tools/latest/bin

# Mark the /android folder readable to everyone
RUN chmod a+rx /android
