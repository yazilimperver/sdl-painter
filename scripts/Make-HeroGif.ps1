<#
.SYNOPSIS
    hero demosunun PPM karelerinden README tanitim GIF'ini uretir.

.DESCRIPTION
    Once kareleri uret:
        .\build\windows-debug\examples\Debug\hero.exe --dump-frames build\hero_frames
    Sonra bu script:
        .\scripts\Make-HeroGif.ps1

    Iki gecisli palet kullanilir: once sahneye ozel 256 renklik palet
    cikarilir (palettegen), sonra kareler o palete esleneir (paletteuse).
    Tek gecisli donusum bu sahnedeki degradelerde gorunur bantlanma yapiyor.

    GIF boyutu hedefi asarsa -Fps veya -Width dusurun; script uretilen
    boyutu her seferinde yazar.

.PARAMETER FramesDir
    PPM karelerinin bulundugu dizin.

.PARAMETER Output
    Uretilecek GIF yolu.

.PARAMETER Fps
    Cikti kare hizi. Kareler 30 fps varsayimiyla uretilir; dusuk deger
    dosyayi kucultur.

.PARAMETER Width
    Cikti genisligi (piksel). Yukseklik en-boy oranindan hesaplanir.

.PARAMETER Mp4
    GIF yerine (veya yaninda) mp4 da uret. GitHub markdown'da video
    embed destegi var ve ayni kalite cok daha kucuk dosyayla geliyor.

.PARAMETER FFmpeg
    ffmpeg.exe yolu. Verilmezse once PATH'e, sonra winget/choco'nun bilinen
    kurulum dizinlerine bakilir.
#>
[CmdletBinding()]
param(
    [string]$FramesDir = "build/hero_frames",
    [string]$Output    = "doc/hero.gif",
    [int]$Fps          = 15,
    [int]$Width        = 720,
    [switch]$Mp4,
    [string]$FFmpeg
)

$ErrorActionPreference = 'Stop'

# ffmpeg'i bul. winget PATH'i guncelledigi halde ACIK olan terminale
# yansitmaz; bu yuzden PATH'te bulamazsak kurulum dizinlerine de bakiyoruz —
# aksi halde "kurdum ama calismiyor" tuzagina dusuluyor.
function Resolve-FFmpeg {
    param([string]$Explicit)

    if ($Explicit) {
        if (Test-Path $Explicit) { return (Resolve-Path $Explicit).Path }
        Write-Error "Belirtilen ffmpeg bulunamadi: $Explicit"
    }

    $onPath = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
    if ($onPath) { return $onPath }

    $roots = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages",
        "$env:ProgramData\chocolatey\bin"
    )
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $hit = Get-ChildItem $root -Filter ffmpeg.exe -Recurse -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($hit) {
            Write-Warning "ffmpeg PATH'te degil, kurulum dizininde bulundu: $($hit.FullName)"
            Write-Warning "Kalici cozum: yeni bir terminal acin (winget PATH'i acik oturuma yansitmaz)."
            return $hit.FullName
        }
    }
    return $null
}

$ffmpeg = Resolve-FFmpeg -Explicit $FFmpeg
if (-not $ffmpeg) {
    Write-Error @"
ffmpeg bulunamadi. Kurulum:
    winget install --id Gyan.FFmpeg -e
    (veya) choco install ffmpeg
Kurulumdan sonra YENI bir terminal acin; winget PATH'i acik oturuma yansitmaz.
Alternatif: -FFmpeg "C:\yol\ffmpeg.exe"
"@
}

if (-not (Test-Path $FramesDir)) {
    Write-Error "Kare dizini yok: $FramesDir`nOnce: hero.exe --dump-frames $FramesDir"
}
$frames = Get-ChildItem -Path (Join-Path $FramesDir "frame_*.ppm") -ErrorAction SilentlyContinue
if ($frames.Count -eq 0) {
    Write-Error "$FramesDir icinde frame_*.ppm yok.`nOnce: hero.exe --dump-frames $FramesDir"
}
Write-Host "$($frames.Count) kare bulundu."

$outDir = Split-Path -Parent $Output
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force $outDir | Out-Null
}

$palette = Join-Path $env:TEMP "sdlpainter_hero_palette.png"
$input   = Join-Path $FramesDir "frame_%04d.ppm"
$filters = "fps=$Fps,scale=${Width}:-1:flags=lanczos"

Write-Host "1/2  palet cikariliyor..."
& $ffmpeg -y -loglevel error -framerate 30 -i $input `
    -vf "$filters,palettegen=stats_mode=diff" $palette
if ($LASTEXITCODE -ne 0) { Write-Error "palettegen basarisiz (exit $LASTEXITCODE)" }

Write-Host "2/2  GIF uretiliyor..."
& $ffmpeg -y -loglevel error -framerate 30 -i $input -i $palette `
    -lavfi "$filters[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3" `
    -loop 0 $Output
if ($LASTEXITCODE -ne 0) { Write-Error "paletteuse basarisiz (exit $LASTEXITCODE)" }

Remove-Item $palette -Force -ErrorAction SilentlyContinue

$mb = [math]::Round((Get-Item $Output).Length / 1MB, 2)
Write-Host "$Output  ->  $mb MB  (${Width}px, $Fps fps)"
if ($mb -gt 3) {
    Write-Warning "3 MB hedefinin uzerinde. -Fps 12 veya -Width 640 deneyin, ya da -Mp4 kullanin."
}

if ($Mp4) {
    $mp4Out = [System.IO.Path]::ChangeExtension($Output, ".mp4")
    Write-Host "mp4 uretiliyor..."
    # yuv420p + cift sayiya yuvarlama: eski oynaticilar ve GitHub icin gerekli.
    & $ffmpeg -y -loglevel error -framerate 30 -i $input `
        -vf "scale=${Width}:-2:flags=lanczos" `
        -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart $mp4Out
    if ($LASTEXITCODE -ne 0) { Write-Error "mp4 uretimi basarisiz (exit $LASTEXITCODE)" }
    $mp4Mb = [math]::Round((Get-Item $mp4Out).Length / 1MB, 2)
    Write-Host "$mp4Out  ->  $mp4Mb MB"
}
