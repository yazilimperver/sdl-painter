param(
    [string]$BuildType = "Debug",
    [switch]$Vulkan,
    [switch]$NoExamples,
    [switch]$NoTests
)

# Conan package dependencies installer.
# Usage: .\scripts\Conan-Install.ps1 [Debug|Release|ASan]
#                                    [-Vulkan] [-NoExamples] [-NoTests]

Write-Output "Starting Conan installation."

$Preset = ""
$script:BUILD_TYPE = $BuildType

# Determine the build type and associated preset
switch ($BuildType) {
    "Debug"   { $Preset = "windows-debug" }
    "Release" { $Preset = "windows-release" }
    "ASan"    { $Preset = "windows-debug" ; $script:BUILD_TYPE = "Debug" }
    default {
        Write-Error "Error: Unknown build type '$BuildType'. Use Debug|Release|ASan."
        exit 1
    }
}

$OutputFolder = "build/$Preset/generators"

# Build Conan options list. Boolean flags are forwarded as -o sdl_painter/*:foo=...
$ConanOptions = @()
if ($Vulkan)     { $ConanOptions += @("-o", "sdl_painter/*:with_vulkan=True") }
if ($NoExamples) { $ConanOptions += @("-o", "sdl_painter/*:build_examples=False") }
if ($NoTests)    { $ConanOptions += @("-o", "sdl_painter/*:build_tests=False") }

$OptionsSummary = if ($ConanOptions.Count -gt 0) { ($ConanOptions -join ' ') } else { '(defaults)' }
Write-Output ">>> Conan install: build_type=$script:BUILD_TYPE, output=$OutputFolder, options=$OptionsSummary"

# Install Conan dependencies
conan install . `
    --output-folder="$OutputFolder" `
    --build=missing `
    -s build_type="$script:BUILD_TYPE" `
    @ConanOptions
if (-not $?) { exit $LASTEXITCODE }

Write-Output ">>> Conan installation completed."
