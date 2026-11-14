# Windows PowerShell 5.1 compatible. Discovery and planning functions are read-only.
Set-StrictMode -Version Latest

function Read-JsonFile([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        try { return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json } catch { return $null }
    }
    return $null
}

function Test-VersionRange([version]$Version, [string]$Range) {
    $ends = $Range.Split('-')
    return $Version -ge [version]$ends[0] -and $Version -le [version]$ends[-1]
}

function Get-EngineInfo([string]$Root, [string]$Association) {
    if (!$Root -or !(Test-Path -LiteralPath $Root -PathType Container)) { return }
    $Root = (Resolve-Path -LiteralPath $Root).Path
    $version = Read-JsonFile (Join-Path $Root 'Engine/Build/Build.version')
    if (!$version) { return }
    $release = '{0}.{1}' -f $version.MajorVersion, $version.MinorVersion
    if ($Association -match '^\d+\.\d+$' -and $release -ne $Association) { return }
    foreach ($file in @('Engine/Build/BatchFiles/Build.bat', 'Engine/Binaries/Win64/UnrealEditor.exe', 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')) {
        if (!(Test-Path -LiteralPath (Join-Path $Root $file) -PathType Leaf)) { return }
    }
    [pscustomobject]@{ Root = $Root; Version = $release; Patch = [int]$version.PatchVersion }
}

function Find-UnrealEngine([string]$Association, [string]$PreferredRoot, [string[]]$AdditionalRoots = @()) {
    $roots = [Collections.Generic.List[string]]::new()
    if ($PreferredRoot) { $roots.Add($PreferredRoot) }
    foreach ($root in $AdditionalRoots) { $roots.Add($root) }
    $manifest = Read-JsonFile (Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat')
    if ($manifest -and $manifest.PSObject.Properties['InstallationList']) {
        foreach ($install in $manifest.InstallationList) { $roots.Add([string]$install.InstallLocation) }
    }
    foreach ($key in @('HKLM:/SOFTWARE/EpicGames/Unreal Engine', 'HKLM:/SOFTWARE/WOW6432Node/EpicGames/Unreal Engine')) {
        foreach ($entry in @(Get-ChildItem $key -ErrorAction SilentlyContinue)) {
            $value = Get-ItemProperty $entry.PSPath -ErrorAction SilentlyContinue
            if ($value -and $value.PSObject.Properties['InstalledDirectory']) { $roots.Add([string]$value.InstalledDirectory) }
        }
    }
    $builds = Get-ItemProperty 'HKCU:/Software/Epic Games/Unreal Engine/Builds' -ErrorAction SilentlyContinue
    if ($builds) {
        foreach ($property in $builds.PSObject.Properties) {
            if ($property.Name -notlike 'PS*' -and ($Association -match '^\d+\.\d+$' -or $property.Name -eq $Association)) { $roots.Add([string]$property.Value) }
        }
    }
    $bases = @((Join-Path $env:ProgramFiles 'Epic Games'))
    foreach ($drive in @(Get-PSDrive -PSProvider FileSystem)) {
        foreach ($suffix in @('Unreal', 'Epic Games', 'Games', 'Program Files/Epic Games')) { $bases += Join-Path $drive.Root $suffix }
    }
    foreach ($base in $bases) {
        foreach ($directory in @(Get-ChildItem -LiteralPath $base -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue)) { $roots.Add($directory.FullName) }
    }
    $found = @($roots | Select-Object -Unique | ForEach-Object { Get-EngineInfo $_ $Association })
    if ($PreferredRoot) {
        $preferred = @($found | Where-Object { $_.Root.TrimEnd('\', '/') -ieq $PreferredRoot.TrimEnd('\', '/') })
        if ($preferred.Count) { return $preferred[0] }
    }
    return $found | Sort-Object Patch -Descending | Select-Object -First 1
}

function Get-WindowsToolchain([string]$EngineRoot, [string[]]$InstallRoots = @(), [string]$KitsRoot = '') {
    $rules = Read-JsonFile (Join-Path $EngineRoot 'Engine/Config/Windows/Windows_SDK.json')
    if (!$rules) { throw 'Engine Windows_SDK.json is missing or invalid. Verify this engine in Epic Launcher.' }
    if (!$InstallRoots.Count) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
            $InstallRoots = @($instances | ForEach-Object { $_.installationPath })
        }
    }
    $compilers = @(
        foreach ($root in $InstallRoots) {
            foreach ($folder in @(Get-ChildItem -LiteralPath (Join-Path $root 'VC/Tools/MSVC') -Directory -ErrorAction SilentlyContinue)) {
                $cl = Join-Path $folder.FullName 'bin/Hostx64/x64/cl.exe'
                if (!(Test-Path -LiteralPath $cl)) { continue }
                # Folder names can stay at 35207 even when the compiler was patched to 35219.
                $actual = (Get-Item -LiteralPath $cl).VersionInfo.FileVersion
                if ($actual -notmatch '(\d+)\.(\d+)\.(\d+)') { continue }
                $version = [version]('14.{0}.{1}' -f $Matches[2], $Matches[3])
                $banned = @($rules.BannedVisualCppVersions | Where-Object { Test-VersionRange $version $_ }).Count -gt 0
                if ($version -ge [version]$rules.MinimumVisualCppVersion -and !$banned) {
                    [pscustomobject]@{ Version = $version.ToString(); InstallRoot = $root; CompilerPath = $cl; Folder = $folder.Name }
                }
            }
        }
    )
    if (!$KitsRoot) { $KitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10' }
    $sdks = @(
        foreach ($dir in @(Get-ChildItem -LiteralPath (Join-Path $KitsRoot 'Include') -Directory -ErrorAction SilentlyContinue)) {
            $ver = $null
            if (![version]::TryParse($dir.Name, [ref]$ver)) { continue }
            if ($ver -lt [version]$rules.MinVersion -or $ver -gt [version]$rules.MaxVersion) { continue }
            if ((Test-Path -LiteralPath (Join-Path $dir.FullName 'um/Windows.h')) -and
                (Test-Path -LiteralPath (Join-Path $KitsRoot ('Lib/{0}/um/x64/kernel32.lib' -f $dir.Name))) -and
                (Test-Path -LiteralPath (Join-Path $KitsRoot ('bin/{0}/x64/rc.exe' -f $dir.Name)))) { $ver }
        }
    )
    [pscustomobject]@{
        Compiler = $compilers | Sort-Object { [version]$_.Version } -Descending | Select-Object -First 1
        SDK = $sdks | Sort-Object -Descending | Select-Object -First 1
        Rules = $rules
    }
}

function Get-BuildFingerprint([string]$ProjectRoot, [string]$EngineRoot, [string]$Toolchain) {
    $paths = @((Join-Path $ProjectRoot 'shooter.uproject'), (Join-Path $EngineRoot 'Engine/Build/Build.version'))
    foreach ($folder in @('Source', 'Config', 'Plugins')) {
        $base = Join-Path $ProjectRoot $folder
        if (Test-Path -LiteralPath $base) {
            $paths += Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object {
                $_.FullName -notmatch '[\\/](Intermediate|Binaries|Saved|DerivedDataCache)[\\/]' -and
                $_.Extension -in @('.cpp', '.c', '.cc', '.cxx', '.h', '.hpp', '.inl', '.cs', '.ini', '.uplugin', '.lib', '.dll', '.usf', '.ush')
            } | ForEach-Object { $_.FullName }
        }
    }
    $lines = @($ProjectRoot, $EngineRoot, $Toolchain)
    foreach ($path in ($paths | Sort-Object -Unique)) {
        if (Test-Path -LiteralPath $path) { $lines += $path + ':' + (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines -join "`n"))).Replace('-', '') }
    finally { $sha.Dispose() }
}

function Test-BuildCurrent([string]$ProjectRoot, [string]$Fingerprint, [string]$StatePath) {
    $state = Read-JsonFile $StatePath
    if (!$state -or !$state.PSObject.Properties['Fingerprint'] -or $state.Fingerprint -ne $Fingerprint) { return $false }
    foreach ($path in @('Binaries/Win64/UnrealEditor-shooter.dll', 'Binaries/Win64/UnrealEditor.modules', 'Binaries/Win64/shooterEditor.target')) {
        if (!(Test-Path -LiteralPath (Join-Path $ProjectRoot $path))) { return $false }
    }
    $dll = Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'Binaries/Win64/UnrealEditor-shooter.dll') -Algorithm SHA256
    return $state.PSObject.Properties['BinaryHash'] -and $state.BinaryHash -eq $dll.Hash
}

function Assert-ProjectClosed([string]$ProjectRoot) {
    $dll = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Binaries/Win64/UnrealEditor-shooter.dll'))
    foreach ($process in @(Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue)) {
        $loaded = $false
        try { $loaded = @($process.Modules | Where-Object { $_.FileName -ieq $dll }).Count -gt 0 } catch { continue }
        if ($loaded) { throw "This project is open in Unreal (PID $($process.Id)). Save your work and close its editor/game, then retry. No processes were stopped." }
    }
}

function Get-LfsPointers([string]$ContentRoot) {
    if (!(Test-Path -LiteralPath $ContentRoot)) { return }
    foreach ($file in @(Get-ChildItem -LiteralPath $ContentRoot -Recurse -File | Where-Object { $_.Length -lt 1024 -and $_.Extension -in @('.umap', '.uasset', '.ubulk') })) {
        $firstLine = Get-Content -LiteralPath $file.FullName -TotalCount 1
        if ($firstLine -eq 'version https://git-lfs.github.com/spec/v1') { $file.FullName }
    }
}

function ConvertTo-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]' -and $Value.Length) { return $Value }
    return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}

