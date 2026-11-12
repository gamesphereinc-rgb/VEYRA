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
$screenshotPath = Join-Path $ResultsPath "veyra-home.png"
$logPath = Join-Path $ResultsPath "startup-diagnostics.txt"

function Add-Note([string]$Message) {
    $notes.Add(("{0:HH:mm:ss} {1}" -f (Get-Date), $Message))
}

try {
    $executables = @(Get-ChildItem -LiteralPath $PackagePath -Filter "VEYRA.exe" -File -Recurse)
    if ($executables.Count -eq 0) {
        throw "VEYRA.exe was not found anywhere in the downloaded Windows build artifact."
    }
    if ($executables.Count -gt 1) {
        throw "The build artifact contains multiple VEYRA.exe files; refusing to guess which package to launch."
    }

    $exePath = $executables[0].FullName
    $PackagePath = $executables[0].Directory.FullName
    Add-Note "Found packaged executable: $exePath"
    Add-Note "Using executable directory as working directory: $PackagePath"
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

        [DllImport("user32.dll", SetLastError = true)]
        public static extern bool SetCursorPos(int x, int y);

        [DllImport("user32.dll")]
        public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
    }
}
"@ | Out-Null

        function Get-VeyraWindowRect {
            $rect = New-Object VeyraRemotePreview.WindowRect
            if (-not [VeyraRemotePreview.NativeMethods]::GetWindowRect($windowHandle, [ref]$rect)) {
                throw "Windows could not read the VEYRA window bounds."
            }
            return $rect
        }

        function Save-VeyraWindowScreenshot([string]$FileName) {
            $rect = Get-VeyraWindowRect
            $screenBounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
            $left = [Math]::Max($screenBounds.Left, $rect.Left)
            $top = [Math]::Max($screenBounds.Top, $rect.Top)
            $right = [Math]::Min(($screenBounds.Left + $screenBounds.Width), $rect.Right)
            $bottom = [Math]::Min(($screenBounds.Top + $screenBounds.Height), $rect.Bottom)
            $width = $right - $left
            $height = $bottom - $top

            if ($width -lt 400 -or $height -lt 300) {
                throw "The VEYRA window is outside the available desktop capture area."
            }

            $path = Join-Path $ResultsPath $FileName
            $bitmap = New-Object System.Drawing.Bitmap($width, $height)
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $sourcePoint = New-Object System.Drawing.Point($left, $top)
                $destinationPoint = New-Object System.Drawing.Point(0, 0)
                $captureSize = New-Object System.Drawing.Size($width, $height)
                $graphics.CopyFromScreen(
                    $sourcePoint, $destinationPoint, $captureSize,
                    [System.Drawing.CopyPixelOperation]::SourceCopy
                )
                $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $graphics.Dispose()
                $bitmap.Dispose()
            }

            $info = Get-Item -LiteralPath $path
            if ($info.Length -lt 5000) {
                throw "Screenshot $FileName was unexpectedly small ($($info.Length) bytes)."
            }
            Add-Note "Captured $FileName at ${width}x${height} ($($info.Length) bytes)."
        }

        $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $targetWidth = [Math]::Min(1100, [Math]::Max(720, $screen.Width - 40))
        # Leave the taskbar and its reserved area outside the browser window.
        $targetHeight = [Math]::Min(680, [Math]::Max(560, $screen.Height - 110))
        [void][VeyraRemotePreview.NativeMethods]::ShowWindow($windowHandle, 9)
        [void][VeyraRemotePreview.NativeMethods]::MoveWindow(
            $windowHandle, $screen.Left + 10, $screen.Top + 10,
            $targetWidth, $targetHeight, $true
        )
        [void][VeyraRemotePreview.NativeMethods]::SetForegroundWindow($windowHandle)
        Start-Sleep -Seconds 3
        Save-VeyraWindowScreenshot "veyra-home.png"
        $screenshotStatus = "HOME CAPTURED"
        
        # Click VEYRA's own new-tab Settings button using its stable topbar position.
        # Coordinates are relative to the right edge because the page header is right-aligned.
        $windowRect = Get-VeyraWindowRect
        $settingsX = $windowRect.Right - 247
        $settingsY = $windowRect.Top + 161
        if ($settingsX -le $windowRect.Left -or $settingsY -ge $windowRect.Bottom) {
            throw "The Settings button position falls outside the VEYRA window."
        }
        [void][VeyraRemotePreview.NativeMethods]::SetCursorPos($settingsX, $settingsY)
        [VeyraRemotePreview.NativeMethods]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
        [VeyraRemotePreview.NativeMethods]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
        Add-Note "Clicked the new-tab Settings control."
        Start-Sleep -Seconds 2
        Save-VeyraWindowScreenshot "veyra-settings-appearance.png"

        # Scroll within the settings content to reveal the About & updates card.
        $scrollX = $windowRect.Left + [int](($windowRect.Right - $windowRect.Left) * 0.68)
        $scrollY = $windowRect.Top + [int](($windowRect.Bottom - $windowRect.Top) * 0.72)
        [void][VeyraRemotePreview.NativeMethods]::SetCursorPos($scrollX, $scrollY)
        $wheelData = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]-850), 0)
        [VeyraRemotePreview.NativeMethods]::mouse_event(0x0800, 0, 0, $wheelData, [UIntPtr]::Zero)
        Add-Note "Scrolled settings content to reveal lower sections."
        Start-Sleep -Seconds 2
        Save-VeyraWindowScreenshot "veyra-settings-lower.png"
        $screenshotStatus = "HOME AND SETTINGS CAPTURED"
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
    "- Screenshots: veyra-home.png, veyra-settings-appearance.png, veyra-settings-lower.png (when capture succeeds)"
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
