param(
 [Parameter(Mandatory=$true)][string]$ArchivePath,
 [Parameter(Mandatory=$true)][int]$ProcessId,
 [Parameter(Mandatory=$true)][string]$InstallDirectory
)
$ErrorActionPreference='Stop'
$updates=Join-Path $env:LOCALAPPDATA 'VEYRA\Updates'
$stage=Join-Path $updates 'staging'
$log=Join-Path $updates 'update-install.log'
try {
 $running=Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
 if($running){Wait-Process -Id $ProcessId -Timeout 180}
 if(Get-Process -Id $ProcessId -ErrorAction SilentlyContinue){throw 'VEYRA did not close in time. No files were replaced.'}
 if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force}
 New-Item -ItemType Directory -Path $stage -Force | Out-Null
 Expand-Archive -LiteralPath $ArchivePath -DestinationPath $stage -Force
 foreach($name in @('VEYRA.exe','VEYRA.dll','libcef.dll','icudtl.dat')){
  if(-not(Test-Path -LiteralPath (Join-Path $stage $name))){throw "Update package is missing $name. No files were replaced."}
 }
 if(-not(Test-Path -LiteralPath $InstallDirectory)){throw 'VEYRA installation folder no longer exists.'}
 Copy-Item -Path (Join-Path $stage '*') -Destination $InstallDirectory -Recurse -Force
 Remove-Item -LiteralPath $ArchivePath -Force -ErrorAction SilentlyContinue
 Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
 Start-Process -FilePath (Join-Path $InstallDirectory 'VEYRA.exe') -WorkingDirectory $InstallDirectory
 ("Update installed successfully at " + (Get-Date -Format o)) | Set-Content -LiteralPath $log
}
catch {
 ("Update failed at " + (Get-Date -Format o) + [Environment]::NewLine + $_.Exception.Message) | Set-Content -LiteralPath $log
 Add-Type -AssemblyName PresentationFramework
 [System.Windows.MessageBox]::Show(("VEYRA couldn't finish installing the update." + [Environment]::NewLine + [Environment]::NewLine + $_.Exception.Message + [Environment]::NewLine + "Details: " + $log),'VEYRA updater','OK','Warning') | Out-Null
}
