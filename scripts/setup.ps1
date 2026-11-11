param([switch]$SkipConfigure)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Version = "154.0.34+g14c5a08+chromium-154.0.8037.98"
$Platform = "windows64"
$Archive = "cef_binary_$($Version)_$($Platform).tar.bz2"
$Folder = "cef_binary_$($Version)_$($Platform)"
$ThirdParty = Join-Path $Root "third_party\cef"
$CefRoot = Join-Path $ThirdParty $Folder
$ArchivePath = Join-Path $ThirdParty $Archive
$HashPath = "$ArchivePath.sha1"

function Invoke-Download {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $PartialPath = "$Destination.partial"
    Remove-Item $PartialPath -Force -ErrorAction SilentlyContinue

    Write-Host "Downloading: $Url" -ForegroundColor Yellow
    Write-Host "Progress is reported by curl; automatic retries are enabled."

    if (-not (Get-Command curl.exe -ErrorAction SilentlyContinue)) {
        throw "curl.exe is required on the Windows build runner."
    }

    $CurlArgs = @(
        "--fail",
        "--location",
        "--show-error",
        "--progress-bar",
        "--retry", "5",
        "--retry-all-errors",
        "--retry-delay", "5",
        "--connect-timeout", "30",
        "--max-time", "3600",
        "--output", $PartialPath,
        $Url
    )
    & curl.exe @CurlArgs

    $CurlExitCode = $LASTEXITCODE
    if ($CurlExitCode -ne 0) {
        Remove-Item $PartialPath -Force -ErrorAction SilentlyContinue
        throw "Download failed with curl exit code $CurlExitCode."
    }

    if (-not (Test-Path $PartialPath) -or (Get-Item $PartialPath).Length -le 0) {
        Remove-Item $PartialPath -Force -ErrorAction SilentlyContinue
        throw "Download returned an empty file: $Url"
    }

    Move-Item -Path $PartialPath -Destination $Destination -Force
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake 3.24 or newer is required."
}

New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null

$FindCefFile = Join-Path $CefRoot "cmake\FindCEF.cmake"
if (-not (Test-Path $FindCefFile)) {
    # Remove an incomplete extraction before attempting a clean extraction.
    if (Test-Path $CefRoot) {
        Remove-Item $CefRoot -Recurse -Force
    }

    $EncodedArchive = $Archive.Replace("+", "%2B")
    $BaseUrl = "https://cef-builds.spotifycdn.com/$EncodedArchive"

    if (-not (Test-Path $HashPath)) {
        Write-Host "Fetching the official CEF SHA-1 checksum..."
        Invoke-Download -Url "$BaseUrl.sha1" -Destination $HashPath
    }

    if (-not (Test-Path $ArchivePath)) {
        Write-Host "CEF cache miss: downloading the official Windows x64 distribution (about 350 MB)." -ForegroundColor Yellow
        Invoke-Download -Url $BaseUrl -Destination $ArchivePath
    }
    else {
        Write-Host "CEF archive already exists; reusing it."
    }

    $HashText = Get-Content $HashPath -Raw
    $HashMatch = [regex]::Match($HashText, "(?i)\b([a-f0-9]{40})\b")
    if (-not $HashMatch.Success) {
        throw "The CEF checksum response did not contain a valid SHA-1 hash."
    }

    $ExpectedHash = $HashMatch.Groups[1].Value.ToLowerInvariant()
    $ActualHash = (Get-FileHash -Algorithm SHA1 -Path $ArchivePath).Hash.ToLowerInvariant()

    if ($ActualHash -ne $ExpectedHash) {
        Remove-Item $ArchivePath -Force -ErrorAction SilentlyContinue
        throw "CEF SHA-1 verification failed. The downloaded archive was removed."
    }

    Write-Host "CEF checksum verified. Extracting the distribution..." -ForegroundColor Green
    & tar.exe -xjf $ArchivePath -C $ThirdParty
    $TarExitCode = $LASTEXITCODE
    if ($TarExitCode -ne 0) {
        Remove-Item $CefRoot -Recurse -Force -ErrorAction SilentlyContinue
        throw "CEF extraction failed with tar exit code $TarExitCode."
    }

    Remove-Item $ArchivePath, $HashPath -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path $FindCefFile)) {
    throw "CEF folder not found or incomplete after extraction: $CefRoot"
}

Write-Host "CEF runtime ready: $CefRoot" -ForegroundColor Green

if (-not $SkipConfigure) {
    Write-Host "Configuring Visual Studio 2022 x64..." -ForegroundColor Yellow
    & cmake -S $Root -B (Join-Path $Root "build") -G "Visual Studio 17" -A x64 "-DCEF_ROOT=$CefRoot"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed. Install Visual Studio 2022 with Desktop development with C++."
    }

    Write-Host "Configuration complete. Run scripts\build.ps1." -ForegroundColor Green
}