function Get-SignedInstaller([string]$Url, [string]$Destination, [string]$Publisher = 'Microsoft Corporation') {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    foreach ($attempt in 1..3) {
        if (!(Test-Path -LiteralPath $Destination)) {
            try { Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing -TimeoutSec 60 }
            catch { if ($attempt -eq 3) { throw "Download failed: $Url. Check your connection and rerun. $($_.Exception.Message)" }; Start-Sleep -Seconds 2; continue }
        }
        $signature = Get-AuthenticodeSignature -LiteralPath $Destination
        if ($signature.Status -eq 'Valid' -and $signature.SignerCertificate.Subject -match $Publisher) { return $Destination }
        Remove-Item -LiteralPath $Destination -Force
    }
    throw 'Installer signature verification failed; no installer was executed.'
}

function Invoke-ElevatedInstaller([string]$File, [string[]]$Arguments) {
    $argumentLine = ($Arguments | ForEach-Object { ConvertTo-NativeArgument $_ }) -join ' '
    foreach ($attempt in 1..2) {
        $process = Start-Process -FilePath $File -ArgumentList $argumentLine -Verb RunAs -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -eq 0) { return }
        if ($process.ExitCode -eq 3010) { throw 'Installation completed. Windows requires a restart; restart and run Launch.bat again.' }
        if ($process.ExitCode -eq 1618 -and $attempt -eq 1) { Write-Host 'Another installer is running. Retrying in five seconds...' -ForegroundColor Yellow; Start-Sleep -Seconds 5; continue }
        throw "Installer exited with code $($process.ExitCode). Check Visual Studio Installer; then rerun. UAC cancellation makes no project changes."
    }
}

