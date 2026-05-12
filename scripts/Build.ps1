param(
    [string]$BuildType = "Debug",
    [switch]$SkipConan,
    [switch]$Vulkan,
    [switch]$NoExamples,
    [switch]$NoTests,
    [string]$Target = "",
    [switch]$Clean,
    [int]$Jobs = 0,
    [switch]$Docs
)

# Builds the project. Runs Conan install automatically if toolchain is missing.
# Usage:
#   .\scripts\Build.ps1                                # Debug, default options
#   .\scripts\Build.ps1 Release                        # Release
#   .\scripts\Build.ps1 -Vulkan                        # Debug + Vulkan backend
#   .\scripts\Build.ps1 Release -Vulkan -NoExamples    # Combined options
#   .\scripts\Build.ps1 -Target phase2b_demo           # Build single target
#   .\scripts\Build.ps1 -Clean                         # Wipe build dir, reinstall, rebuild
#   .\scripts\Build.ps1 -Jobs 8                        # Parallel build with 8 jobs
#   .\scripts\Build.ps1 -Docs                          # Build + Doxygen HTML documentation

Write-Output "Starting build script."

$ConfigurePreset = ""
$BuildPreset = ""

switch ($BuildType) {
    "Debug"   { $ConfigurePreset = "windows-debug";   $BuildPreset = "windows-debug" }
    "Release" { $ConfigurePreset = "windows-release"; $BuildPreset = "windows-release" }
    default {
        Write-Error "Error: Unknown build type '$BuildType'. Use Debug|Release."
        exit 1
    }
}

$BuildDir      = "build/$ConfigurePreset"
$ToolchainFile = "$BuildDir/generators/build/generators/conan_toolchain.cmake"

# Conan opsiyonlari ile toolchain icerigi degisir; flag setini bir
# stamp dosyasina yazip karsilastirarak stale toolchain'i tespit ediyoruz.
$OptionStamp = @()
if ($Vulkan)     { $OptionStamp += "vulkan" }
if ($NoExamples) { $OptionStamp += "no-examples" }
if ($NoTests)    { $OptionStamp += "no-tests" }
$OptionStampStr = ($OptionStamp -join ',')
$StampFile = "$BuildDir/.sdlpainter-options.stamp"

if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Output ">>> Clean: removing $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    } else {
        Write-Output ">>> Clean: $BuildDir already absent"
    }
}

# Conan install kosullari:
#   1) -SkipConan verilmediyse VE
#   2) toolchain yok, ya da kayitli opsiyonlar mevcut flag setiyle uyusmuyor
$NeedConan = $false
if (-not $SkipConan) {
    if (-not (Test-Path $ToolchainFile)) {
        $NeedConan = $true
        Write-Output ">>> Toolchain not found, Conan install required."
    } elseif (Test-Path $StampFile) {
        $PreviousOptions = (Get-Content $StampFile -Raw -ErrorAction SilentlyContinue).Trim()
        if ($PreviousOptions -ne $OptionStampStr) {
            $NeedConan = $true
            Write-Output ">>> Build options changed ('$PreviousOptions' -> '$OptionStampStr'); reinstalling Conan deps."
        }
    } else {
        # Toolchain var ama stamp yok -> bilinmeyen onceki opsiyonlar; guvenli taraf, yeniden kur.
        $NeedConan = $true
        Write-Output ">>> No option stamp found alongside existing toolchain; reinstalling Conan deps."
    }
}

if ($NeedConan) {
    # Hashtable splatting: -Name $Value cifti olarak gecirir; switch'ler bool.
    $ConanArgs = @{
        BuildType  = $BuildType
        Vulkan     = [bool]$Vulkan
        NoExamples = [bool]$NoExamples
        NoTests    = [bool]$NoTests
    }

    & "$PSScriptRoot\Conan-Install.ps1" @ConanArgs
    if (-not $?) { exit $LASTEXITCODE }

    # Yeni opsiyonlari stampe yaz.
    if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null }
    Set-Content -Path $StampFile -Value $OptionStampStr -Encoding utf8
}

Write-Output ">>> Configure: preset=$ConfigurePreset"

# 1. Configure the build system
cmake --preset "$ConfigurePreset"
if (-not $?) { exit $LASTEXITCODE }

# 2. Build the project
$BuildArgs = @("--build", "--preset", $BuildPreset)
if ($Target -ne "") {
    $BuildArgs += @("--target", $Target)
    Write-Output ">>> Build: preset=$BuildPreset target=$Target"
} else {
    Write-Output ">>> Build: preset=$BuildPreset (all targets)"
}
if ($Jobs -gt 0) {
    $BuildArgs += @("-j", $Jobs)
}

cmake @BuildArgs
if (-not $?) { exit $LASTEXITCODE }

Write-Output ">>> Build completed."

if ($Docs) {
    if (-not (Get-Command doxygen -ErrorAction SilentlyContinue)) {
        Write-Error "Hata: doxygen bulunamadı. Lütfen kurun ve PATH'e ekleyin."
        exit 1
    }
    Write-Output ">>> Doxygen dokümantasyonu oluşturuluyor..."
    doxygen Doxyfile
    if (-not $?) { exit $LASTEXITCODE }
    Write-Output ">>> Dokümantasyon hazır: build/docs/index.html"
}
