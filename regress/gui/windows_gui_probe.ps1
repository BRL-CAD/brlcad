param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
Start-Transcript -Path (Join-Path $ArtifactDirectory 'diagnostics.txt') -Force | Out-Null

$form = $null
$bitmap = $null
$graphics = $null
try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class ProbeWindow
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
}
'@

    $screen = [System.Windows.Forms.Screen]::PrimaryScreen
    if ($null -eq $screen) {
        throw 'No primary screen is available.'
    }

    $bounds = $screen.Bounds
    Write-Host "Interactive session: $([Environment]::UserInteractive)"
    Write-Host "Session ID: $([Diagnostics.Process]::GetCurrentProcess().SessionId)"
    Write-Host "Primary screen: $bounds"

    $windowTitle = 'BRL-CAD Windows GUI probe'
    $windowWidth = 320
    $windowHeight = 200
    $paintDelayMilliseconds = 1000
    $colorTolerance = 16
    $expectedColor = [System.Drawing.Color]::FromArgb(224, 48, 160)

    $form = [System.Windows.Forms.Form]::new()
    $form.Text = $windowTitle
    $form.ClientSize = [System.Drawing.Size]::new($windowWidth, $windowHeight)
    $form.StartPosition = 'Manual'
    $form.Location = [System.Drawing.Point]::new(
        $bounds.Left + [int](($bounds.Width - $form.Width) / 2),
        $bounds.Top + [int](($bounds.Height - $form.Height) / 2))
    $form.BackColor = $expectedColor
    $form.TopMost = $true
    $form.Show()
    $form.Activate()
    [System.Windows.Forms.Application]::DoEvents()

    $foundWindow = [ProbeWindow]::FindWindow($null, $windowTitle)
    $windowFound = $foundWindow -ne [IntPtr]::Zero -and $foundWindow -eq $form.Handle
    Write-Host "Window discoverable: $windowFound"

    Start-Sleep -Milliseconds $paintDelayMilliseconds
    [System.Windows.Forms.Application]::DoEvents()
    $bitmap = [System.Drawing.Bitmap]::new($bounds.Width, $bounds.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $capturePath = Join-Path $ArtifactDirectory 'desktop.png'
    $bitmap.Save($capturePath, [System.Drawing.Imaging.ImageFormat]::Png)

    $sampleX = $form.Left + [int]($form.Width / 2) - $bounds.Left
    $sampleY = $form.Top + [int]($form.Height / 2) - $bounds.Top
    $observedColor = $bitmap.GetPixel($sampleX, $sampleY)
    Write-Host "Expected window color: $expectedColor"
    Write-Host "Captured window color: $observedColor"
    if (-not $windowFound) {
        throw 'The probe window could not be found by title.'
    }
    if ([Math]::Abs($observedColor.R - $expectedColor.R) -gt $colorTolerance -or
        [Math]::Abs($observedColor.G - $expectedColor.G) -gt $colorTolerance -or
        [Math]::Abs($observedColor.B - $expectedColor.B) -gt $colorTolerance) {
        throw 'The screen capture does not contain the probe window.'
    }

    Write-Host 'PASS: the native window is discoverable and visible in the screen capture.'
} catch {
    Write-Host "FAIL: $($_.Exception.Message)"
    throw
} finally {
    if ($null -ne $graphics) { $graphics.Dispose() }
    if ($null -ne $bitmap) { $bitmap.Dispose() }
    if ($null -ne $form) {
        $form.Close()
        $form.Dispose()
    }
    Stop-Transcript | Out-Null
}
