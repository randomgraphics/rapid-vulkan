param([string]$Image = (Get-Content "$PSScriptRoot/tag.txt" -Raw).Trim())
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
foreach ($variant in @('d', 'p', 'r')) {
    docker run --rm --isolation process `
        --mount "type=bind,source=$repo,target=C:\rapid-vulkan" `
        --workdir C:\rapid-vulkan $Image `
        powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\rapid-vulkan\dev\docker\windows\ci-build.ps1 -Variant $variant
    if ($LASTEXITCODE -ne 0) { throw "Windows container $variant build failed" }
}
