param([Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path $BuildDirectory).Path
$token = [Guid]::NewGuid().ToString('N').Substring(0, 12)
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "brlinst-smoke-$token"
$productId = "org.brlcad.brlinst.smoke.$token"
$productName = "Brlinst Smoke $token"
$architecture = switch ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
    'X64' { 'x64' }
    'Arm64' { 'arm64' }
    default { throw "Unsupported Windows architecture: $_" }
}
New-Item -ItemType Directory -Path "$testRoot\payload\bin" -Force | Out-Null
Copy-Item "$env:WINDIR\System32\cmd.exe" "$testRoot\payload\bin\app.exe"
Set-Content "$testRoot\license.txt" 'Installer smoke-test payload.' -Encoding UTF8
Set-Content "$testRoot\payload\data.txt" 'Original package data.' -Encoding UTF8
$originalPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$installs = @()
function Invoke-Setup([string]$Exe, [string[]]$Arguments, [int]$Expected = 0) {
    $p = Start-Process -FilePath $Exe -ArgumentList $Arguments -Wait -PassThru
    if ($p.ExitCode -ne $Expected) { throw "$Exe exited $($p.ExitCode), expected $Expected. Logs: $testRoot" }
}
function Wait-Removed([string]$Path) {
    $deadline = (Get-Date).AddSeconds(30)
    while ((Test-Path $Path) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path $Path) { throw "Cleanup did not remove $Path" }
}
try {
    foreach ($version in @('1.0.0', '2.0.0')) {
        $config = @{
            formatVersion = 1; id = $productId; name = $productName; version = $version; architecture = $architecture
            publisher = 'BRL-CAD Development Team'; license = 'license.txt'
            shortcuts = @(@{name = 'Test command'; target = 'bin/app.exe'; desktop = $true})
        } | ConvertTo-Json -Depth 10
        [IO.File]::WriteAllText("$testRoot\product.json", $config, [Text.UTF8Encoding]::new($false))
        $setup = "$testRoot\setup-$version.exe"
        & "$build\brlinst.exe" --config "$testRoot\product.json" --source "$testRoot\payload" --output $setup
        if ($LASTEXITCODE -ne 0) { throw 'Package generation failed' }
        & "$build\brlinst.exe" --verify $setup
        if ($LASTEXITCODE -ne 0) { throw 'Package verification failed' }
        $install = "$testRoot\Installed $version"
        $installs += $install
        $arguments = @('--quiet', '--scope', 'user', '--directory', "`"$install`"", '--start-menu', 'on', '--desktop', 'on', '--path', 'on', '--log', "`"$testRoot\install-$version.log`"")
        Invoke-Setup $setup $arguments
        Invoke-Setup $setup $arguments 3
        $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Brlinst_$productId!$version!$architecture"
        if (-not (Test-Path $key)) { throw 'Uninstall registration missing' }
        if ((Get-ItemProperty $key).InstallLocation -ne $install) { throw 'Wrong registration location' }
        $menu = Join-Path ([Environment]::GetFolderPath('Programs')) "$productName $version ($architecture, current user)\Test command.lnk"
        if (-not (Test-Path $menu)) { throw 'Versioned shortcut missing' }
        $shell = New-Object -ComObject WScript.Shell
        if ($shell.CreateShortcut($menu).TargetPath -ne "$install\bin\app.exe") { throw 'Shortcut target mismatch' }
        if (-not ([Environment]::GetEnvironmentVariable('Path','User').Split(';') -contains "$install\bin")) { throw 'PATH entry missing' }
    }
    $locked = [IO.File]::Open("$($installs[0])\bin\app.exe", [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        Invoke-Setup "$($installs[0])\.brlinst\uninstall.exe" @('--quiet', '--log', "`"$testRoot\locked.log`"") 6
        if (-not (Test-Path "$($installs[0])\.brlinst\uninstall.exe")) { throw 'Failed removal lost its uninstaller' }
    } finally { $locked.Dispose() }
    Set-Content "$($installs[0])\data.txt" 'User-modified data.' -Encoding UTF8
    Set-Content "$($installs[0])\user-file.txt" 'Keep this.' -Encoding UTF8
    Invoke-Setup "$($installs[0])\.brlinst\uninstall.exe" @('--quiet', '--log', "`"$testRoot\uninstall-1.log`"")
    Wait-Removed "$($installs[0])\.brlinst"
    if (-not (Test-Path "$($installs[0])\data.txt") -or -not (Test-Path "$($installs[0])\user-file.txt")) { throw 'Uninstall removed user data' }
    if (-not (Test-Path "$($installs[1])\bin\app.exe")) { throw 'Uninstall affected another version' }
    Invoke-Setup "$($installs[1])\.brlinst\uninstall.exe" @('--quiet', '--log', "`"$testRoot\uninstall-2.log`"")
    Wait-Removed $installs[1]
    if ([Environment]::GetEnvironmentVariable('Path', 'User') -cne $originalPath) { throw 'PATH was not restored' }
    Write-Host "Windows smoke test passed. Logs and preserved test files: $testRoot"
} finally {
    # Retry only this test's owned uninstallers. Do not recursively delete a
    # failed installation or replace PATH with an old snapshot.
    foreach ($install in $installs) {
        $uninstaller = "$install\.brlinst\uninstall.exe"
        if (Test-Path $uninstaller) {
            Start-Process -FilePath $uninstaller -ArgumentList '--quiet' -Wait | Out-Null
        }
    }
}
