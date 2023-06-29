# escape=`

# Use Windows Image with full API support
FROM mcr.microsoft.com/windows:ltsc2019-amd64

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

# Install Visual Studio 2022 Build tools
RUN curl.exe -SL -o vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe
RUN start /w vs_buildtools.exe --quiet --wait --norestart --nocache `
    --installPath "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" `
    --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
RUN del /q vs_buildtools.exe

# Install Git
RUN curl.exe -SL -o git-install.exe https://github.com/git-for-windows/git/releases/download/v2.41.0.windows.1/Git-2.41.0-64-bit.exe
RUN start /w git-install.exe /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS="icons,ext\reg\shellhere,assoc,assoc_sh"
RUN del /q git-install.exe

# Install CMake
RUN curl.exe -SL -o cmake.msi https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4-windows-x86_64.msi
RUN start /w msiexec.exe /i cmake.msi /quiet /norestart
RUN del /q cmake.msi
RUN setx /M PATH "%PATH%;C:\Program Files\CMake\bin"

# Install Python
RUN curl.exe -SL -o python-installer.exe https://www.python.org/ftp/python/3.11.2/python-3.11.2-amd64.exe
RUN python-installer.exe /quiet InstallAllUsers=1 CompileAll=1 AppendPath=1 Include_test=0
RUN del /q python-installer.exe
RUN python.exe -m pip install --upgrade pip
RUN python.exe -m pip install --upgrade termcolor

# Install Vulkan SDK
RUN curl.exe -SL -o vulkan-sdk.exe https://sdk.lunarg.com/sdk/download/1.3.250.0/windows/VulkanSDK-1.3.250.0-Installer.exe
RUN vulkan-sdk.exe --root=c:\vulkan-sdk --accept-licenses --default-answer --confirm-command install com.lunarg.vulkan com.lunarg.vulkan.volk com.lunarg.vulkan.vma
RUN del /q vulkan-sdk.exe

# Misc.
RUN powershell -Command Set-ExecutionPolicy RemoteSigned