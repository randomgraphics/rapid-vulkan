"********************************************************************************"
"*                                                                              *"
"*                    Rapid Vulkan Development Console                          *"
"*                                                                              *"
"********************************************************************************"

# ==============================================================================
# Define local functions
# ==============================================================================

function warn { write-host -ForegroundColor yellow "WARN : $args" }

function error {
    write-host -ForegroundColor red "Detective-Conan build environment setup failed : $args"
    stop-process $PID # kill current power shell process
}

function catch_batch_env( $batch, $arg ) {
    $tag = "[[[===start-environment-table===]]]" # cannot have space here.
    $cmd = "`"$batch`" $arg &echo $tag& set"
    $tag_found = $false
    cmd.exe /c $cmd | foreach-object {
        if ( $tag_found ) {
            $p, $v = $_.split('=')
            Set-Item -path env:$p -value $v
        }
        elseif ( $_ -eq $tag ) {
            $tag_found = $true
        }
        else {
            $_
        }
    }
}

# ==============================================================================
# Get the root directory
# ==============================================================================

# note: $RAPID_VULKAN_ROOT is a global variable that could be used in other places outside of this script.
$global:RAPID_VULKAN_ROOT = split-path -parent $PSScriptRoot | split-path -parent
$env:RAPID_VULKAN_ROOT = $RAPID_VULKAN_ROOT

# ==============================================================================
# Detect container environment
# ==============================================================================
if ( $env:USERNAME -eq "ContainerAdministrator" ) {
    "We are in a Windows container. Config git to ignore ownership differences."
    $git_friendly_path = $RAPID_VULKAN_ROOT -replace '\\', '/'
    git config --global --add safe.directory $git_friendly_path/

    # TODO: setup alternative build folder to avoid interferences with host build folder.
}

# ==============================================================================
# Setup Vulkan SDK
# ==============================================================================

if (-not $(test-path env:VULKAN_SDK)) {
    write-host -ForegroundColor red -NoNewline "Environment variable VULKAN_SDK is not set. Please download and install the latest Vulkan SDK."
}
elseif (-not $(test-path $env:VULKAN_SDK)) {
    fatal "Environment variable VULKAN_SDK is not pointing to valid folder."
}

# ==============================================================================
# Check Python support
# ==============================================================================

$py = [System.Version]$(python.exe -V).Substring(7)
if ([System.Version]"3.6.0" -gt $py) {
    warn "Python is not installed or current version ($py) is too low. Please upgrade to 3.8.0+ for best script compatibility."
}

# ==============================================================================
# Define global functions
# ==============================================================================

# Define your function like this: function global:<name> (...) { .... }
function global:ccc { cmd.exe /c $args }

# A helper function to retrieve current git branch
function global:get-git-branch {
    $branch = $(git rev-parse --abbrev-ref HEAD 2>&1)
    if ($lastExitCode -ne 0) {
        $branch = "!!!GIT ERROR: {$lastExitCode}!!!"
    }
    "$branch"
}

# redefine prompt function
function global:prompt {
    write-host -ForegroundColor Green "==== rapid-vulkan - " -NoNewline
    write-host -ForegroundColor Blue "$RAPID_VULKAN_ROOT" -NoNewline
    write-host -ForegroundColor Green " - " -NoNewline
    write-host -ForegroundColor Yellow "$(get-git-branch)" -NoNewline
    write-host -ForegroundColor Green " ===="
    write-host -ForegroundColor Green "[$env:USERNAME] [$(get-location)]"
    return ">"
}

# ==============================================================================
# setup aliases
# ==============================================================================
if ( Test-Path -path "$RAPID_VULKAN_ROOT\dev\env\alias.powershell.txt" ) {
    # create script block for all aliases
    $aliases = ""
    get-content "$RAPID_VULKAN_ROOT\dev\env\alias.powershell.txt" | foreach {
        $name, $value = $_.split(' ')
        $body = ([System.String]$value).Trim( ' "' )
        $aliases = $aliases +
        "
        function global:$name {$body}
        "
    }
    $aliases = $executioncontext.InvokeCommand.NewScriptBlock( $aliases )

    # run the script
    &$aliases
}
else {
    warn "$RAPID_VULKAN_ROOT\dev\env\alias.txt is missing."
}

# ==============================================================================
# setup git
# ==============================================================================
if ( Test-Path -path "${RAPID_VULKAN_ROOT}\.gitconfig" ) {
    git config --local include.path ${RAPID_VULKAN_ROOT}\.gitconfig
}

# ==============================================================================
# MISC
# ==============================================================================

# Setup PATH
$env:Path = "$RAPID_VULKAN_ROOT\dev\bin;$env:Path"

# update title
$Host.UI.RawUI.WindowTitle = "DC3d ( $RAPID_VULKAN_ROOT )"

# change current location
set-location $RAPID_VULKAN_ROOT

#reset some command line color
Set-PSReadlineOption -Colors @{
    "Parameter" = [System.ConsoleColor]::White
    "Operator"  = [System.ConsoleColor]::White
}

# ==============================================================================
# DONE
# ==============================================================================

write-host -ForegroundColor green "
Rapid Vulkan build environment ready to use. Happy coding!

RAPID_VULKAN_ROOT = $env:RAPID_VULKAN_ROOT
VULKAN_SDK        = $env:VULKAN_SDK
USERNAME          = $env:USERNAME
"
