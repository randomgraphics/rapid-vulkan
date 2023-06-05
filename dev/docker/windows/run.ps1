# read tag.txt file
$tag = Get-Content "$PSScriptRoot\tag.txt"
$tag = $tag -replace '\s', '' # remove space and new line of $tag
"tag = $tag"

# run docker
"docker run --rm -it $tag"
docker run --rm -it $tag