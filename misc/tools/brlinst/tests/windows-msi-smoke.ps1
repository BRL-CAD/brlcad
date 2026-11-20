# SPDX-License-Identifier: BSD-2-Clause
param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [switch]$Install,
    [switch]$Machine
)
$ErrorActionPreference = 'Stop'
if (-not [Environment]::Is64BitProcess) { throw 'Run this test with 64-bit PowerShell' }
if ($Machine -and -not $Install) { throw '-Machine requires -Install' }
if ($Machine) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run machine-scope tests from an elevated PowerShell'
    }
}
$build = (Resolve-Path $BuildDirectory).Path
$token = [Guid]::NewGuid().ToString('N').Substring(0, 12)
$root = Join-Path ([IO.Path]::GetTempPath()) "brlinst-msi-test-$token"
$productName = "Brlinst MSI Test $token"
$architecture = switch ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
    'X64' { 'x64' }
    'Arm64' { 'arm64' }
    default { throw "Unsupported Windows architecture: $_" }
}
New-Item -ItemType Directory -Path "$root\payload\bin" -Force | Out-Null
Copy-Item "$env:WINDIR\System32\cmd.exe" "$root\payload\bin\app.exe"
[IO.File]::WriteAllText("$root\payload\data.txt", 'Original data')
[IO.File]::WriteAllText("$root\license.txt", 'MSI test license {braces} [literal brackets].')
$installer = New-Object -ComObject WindowsInstaller.Installer
$installStateUnknown = -1
$installStateDefault = 5
$compressedPackageFlags = 2
function Read-Property($Database, [string]$Name) {
    $view = $Database.OpenView("SELECT ``Value`` FROM ``Property`` WHERE ``Property`` = '$Name'")
    try {
        [void]$view.Execute()
        $record = $view.Fetch()
        if ($null -eq $record) { throw "Missing MSI property $Name" }
        try { return $record.StringData(1) } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($record) }
    } finally { [void]$view.Close(); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($view) }
}
function Invoke-Msi([string[]]$MsiArguments, [int]$Expected = 0) {
    $p = Start-Process -FilePath "$env:WINDIR\System32\msiexec.exe" -ArgumentList $MsiArguments -Wait -PassThru
    if ($p.ExitCode -ne $Expected) { throw "msiexec exited $($p.ExitCode), expected $Expected. Logs: $root" }
}
function Path-Contains([string]$Value, [string]$Entry) {
    return @($Value -split ';' | Where-Object { $_.TrimEnd('\') -ieq $Entry.TrimEnd('\') }).Count -gt 0
}
$packages = @()
$installed = @()
$scope = if ($Machine) { 'Machine' } else { 'User' }
$originalPath = [Environment]::GetEnvironmentVariable('Path', $scope)
$menuRoot = [Environment]::GetFolderPath($(if ($Machine) { 'CommonPrograms' } else { 'Programs' }))
$desktopRoot = [Environment]::GetFolderPath($(if ($Machine) { 'CommonDesktopDirectory' } else { 'DesktopDirectory' }))
try {
    foreach ($version in @('1.0.0', '2.0.0', '1.0.0')) {
        $config = @{
            formatVersion = 1; id = "test.brlinst.msi.$token"; name = $productName; version = $version; architecture = $architecture
            publisher = 'BRL-CAD Development Team'; license = 'license.txt'
            shortcuts = @(@{name = 'Test command'; target = 'bin/app.exe'; arguments = '/c echo [literal]'; desktop = $true})
        } | ConvertTo-Json -Depth 10
        [IO.File]::WriteAllText("$root\product.json", $config, [Text.UTF8Encoding]::new($false))
        $package = "$root\setup-$($packages.Count).msi"
        & "$build\brlinst.exe" --format msi --config "$root\product.json" --source "$root\payload" --output $package
        if ($LASTEXITCODE -ne 0) { throw 'MSI generation failed' }
        & "$build\brlinst.exe" --verify $package
        if ($LASTEXITCODE -ne 0) { throw 'MSI verification failed' }
        $db = $installer.OpenDatabase($package, 0)
        try {
            $code = Read-Property $db 'ProductCode'
            if ((Read-Property $db 'ALLUSERS') -ne '2' -or (Read-Property $db 'MSIINSTALLPERUSER') -ne '1') {
                throw 'Incorrect dual-purpose scope defaults'
            }
            $summary = $db.SummaryInformation(0)
            try {
                $packageCode = $summary.Property(9)
                if ($summary.Property(15) -ne $compressedPackageFlags) {
                    throw 'Incorrect dual-purpose package summary flags'
                }
            } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($summary) }
        } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($db) }
        $packages += @{ File = $package; Code = $code; PackageCode = $packageCode; Version = $version }
    }
    if ($packages[0].Code -eq $packages[1].Code -or $packages[0].Code -ne $packages[2].Code) {
        throw 'Product identities do not distinguish versions consistently'
    }
    if ($packages[0].PackageCode -eq $packages[2].PackageCode) { throw 'Rebuilt packages share a PackageCode' }
    if ($Install) {
        foreach ($package in $packages[0..1]) {
            $folderName = "$productName $($package.Version) ($architecture, MSI)"
            $destination = if ($Machine) { Join-Path $env:ProgramW6432 $folderName } else { Join-Path $root $folderName }
            $scopeArgs = if ($Machine) { @('ALLUSERS=1') } else { @('ALLUSERS=2', 'MSIINSTALLPERUSER=1') }
            $installArguments = @('/i', "`"$($package.File)`"", '/qn', '/norestart', "INSTALLDIR=`"$destination`"",
                'BRLINST_STARTMENU=1', 'BRLINST_DESKTOP=1', 'BRLINST_PATH=1') + $scopeArgs
            # Failure before mutation must preserve an existing destination.
            New-Item -ItemType Directory -Path $destination | Out-Null
            [IO.File]::WriteAllText("$destination\keep.txt", 'Preexisting file')
            Invoke-Msi ($installArguments + @('/l*v', "`"$root\refuse-$($package.Version).log`"")) 1603
            if ([IO.File]::ReadAllText("$destination\keep.txt") -ne 'Preexisting file') { throw 'Preexisting data changed' }
            Remove-Item "$destination\keep.txt"
            $installed += $package.Code # Retain cleanup information even if install fails.
            Invoke-Msi ($installArguments + @('/l*v', "`"$root\install-$($package.Version).log`""))
            Invoke-Msi ($installArguments + @('/l*v', "`"$root\repeat-$($package.Version).log`"")) 1603
            if ((Get-FileHash "$destination\bin\app.exe").Hash -ne (Get-FileHash "$root\payload\bin\app.exe").Hash) { throw 'Installed bytes differ' }
            if ([IO.File]::ReadAllText("$destination\data.txt") -ne 'Original data') { throw 'Unversioned payload differs' }
            if ($installer.ProductState($package.Code) -ne $installStateDefault) {
                throw 'Missing per-version Windows Installer registration'
            }
            $menu = Join-Path $menuRoot "$folderName\Test command.lnk"
            $desktop = Join-Path $desktopRoot "Test command - $folderName.lnk"
            if (-not (Test-Path $menu) -or -not (Test-Path $desktop)) { throw 'Missing MSI shortcuts' }
            $shell = New-Object -ComObject WScript.Shell
            try {
                $link = $shell.CreateShortcut($menu)
                if ($link.Arguments -ne '/c echo [literal]') { throw 'MSI interpreted literal shortcut arguments as properties' }
            } finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) }
            if (-not (Path-Contains ([Environment]::GetEnvironmentVariable('Path', $scope)) "$destination\bin")) { throw 'Missing PATH entry' }
            [IO.File]::WriteAllText("$destination\user-data.txt", 'Keep user data')
            $package.Destination = $destination; $package.Menu = $menu; $package.Desktop = $desktop
        }
        foreach ($package in $packages[0..1]) {
            Invoke-Msi @('/x', $package.Code, '/qn', '/norestart', '/l*v', "`"$root\remove-$($package.Version).log`"")
            $installed = @($installed | Where-Object { $_ -ne $package.Code })
            if ((Test-Path "$($package.Destination)\bin\app.exe") -or (Test-Path $package.Menu) -or
                (Test-Path $package.Desktop) -or $installer.ProductState($package.Code) -ne $installStateUnknown) {
                throw 'MSI left owned resources behind'
            }
            if ([IO.File]::ReadAllText("$($package.Destination)\user-data.txt") -ne 'Keep user data') { throw 'MSI removed unowned data' }
            if (Path-Contains ([Environment]::GetEnvironmentVariable('Path', $scope)) "$($package.Destination)\bin") { throw 'MSI left its PATH entry behind' }
            if ($package.Version -eq '1.0.0' -and -not (Test-Path "$($packages[1].Destination)\bin\app.exe")) { throw 'Uninstall damaged the other version' }
        }
        if ([Environment]::GetEnvironmentVariable('Path', $scope) -ne $originalPath) { throw 'PATH did not return to its original value' }
    }
    Write-Host "MSI checks passed. Packages and logs: $root"
} finally {
    foreach ($code in $installed) {
        try { Invoke-Msi @('/x', $code, '/qn', '/norestart', '/l*v', "`"$root\cleanup-$code.log`"") }
        catch { Write-Warning $_ }
    }
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
}
