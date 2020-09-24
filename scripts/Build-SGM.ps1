<#
    This script builds the SavedGameManager and populates it into the Unreal project's
    ThirdParty directory.

    By default, the script builds Release for Desktkop, Emulator and HoloLens. The Configurations
    and Platforms parameters can be used to override this behavior
#>
[CmdletBinding()]
param (
    [Parameter(Mandatory=$false)][string[]]$Configurations,
    [Parameter(Mandatory=$false)][string[]]$Platforms
)

$RootPath = "$PSScriptRoot/..";

$NuGet = "$RootPath/Pipes/Binaries/nuget.exe";

$SGM = "SavedGameManager";
$SGMRoot = "$RootPath/$SGM"
$SGMSolution = "$SGMRoot/$SGM.sln"
$SGMOut = "$RootPath/Pipes/ThirdParty/$SGM"


if ($null -eq (Get-Command $NuGet -ErrorAction SilentlyContinue))
{
    if (!(Test-Path $Nuget))
    {
        New-Item -ItemType Directory -Path "$RootPath/Pipes/Binaries" -Force
        Invoke-WebRequest "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $Nuget
    }
}

Function Find-MsBuild()
{
    if (-Not (Get-Command -Module VSSetup))
    {
        Install-Module VSSetup -Scope CurrentUser
    }

    return Join-Path (Get-VSSetupInstance | Select-VSSetupInstance -Version 16.0 -Latest).InstallationPath "MSBuild/Current/Bin/amd64/msbuild.exe"
}

Function Populate-File
{
    Param(
        [string]$SourceFilePath,
        [string]$DestinationDirectory
    )

    if (-Not (Test-Path $DestinationDirectory))
    {
        Write-Host "Creating $DestinationDirectory" -ForegroundColor Cyan
        New-Item -ItemType Directory -Path $DestinationDirectory
    }

    Write-Host "Copying $SourceFilePath to $DestinationDirectory" -ForegroundColor Cyan

    Copy-Item -Path $SourceFilePath -Destination $DestinationDirectory
}

Function Populate-Inc
{
    Populate-File -SourceFilePath $SGMRoot/$SGM.h -DestinationDirectory $SGMOut/inc
}

Function Populate-Binary
{
    Param (
        [string]$Configuration,
        [string]$Platform
    )

    $SourceDir = "$SGMRoot/bld/bin"

    switch ($Platform)
    {
        'Desktop' {
            $SourceDir = "$SourceDir/x64/$Configuration"
            break;
        }
        'Emulator' {
            $SourceDir = "$SourceDir/x64/$Configuration" + "-uwp"
            break;
        }
        'HoloLens' {
            $SourceDir = "$SourceDir/arm64/$Configuration" + "-uwp"
            break;
        }
    }

    Populate-File -SourceFilePath "$SourceDir/$SGM.dll" -DestinationDirectory "$SGMOut/bin/$Platform"
    Populate-File -SourceFilePath "$SourceDir/$SGM.pdb" -DestinationDirectory "$SGMOut/bin/$Platform"
    Populate-File -SourceFilePath "$SourceDir/$SGM.lib" -DestinationDirectory "$SGMOut/lib/$Platform"
}

Function Build-SGM
{
    Param (
        [string]$Configuration,
        [string]$Platform
    )

    $Msbuild = Find-MsBuild
    $Targets = '/t:'
    if ($ForceClean)
    {
        $Targets += 'Clean;'
    }
    $Targets += 'Build'

    Write-Host "Building $($SGMSolution)" -foregroundcolor green
            & "$($Msbuild)" "$($SGMSolution)" $Targets  /m /p:Configuration=$Configuration /p:Platform=$Platform

    Populate-Binary -Configuration $Configuration -Platform $Platform
}

Function Restore-Nuget()
{
    Write-Host "Run NuGet restore on $($SGMSolution)" -foregroundcolor green
            & "$($Nuget)" restore "$($SGMSolution)" /force
}

if (Test-Path $SGMOut)
{
    Write-Host "Removing $SGMOut" -ForegroundColor Cyan
    Remove-Item $SGMOut -Recurse
}

Restore-Nuget

if (!$Configurations -Or $Configurations.Length -eq 0)
{
    $Configurations = 'Release';
}

if (!$Platforms -Or $Platforms.Length -eq 0)
{
    $Platforms = 'Desktop', 'Emulator', 'HoloLens';
}

foreach ($Configuration in $Configurations)
{
    foreach ($Platform in $Platforms)
    {
        Build-SGM $Configuration $Platform
    }
}

Populate-Inc

