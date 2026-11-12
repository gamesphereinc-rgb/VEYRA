[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [Parameter(Mandatory = $true)]
    [string]$ResultsPath,

    [ValidateRange(10, 120)]
    [int]$StartupTimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"
$startedAt = Get-Date
$launchStatus = "FAILED"
$screenshotStatus = "NOT ATTEMPTED"
$windowTitle = ""
$failureMessage = ""
$process = $null
$notes = New-Object System.Collections.Generic.List[string]

New-Item -ItemType Directory -Force -Path $ResultsPath | Out-Null
$PackagePath = (Resolve-Path $PackagePath).Path
$exePath = Join-Path $PackagePath "VEYRA.exe"
$screenshotPath = Join-Path $ResultsPath "veyra-home.png"
$logPath = Join-Path $ResultsPath "startup-diagnostics.txt"

function Add-Note([string]$Message) {
    $notes.Add(("{0:HH:mm:ss} {1}" -f (Get-Date), $Message))
}

try {
    if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
        throw "VEYRA.exe was not found in the downloaded Windows build artifact."
    }

    Add-Note "Found packaged executable: $exePath"
    $process = Start-Process -FilePath $exePath -WorkingDirectory $PackagePath -PassThru
    Add-Note "Started VEYRA process with PID $($process.Id)."

    $deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
    $windowHandle = [IntPtr]::Zero

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 1
        $process.Refresh()

        if ($process.HasExited) {
            throw "VEYRA exited during startup with exit code $($process.ExitCode)."
        }

        $windowHandle = $process.MainWindowHandle
        if ($windowHandle -ne [IntPtr]::Zero) {
            $windowTitle = $process.MainWindowTitle
            if (-not [string]::IsNullOrWhiteSpace($windowTitle)) {
                break
            }
        }
    }

    if ($windowHandle -eq [IntPtr]::Zero -or [string]::IsNullOrWhiteSpace($windowTitle)) {
        throw "VEYRA did not expose a visible top-level window within $StartupTimeoutSeconds seconds."
    }

    $launchStatus = "PASSED"
    Add-Note "Main window appeared: $windowTitle"
    Add-Note "Waiting briefly for the local new-tab page to render."

    Start-Sleep -Seconds 8
    $process.Refresh()
    if ($process.HasExited) {
        throw "VEYRA exited before the preview could be captured."
    }

    try {
        Add-Type -AssemblyName System.Drawing
        Add-Type -AssemblyName System.Windows.Forms

        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace VeyraRemotePreview {
    [StructLayout(LayoutKind.Sequential)]
    public struct WindowRect {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static class NativeMethods {
        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool GetWindowRect(IntPtr hWnd, out WindowRect rect);

        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        public static extern bool ShowWindow(IntPtr hWnd, int command);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int width, int height, bool repaint);
    }
}
"@ | Out-Null

        $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $targetWidth = [Math]::Min(1100, [Math]::Max(720, $screen.Width - 40))
        $targetHeight = [Math]::Min(760, [Math]::Max(560, $screen.Height - 40))
        [void][VeyraRemotePreview.NativeMethods]::ShowWindow($windowHandle, 9)
        [void][VeyraRemotePreview.NativeMethods]::MoveWindow(
            $windowHandle, $screen.Left + 10, $screen.Top + 10,
            $targetWidth, $targetHeight, $true
        )
        [void][VeyraRemotePreview.NativeMethods]::SetForegroundWindow($windowHandle)
        Start-Sleep -Seconds 3

        $windowRect = New-Object VeyraRemotePreview.WindowRect
        if (-not [VeyraRemotePreview.NativeMethods]::GetWindowRect($windowHandle, [ref]$windowRect)) {
            throw "Windows could not read the VEYRA window bounds."
        }

        $left = [Math]::Max($screen.Left, $windowRect.Left)
        $top = [Math]::Max($screen.Top, $windowRect.Top)
        $right = [Math]::Min(($screen.Left + $screen.Width), $windowRect.Right)
        $bottom = [Math]::Min(($screen.Top + $screen.Height), $windowRect.Bottom)
        $captureWidth = $right - $left
        $captureHeight = $bottom - $top

        if ($captureWidth -lt 400 -or $captureHeight -lt 300) {
            throw "The VEYRA window is outside the available desktop capture area."
        }

        $bitmap = New-Object System.Drawing.Bitmap($captureWidth, $captureHeight)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $sourcePoint = New-Object System.Drawing.Point($left, $top)
            $destinationPoint = New-Object System.Drawing.Point(0, 0)
            $captureSize = New-Object System.Drawing.Size($captureWidth, $captureHeight)
            $graphics.CopyFromScreen(
                $sourcePoint, $destinationPoint, $captureSize,
                [System.Drawing.CopyPixelOperation]::SourceCopy
            )
            $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $graphics.Dispose()
            $bitmap.Dispose()
        }

        $screenshotInfo = Get-Item -LiteralPath $screenshotPath
        if ($screenshotInfo.Length -lt 5000) {
            throw "The desktop capture was unexpectedly small ($($screenshotInfo.Length) bytes)."
        }

        $screenshotStatus = "CAPTURED"
        Add-Note "Captured VEYRA window at $($captureWidth)x$($captureHeight)."
        Add-Note "Screenshot size: $($screenshotInfo.Length) bytes."
    }
    catch {
        $screenshotStatus = "UNAVAILABLE"
        Add-Note "Screenshot capture unavailable: $($_.Exception.Message)"
    }
}
catch {
    $launchStatus = "FAILED"
    $failureMessage = $_.Exception.Message
    Add-Note "STARTUP FAILURE: $failureMessage"
}
finally {
    if ($process) {
        try {
            $process.Refresh()
            if (-not $process.HasExited) {
                Add-Note "Closing VEYRA after the test."
                [void]$process.CloseMainWindow()
                if (-not $process.WaitForExit(8000)) {
                    Add-Note "Graceful close timed out; stopping the test process."
                    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                }
            }
        }
        catch {
            Add-Note "Cleanup note: $($_.Exception.Message)"
        }
    }

    $possibleLogs = @(
        (Join-Path $PackagePath "debug.log"),
        (Join-Path $env:LOCALAPPDATA "VEYRA\Profile\debug.log")
    )

    foreach ($candidate in $possibleLogs) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $destination = Join-Path $ResultsPath ([IO.Path]::GetFileName((Split-Path $candidate -Parent)) + "-debug.log")
            try {
                Copy-Item -LiteralPath $candidate -Destination $destination -Force
                Add-Note "Collected CEF diagnostic log: $destination"
            }
            catch {
                Add-Note "Could not copy diagnostic log: $($_.Exception.Message)"
            }
        }
    }
}

$finishedAt = Get-Date
$summary = @(
    "# VEYRA remote Windows preview"
    ""
    "- Started: $($startedAt.ToString('yyyy-MM-dd HH:mm:ss zzz'))"
    "- Finished: $($finishedAt.ToString('yyyy-MM-dd HH:mm:ss zzz'))"
    "- Startup / main window: **$launchStatus**"
    "- Desktop screenshot: **$screenshotStatus**"
    "- Window title: $windowTitle"
    "- Screenshot file: $(if (Test-Path -LiteralPath $screenshotPath) { 'veyra-home.png' } else { 'not available' })"
    ""
    "## Diagnostics"
    ""
    ($notes | ForEach-Object { "- $_" })
)

Set-Content -LiteralPath (Join-Path $ResultsPath "summary.md") -Value $summary -Encoding UTF8
Set-Content -LiteralPath $logPath -Value $notes -Encoding UTF8

Write-Host ($summary -join [Environment]::NewLine)

if ($launchStatus -ne "PASSED") {
    exit 1
}
