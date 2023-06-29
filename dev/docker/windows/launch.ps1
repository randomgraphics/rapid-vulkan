# read tag.txt file
$tag = Get-Content "$PSScriptRoot\tag.txt"
$tag = $tag -replace '\s', '' # remove space and new line of $tag
"tag = $tag"

# get the root directory of the repository
$repo = Split-Path -parent $PSScriptRoot | Split-Path -parent | Split-Path -parent 

# launch docker
docker run --rm -it -v ${repo}:C:/rapid-vulkan -w c:/rapid-vulkan $tag env.cmd