function Install-Toolchain([string]$EngineRoot, [string]$DownloadRoot) {
    $status = Get-WindowsToolchain $EngineRoot
    if ($status.Compiler -and $status.SDK) { Write-Host '  C++ compiler and Windows SDK already ready.' -ForegroundColor Green; return $status }
    $rules = $status.Rules
    $components = @('Microsoft.VisualStudio.Workload.VCTools')
    $components += @($rules.VisualStudio2022SuggestedComponents | Where-Object { $_ -like '*x86.x64' })
    $components += @($rules.VisualStudioSuggestedComponents | Where-Object { $_ -like '*Windows*SDK*' })
    $installer = Get-SignedInstaller 'https://aka.ms/vs/17/release/vs_BuildTools.exe' (Join-Path $DownloadRoot 'vs_BuildTools.exe')
    $arguments = @('--wait', '--passive', '--norestart')
    # Add components to an existing VS 2022 installation instead of installing a duplicate.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $instances = @(& $vswhere -all -products '*' -version '[17.0,18.0)' -format json | ConvertFrom-Json)
        if ($instances.Count) {
            $arguments = @('modify', '--installPath', $instances[0].installationPath, '--channelId', 'VisualStudio.17.Release') + $arguments
            if ($instances[0].productId -ne 'Microsoft.VisualStudio.Product.BuildTools') {
                $components = @($components | Where-Object { $_ -ne 'Microsoft.VisualStudio.Workload.VCTools' }) + 'Microsoft.VisualStudio.Workload.NativeDesktop'
            }
        }
    }
    foreach ($component in ($components | Select-Object -Unique)) { $arguments += @('--add', $component) }
    Write-Host '  Installing required C++ tools (Windows may ask for administrator approval)...' -ForegroundColor Cyan
    Invoke-ElevatedInstaller $installer $arguments
    $status = Get-WindowsToolchain $EngineRoot
    if (!$status.Compiler -or !$status.SDK) { throw 'No supported compiler/SDK detected after installation. Update Visual Studio 2022, then rerun.' }
    return $status
}

function Invoke-NativeLogged([string]$File, [string[]]$Arguments, [string]$LogPath, [string]$Activity) {
    Write-Host "  $Activity" -ForegroundColor Cyan
    Write-Progress -Activity $Activity -Status 'Working; full output is being saved to the log.'
    $ErrorActionPreference = 'Continue'
    & $File @Arguments 2>&1 | Tee-Object -FilePath $LogPath | ForEach-Object {
        $line = $_.ToString()
        if ($line -match 'error|failed|succeeded|up to date|\[\d+/\d+\]|DEMO_CREATED|Test Completed') { Write-Host "    $line" }
    }
    $code = $LASTEXITCODE
    Write-Progress -Activity $Activity -Completed
    if ($code -ne 0) { throw "$Activity failed (exit $code). Full log: $LogPath" }
}

function Assert-GameplayReport([string]$ReportPath) {
    $report = Read-JsonFile $ReportPath
    if (!$report) { throw "Unreal did not produce a valid gameplay report: $ReportPath" }
    if ($report.failed -gt 0 -or $report.notRun -gt 0 -or $report.inProcess -gt 0 -or ($report.succeeded + $report.succeededWithWarnings) -eq 0) {
        throw "Gameplay checks failed or did not complete. Report: $ReportPath"
    }
    return $report.succeeded + $report.succeededWithWarnings
}

Export-ModuleMember -Function *
