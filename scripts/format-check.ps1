param(
    [switch]$FixMode,
    [string]$ClangFormat = "clang-format"
)

# Performs format check using clang-format (same rules as CI clang-format-18).
# Usage: .\scripts\Format-Check.ps1 [-FixMode] [-ClangFormat clang-format-18]

Write-Output "Starting format check script."

# Find all headers and sources recursively
$Files = @()
$Files += Get-ChildItem -Path "include" -Recurse -Include "*.h"  | Select-Object -ExpandProperty FullName
$Files += Get-ChildItem -Path "src"     -Recurse -Include "*.h"  | Select-Object -ExpandProperty FullName
$Files += Get-ChildItem -Path "src"     -Recurse -Include "*.cpp"| Select-Object -ExpandProperty FullName

if ($Files.Count -eq 0) {
    Write-Warning "No header or source files found in include/ or src/."
    exit 0
}

if ($FixMode) {
    Write-Output ">>> Fixing format ($($Files.Count) files)..."
    foreach ($File in $Files) {
        & $ClangFormat -i "$File"
        if (-not $?) {
            Write-Error "clang-format failed on: $File"
            exit 1
        }
    }
    Write-Output ">>> Format fixing completed."
} else {
    Write-Output ">>> Checking format ($($Files.Count) files)..."
    $Failed = $false
    foreach ($File in $Files) {
        & $ClangFormat --dry-run --Werror "$File"
        if (-not $?) {
            $Failed = $true
        }
    }
    if ($Failed) {
        Write-Error ">>> Format check FAILED. Run with -FixMode to auto-fix."
        exit 1
    }
    Write-Output ">>> Format check successful."
}
