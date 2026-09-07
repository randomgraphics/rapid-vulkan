param([switch]$Gpu)
$ErrorActionPreference = 'Stop'
$tag = (Get-Content "$PSScriptRoot/tag.txt" -Raw).Trim()
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
$options = @('run', '--rm', '-it', '--isolation', 'process', '--mount', "type=bind,source=$repo,target=C:\rapid-vulkan", '--workdir', 'C:\rapid-vulkan')
if ($Gpu) { $options += @('--device', 'class/5B45201D-F2F2-4F3B-85BB-30FF1F953599') }
docker @options $tag powershell.exe -NoLogo -NoExit -ExecutionPolicy Bypass -File C:\rapid-vulkan\dev\env\env.ps1
if ($LASTEXITCODE -ne 0) { throw 'Windows container shell failed' }
