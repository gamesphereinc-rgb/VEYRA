[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerDirectory,

    [Parameter(Mandatory = $true)]
    [string]$TestRoot
)

$ErrorActionPreference = "Stop"
$installer = Get-ChildItem -LiteralPath $InstallerDirectory -Filter "VEYRA-Setup-*-x64.exe" -File |
    Select-Object -First 1
if (-not $installer) {
    throw "The packaged VEYRA setup executable was not found."
}

$installDirectory = Join-Path $TestRoot "install"
$startMenuShortcut = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\VEYRA.lnk"
$uninstallRoot = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall"
$profileDirectory = Join-Path $env:LOCALAPPDATA "VEYRA\Profile"
$markerPath = Join-Path $profileDirectory ("installer-test-" + [guid]::NewGuid().ToString("N") + ".txt")
$profileExisted = Test-Path -LiteralPath $profileDirectory
$markerCreated = $false

try {
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $profileDirectory -Force | Out-Null
    Set-Content -LiteralPath $markerPath -Value "VEYRA installer profile preservation test" -Encoding UTF8
    $markerCreated = $true

    Write-Host "Installing VEYRA silently into $installDirectory"
    $arguments = @(
        "/VERYSILENT",
        "/SUPPRESSMSGBOXES",
        "/NORESTART",
        "/SP-",
        "/CURRENTUSER",
        "/DIR=$installDirectory"
    )
    $setupProcess = Start-Process -FilePath $installer.FullName -ArgumentList $arguments -Wait -PassThru
    if ($setupProcess.ExitCode -ne 0) {
        throw "VEYRA setup returned exit code $($setupProcess.ExitCode)."
    }

    $installedExe = Join-Path $installDirectory "VEYRA.exe"
    $installedBootstrap = Join-Path $installDirectory "bootstrap.exe"
    $installedIcon = Join-Path $installDirectory "VEYRA.ico"
    if (-not (Test-Path -LiteralPath $installedExe -PathType Leaf)) {
        throw "Setup completed but VEYRA.exe was not installed."
    }
    if (-not (Test-Path -LiteralPath $installedBootstrap -PathType Leaf)) {
        throw "Setup completed but the original CEF bootstrap.exe was not installed."
    }
    if (-not (Test-Path -LiteralPath $installedIcon -PathType Leaf)) {
        throw "Setup completed but VEYRA.ico was not installed."
    }
    $bootstrapHash = (Get-FileHash -LiteralPath $installedBootstrap -Algorithm SHA256).Hash
    $renamedHash = (Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash
    if ($bootstrapHash -ne $renamedHash) {
        throw "VEYRA.exe is not byte-for-byte identical to bootstrap.exe."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $installDirectory "VEYRA.dll") -PathType Leaf)) {
        throw "Setup completed but VEYRA.dll is missing."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $installDirectory "ui\newtab.html") -PathType Leaf)) {
        throw "Setup completed but the VEYRA UI files are missing."
    }
    if (-not (Test-Path -LiteralPath $startMenuShortcut -PathType Leaf)) {
        throw "Setup did not create the VEYRA Start Menu shortcut."
    }
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($startMenuShortcut)
    if ((Split-Path -Leaf $shortcut.TargetPath) -ne "VEYRA.exe") {
        throw "The VEYRA Start Menu shortcut does not target VEYRA.exe."
    }
    if ($shortcut.IconLocation -notmatch "VEYRA\.ico") {
        throw "The VEYRA Start Menu shortcut does not use the VEYRA sidecar icon."
    }

    $uninstallEntry = Get-ChildItem -LiteralPath $uninstallRoot -ErrorAction Stop |
        ForEach-Object { Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue } |
        Where-Object { $_.DisplayName -eq "VEYRA" } |
        Select-Object -First 1
    if (-not $uninstallEntry -or -not $uninstallEntry.UninstallString) {
        throw "VEYRA was not registered in Windows Installed Apps."
    }
    Write-Host "Install verification passed: EXE, DLL, UI, Start Menu shortcut, and uninstall registration."

    $uninstallCommand = [string]$uninstallEntry.UninstallString
    if ($uninstallCommand -match '^\s*"(?<exe>[^"]+)"') {
        $uninstaller = $Matches.exe
    } else {
        $uninstaller = ($uninstallCommand -split '\s+', 2)[0]
    }
    if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        throw "Windows uninstall entry points to a missing uninstaller: $uninstaller"
    }

    Write-Host "Uninstalling VEYRA silently"
    $uninstallProcess = Start-Process -FilePath $uninstaller -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") -Wait -PassThru
    if ($uninstallProcess.ExitCode -ne 0) {
        throw "VEYRA uninstaller returned exit code $($uninstallProcess.ExitCode)."
    }

    if (Test-Path -LiteralPath $installedExe) {
        throw "VEYRA.exe remains after uninstall."
    }
    if (Test-Path -LiteralPath $startMenuShortcut) {
        throw "VEYRA Start Menu shortcut remains after uninstall."
    }
    $remainingEntry = Get-ChildItem -LiteralPath $uninstallRoot -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction SilentlyContinue } |
        Where-Object { $_.DisplayName -eq "VEYRA" } |
        Select-Object -First 1
    if ($remainingEntry) {
        throw "VEYRA remains registered in Windows Installed Apps after uninstall."
    }
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        throw "Uninstall unexpectedly removed the browser profile marker."
    }
    Write-Host "Uninstall verification passed: app files and shortcut removed; browser profile data preserved."
}
finally {
    if ($markerCreated -and (Test-Path -LiteralPath $markerPath)) {
        Remove-Item -LiteralPath $markerPath -Force -ErrorAction SilentlyContinue
    }
    if (-not $profileExisted -and (Test-Path -LiteralPath $profileDirectory)) {
        $remaining = Get-ChildItem -LiteralPath $profileDirectory -Force -ErrorAction SilentlyContinue
        if (-not $remaining) {
            Remove-Item -LiteralPath $profileDirectory -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $TestRoot) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "VEYRA installer smoke test completed successfully."
