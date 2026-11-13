param([switch]$SkipConfigure)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Version = "156.0.7+g47dbc1d+chromium-156.0.8078.17"
$Platform = "windows64_beta_minimal"
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

function Find-SevenZip {
    $Command = Get-Command "7z.exe" -ErrorAction SilentlyContinue
    if ($Command -and $Command.Source) {
        return $Command.Source
    }

    if ($env:ProgramFiles) {
        $Candidate = Join-Path $env:ProgramFiles "7-Zip\7z.exe"
        if (Test-Path $Candidate) {
            return $Candidate
        }
    }

    return $null
}

function Expand-CefArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchiveFile,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $Timer = [System.Diagnostics.Stopwatch]::StartNew()
    $SevenZip = Find-SevenZip

    if ($SevenZip) {
        Write-Host "Using 7-Zip for streamed BZip2/TAR extraction: $SevenZip" -ForegroundColor Cyan

        # Stream the decompressed TAR directly into 7-Zip. This avoids writing
        # a huge intermediate .tar file and exposes extraction progress.
        $CommandFile = Join-Path $env:TEMP "veyra-cef-extract-$PID.cmd"
        $Pipeline = '"' + $SevenZip + '" x -so "' + $ArchiveFile + '" -bsp0 | "' +
            $SevenZip + '" x -si -ttar -o"' + $Destination + '" -y'
        $CommandText = "@echo off" + [Environment]::NewLine + $Pipeline +
            [Environment]::NewLine + "exit /b %ERRORLEVEL%" + [Environment]::NewLine

        try {
            [System.IO.File]::WriteAllText($CommandFile, $CommandText, [System.Text.Encoding]::ASCII)
            & $env:ComSpec /d /c ('"{0}"' -f $CommandFile)
            $ExtractExitCode = $LASTEXITCODE
        }
        finally {
            Remove-Item $CommandFile -Force -ErrorAction SilentlyContinue
        }
    }
    else {
        Write-Warning "7-Zip was not found. Falling back to Windows tar.exe; extraction may be slower."
        & tar.exe -xjf $ArchiveFile -C $Destination
        $ExtractExitCode = $LASTEXITCODE
    }

    $Timer.Stop()
    Write-Host "CEF extraction process finished in $($Timer.Elapsed.ToString())." -ForegroundColor Cyan

    if ($ExtractExitCode -ne 0) {
        throw "CEF extraction failed with exit code $ExtractExitCode."
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake 3.24 or newer is required."
}

New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null

$FindCefFile = Join-Path $CefRoot "cmake\FindCEF.cmake"
$RequiredFiles = @(
    $FindCefFile,
    (Join-Path $CefRoot "Release\libcef.lib"),
    (Join-Path $CefRoot "Release\bootstrap.exe")
)

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
        Write-Host "CEF 156 beta cache miss: downloading the Windows x64 minimal distribution (about 167 MB)." -ForegroundColor Yellow
        Invoke-Download -Url $BaseUrl -Destination $ArchivePath
    }
    else {
        Write-Host "CEF minimal archive already exists; reusing it."
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

    Write-Host "CEF SHA-1 checksum verified. Beginning minimal distribution extraction..." -ForegroundColor Green
    try {
        Expand-CefArchive -ArchiveFile $ArchivePath -Destination $ThirdParty
    }
    catch {
        if (Test-Path $CefRoot) {
            Remove-Item $CefRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
        throw
    }

    Remove-Item $ArchivePath, $HashPath -Force -ErrorAction SilentlyContinue
}

foreach ($RequiredFile in $RequiredFiles) {
    if (-not (Test-Path $RequiredFile)) {
        throw "CEF minimal distribution is incomplete. Required file not found: $RequiredFile"
    }
}

Write-Host "CEF minimal runtime ready: $CefRoot" -ForegroundColor Green

if (-not $SkipConfigure) {
    Write-Host "Configuring Visual Studio 2022 x64..." -ForegroundColor Yellow
    & cmake -S $Root -B (Join-Path $Root "build") -G "Visual Studio 17" -A x64 "-DCEF_ROOT=$CefRoot"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed. Install Visual Studio 2022 with Desktop development with C++."
    }

    Write-Host "Configuration complete. Run scripts\build.ps1." -ForegroundColor Green
}
