param([ValidateSet("Debug","Release")][string]$Configuration="Release")
$ErrorActionPreference="Stop"
$Root=(Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$CefRoot=Join-Path $Root "third_party\cef\cef_binary_154.0.34+g14c5a08+chromium-154.0.8037.98_windows64_minimal"

if(!(Test-Path (Join-Path $Root "build\VEYRA.sln"))){
    & (Join-Path $PSScriptRoot "setup.ps1")
    if($LASTEXITCODE -ne 0){throw "VEYRA setup failed."}
}

if($Configuration -eq "Debug" -and !(Test-Path (Join-Path $CefRoot "Debug\libcef.lib"))){
    throw "The minimal CEF distribution contains Release binaries only. Use -Configuration Release, or configure with a full CEF distribution for Debug builds."
}

& cmake --build (Join-Path $Root "build") --config $Configuration --target VEYRA --parallel
if($LASTEXITCODE -ne 0){throw "VEYRA build failed. Review the compiler output."}
Write-Host "Build complete: $Root\build\out\VEYRA.exe" -ForegroundColor Green
