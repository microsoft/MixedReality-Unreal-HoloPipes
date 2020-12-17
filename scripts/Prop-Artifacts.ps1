<#
    This script gathers the build artifacts into a single directory for publishing

    By default, the script assumes the source root is one level up from the script,
    and that the prop location is an "Artifacts" directory beneath it. These can
    be changed with the SourceRoot and ArtifactRoot parameters.
#>
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)][string]$SourceRoot,
    [Parameter(Mandatory=$false)][string]$ArtifactRoot
)

if (!$SourceRoot)
{
    $SourceRoot = "$PSScriptRoot\..";
}

$BuildRoot = "$SourceRoot\pipes";

if (!$ArtifactRoot)
{
    $ArtifactRoot = "$SourceRoot\Artifacts";
}

write-host -ForegroundColor Cyan "Propping artifacts from $BuildRoot to $ArtifactRoot"

Function Prop-Directory
{
    Param (
        [string]$Src,
        [string]$Dst
    )

    $SrcPath = "$BuildRoot\$Src"

    if (Test-Path $SrcPath)
    {
        $DstPath = "$ArtifactRoot\$Dst";

        if (Test-Path $DstPath)
        {
            Write-Host "    Removing Stale $DstPath" -ForegroundColor Cyan
            Remove-Item $DstPath -Recurse
        }

        Write-Host "    Copying $SrcPath to $DstPath" -ForegroundColor Cyan
        Copy-Item -Path $SrcPath -Destination $DstPath -Recurse
    }
}

Prop-Directory -Src "Saved\Crashes" "Crashes"
Prop-Directory -Src "Saved\Logs" "Logs"
Prop-Directory -Src "Build\HoloLens" "Build"
Prop-Directory -Src "Binaries\HoloLens" "Binaries"
Prop-Directory -Src "Config" "Config"

