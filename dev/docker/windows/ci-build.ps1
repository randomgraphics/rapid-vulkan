param([string]$Source = 'C:\rapid-vulkan')
$ErrorActionPreference = 'Stop'
Set-Location $Source
& git.exe config --global --add safe.directory ($Source -replace '\\', '/')
if ($LASTEXITCODE -ne 0) { throw 'Git safe.directory setup failed' }
. .\dev\env\env.ps1
# The image supplies the compiler, Vulkan SDK and Python packages; never install on the host.
& python.exe -c 'import termcolor'
if ($LASTEXITCODE -ne 0) { throw 'Image is missing the build script dependencies' }
# Isolated output prevents a host build from satisfying this check accidentally.
& python.exe dev\bin\build.py -b C:\rapid-vulkan-build d
if ($LASTEXITCODE -ne 0) { throw 'Windows debug build failed' }
