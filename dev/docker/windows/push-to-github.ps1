# read tag.txt file
$tag = Get-Content "$PSScriptRoot\tag.txt"
$tag = $tag -replace '\s', '' # remove space and new line of $tag
"tag = $tag"

docker push $tag