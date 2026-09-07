param([string]$CacheFile = 'C:\docker-image-cache\garnet-windows.tar')
$ErrorActionPreference = 'Stop'
$image = Get-Content "$PSScriptRoot/image-lock.json" -Raw | ConvertFrom-Json
$timer = [Diagnostics.Stopwatch]::StartNew()
if (Test-Path -LiteralPath $CacheFile) {
    Write-Host 'GARNET_IMAGE_CACHE=hit'
    docker image load --input $CacheFile
    if ($LASTEXITCODE -ne 0) { throw 'Cached image load failed; invalidate the tarball cache key.' }
    Write-Host "Image load seconds: $($timer.Elapsed.TotalSeconds)"
} else {
    Write-Host 'GARNET_IMAGE_CACHE=miss'
    docker image pull $image.reference
    if ($LASTEXITCODE -ne 0) { throw 'Published image pull failed' }
    Write-Host "Image pull seconds: $($timer.Elapsed.TotalSeconds)"
}
# docker save/load does not retain registry digests; verify the immutable image ID instead.
$actualId = docker image inspect --format '{{.Id}}' $image.imageId
if ($LASTEXITCODE -ne 0 -or $actualId -ne $image.imageId) { throw 'Expected image is missing from the Docker engine' }
if (-not (Test-Path -LiteralPath $CacheFile)) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $CacheFile) | Out-Null
    $timer.Restart()
    # Publish the archive only after a complete export, so a partial file cannot be cached.
    docker image save --output "$CacheFile.tmp" $image.imageId
    if ($LASTEXITCODE -ne 0) { throw 'Image archive export failed' }
    Move-Item -LiteralPath "$CacheFile.tmp" -Destination $CacheFile
    Write-Host "Image save seconds: $($timer.Elapsed.TotalSeconds)"
}
Write-Host "Verified image ID: $actualId"
Write-Host "Archive bytes: $((Get-Item -LiteralPath $CacheFile).Length)"
