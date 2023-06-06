# read tag.txt file
$tag = Get-Content "$PSScriptRoot\tag.txt"
$tag = $tag -replace '\s', '' # remove space and new line of $tag
"tag = $tag"

# get the root directory of the repository
$repo = Split-Path -parent $PSScriptRoot | Split-Path -parent | Split-Path -parent 

# pull the image
"Pull image $tag from docker hub"
docker pull $tag

# run docker
"Launch $tag container to run CI test script"
$command = "docker run --rm -v ${repo}:C:/rapid-vulkan -w c:/rapid-vulkan $tag powershell.exe -Command ""& {
    . dev\env\env.ps1
    b -b build.ci d
    b -b build.ci p
    b -b build.ci r
}"""
Invoke-Expression $command