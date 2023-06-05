# get current folder of this script
$script_path = Split-Path -parent $MyInvocation.MyCommand.Definition

# read tag.txt file
$tag = Get-Content "$script_path\tag.txt"
$tag = $tag -replace '\s', '' # remove space and new line of $tag
"tag = $tag"

# run docker build command
echo "docker build -t $tag ""$script_path"""
docker build -t $tag "$script_path"
