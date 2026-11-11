param([switch]$SkipConfigure)
$ErrorActionPreference="Stop";$ProgressPreference="SilentlyContinue"
$Root=(Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Version="154.0.34+g14c5a08+chromium-154.0.8037.98";$Platform="windows64"
$Archive="cef_binary_$($Version)_$($Platform).tar.bz2"
$Folder="cef_binary_$($Version)_$($Platform)"
$ThirdParty=Join-Path $Root "third_party\cef";$CefRoot=Join-Path $ThirdParty $Folder
$ArchivePath=Join-Path $ThirdParty $Archive;$HashPath="$ArchivePath.sha1"

if(!(Get-Command cmake -ErrorAction SilentlyContinue)){throw "CMake 3.24 or newer is required."}
New-Item -ItemType Directory -Force -Path $ThirdParty|Out-Null
if(!(Test-Path (Join-Path $CefRoot "cmake\FindCEF.cmake"))){
  if(!(Test-Path $ArchivePath)){
    $url="https://cef-builds.spotifycdn.com/"+$Archive.Replace("+","%2B")
    Write-Host "Downloading official CEF Windows x64 binaries (about 350 MB)..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "$url.sha1" -OutFile $HashPath
    Invoke-WebRequest -Uri $url -OutFile $ArchivePath
  }
  if(!(Test-Path $HashPath)){
    $url="https://cef-builds.spotifycdn.com/"+$Archive.Replace("+","%2B")
    Invoke-WebRequest -Uri "$url.sha1" -OutFile $HashPath
  }
  $expected=((Get-Content $HashPath -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
  $actual=(Get-FileHash -Algorithm SHA1 $ArchivePath).Hash.ToLowerInvariant()
  if($actual -ne $expected){Remove-Item $ArchivePath -Force;throw "CEF SHA-1 verification failed; archive removed."}
  Write-Host "Extracting CEF..." -ForegroundColor Yellow
  & tar.exe -xjf $ArchivePath -C $ThirdParty
  if($LASTEXITCODE -ne 0){throw "CEF extraction failed; check that tar.exe is available."}
  Remove-Item $ArchivePath,$HashPath -Force -ErrorAction SilentlyContinue
}
if(!(Test-Path (Join-Path $CefRoot "cmake\FindCEF.cmake"))){throw "CEF folder not found after extraction: $CefRoot"}
Write-Host "CEF runtime ready." -ForegroundColor Green
if(!$SkipConfigure){
  Write-Host "Configuring Visual Studio 2022 x64..." -ForegroundColor Yellow
  & cmake -S $Root -B (Join-Path $Root "build") -G "Visual Studio 17" -A x64 "-DCEF_ROOT=$CefRoot"
  if($LASTEXITCODE -ne 0){throw "CMake configure failed. Install Visual Studio 2022 with Desktop development with C++."}
  Write-Host "Configuration complete. Run scripts\build.ps1." -ForegroundColor Green
}
