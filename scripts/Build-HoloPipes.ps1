<#
    This script builds HoloPipes

    By default, the script builds Shipping for HoloLens. The Platform
    and Config parameters can be used to override this behavior
#>

[CmdletBinding()]
param (
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [Parameter(Mandatory=$false)][string]$SourcesRoot,
    [Parameter(Mandatory=$false)][string]$OutputPath,
    [Parameter(Mandatory=$false)][string]$Config,
    [Parameter(Mandatory=$false)][string]$Platform,
    [Parameter(Mandatory=$false)][switch]$Prefast
)

if (!$Config)
{
    $Config = "Shipping"
}

if (!$Platform)
{
    $Platform = "Hololens"
}

if (!$SourcesRoot)
{
    $SourcesRoot = "$PSScriptRoot/..";
}

if (!$OutputPath)
{
    $OutputPath = "$SourcesRoot/Bundle"
}

Function Update-Prefast
{
    Param(
        [bool]$EnablePrefast
    )

    $prefastOption = if ($EnablePrefast) {"True"} else {"False"}
    $iniFile = "$SourcesRoot\Pipes\config\Hololens\HololensEngine.ini"

    if (!((Get-Content -path $iniFile -raw) -match "bRunNativeCodeAnalysis=$prefastOption"))
     {
        Write-Host -ForegroundColor Cyan "Setting native code analysis to $prefastOption for project"

        if((Get-Content -path $iniFile -raw) -match "bRunNativeCodeAnalysis=.*")
        {
            (Get-Content -path $iniFile -raw) -replace "bRunNativeCodeAnalysis=.*","bRunNativeCodeAnalysis=$prefastOption" | Set-Content $iniFile
        }
        else
        {
            Add-Content $iniFile "bRunNativeCodeAnalysis=$prefastOption"
        }

    }

    if (!((Get-Content -path $iniFile -raw) -match "NativeCodeAnalysisRuleset=.*"))
    {
        Write-Host -ForegroundColor Cyan "Adding SDL code analysis ruleset to project"
        Add-Content $iniFile "NativeCodeAnalysisRuleset=..\sdl\SDL-7.0-Required.ruleset"
    }
}

Update-Prefast -EnablePrefast $Prefast.IsPresent

$buildCommand = ". `"$($EngineRoot)\Engine\Build\BatchFiles\RunUAT.bat`" -ScriptsForProject=`"$($SourcesRoot)\Pipes\HoloPipes.uproject`" BuildCookRun -project=`"$SourcesRoot\Pipes\HoloPipes.uproject`" -clean -build -cook -allmaps -stage -archive -archivedirectory=`"$OutputPath`" -package -ue4exe=`"$EngineRoot\Engine\Binaries\Win64\UE4Editor-Cmd.exe`" -prereqs -targetplatform=$Platform -target=HoloPipes -clientconfig=$Config -utf8output"

Write-Host -ForegroundColor Green "Running command: '$($buildCommand)'"

Invoke-Expression $buildCommand

Update-Prefast -EnablePrefast $false