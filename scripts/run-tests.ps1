param(
    [string]$BuildType = "Debug",
    [string]$Filter = ""
)

# GTest unit tests run script.
# Usage:
#   .\scripts\Run-Tests.ps1                       # All tests, Debug
#   .\scripts\Run-Tests.ps1 Release               # All tests, Release
#   .\scripts\Run-Tests.ps1 -Filter Transform     # Only TransformTest.* matches

Write-Output "Starting test execution script."

$Preset = ""

switch ($BuildType) {
    "Debug"   { $Preset = "windows-debug" }
    "Release" { $Preset = "windows-release" }
    default {
        Write-Error "Error: Unknown build type '$BuildType'. Use Debug|Release."
        exit 1
    }
}

$CtestArgs = @(
    "--preset", $Preset,
    "-C", $BuildType,
    "--output-on-failure"
)

if ($Filter -ne "") {
    $CtestArgs += @("-R", $Filter)
    Write-Output ">>> Test execution starting: preset=$Preset filter=/$Filter/"
} else {
    Write-Output ">>> Test execution starting: preset=$Preset"
}

# Execute ctest with the determined preset
# -C flag required for MSVC multi-config generators
ctest @CtestArgs
if (-not $?) { exit $LASTEXITCODE }

Write-Output ">>> Test execution finished."
