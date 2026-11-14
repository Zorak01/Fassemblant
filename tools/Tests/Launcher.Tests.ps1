# Dependency-free tests: no installers, downloads, registry writes or real builds.
#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Launcher.Core.psm1') -Force -DisableNameChecking
$script:passed = 0
function Assert([bool]$Condition, [string]$Name) {
    if (!$Condition) { throw "FAIL: $Name" }
    $script:passed++
    Write-Host "  PASS $Name" -ForegroundColor Green
}
function Put([string]$Path, [string]$Text) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [IO.File]::WriteAllText($Path, $Text)
}
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('Fassemblant-LauncherTests-' + [guid]::NewGuid().ToString('N'))
try {
    $engine = Join-Path $fixture 'Engine with spaces & brackets [test]'
    $project = Join-Path $fixture "Friend's project [demo]"
    Put (Join-Path $engine 'Engine/Build/Build.version') '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":4}'
    foreach ($file in @('Engine/Build/BatchFiles/Build.bat', 'Engine/Binaries/Win64/UnrealEditor.exe', 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe')) { Put (Join-Path $engine $file) 'fixture' }
    Assert ($null -ne (Get-EngineInfo $engine '5.8')) 'Discovers a matching installation with spaces and special characters'
    Assert ($null -eq (Get-EngineInfo $engine '5.9')) 'Does not silently substitute a different engine version'
    Assert ($null -eq (Get-EngineInfo (Join-Path $fixture 'Missing') '5.8')) 'Missing engine returns a recoverable empty result'
    Put (Join-Path $engine 'Engine/Build/Build.version') 'corrupt json'
    Assert ($null -eq (Get-EngineInfo $engine '5.8')) 'Corrupt engine metadata is rejected'
    Put (Join-Path $engine 'Engine/Build/Build.version') '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":4}'
    Assert (Test-VersionRange ([version]'14.44.35207') '14.44.0-14.44.35210') 'Rejects known-bad MSVC revision'
    Assert (!(Test-VersionRange ([version]'14.44.35219') '14.44.0-14.44.35210')) 'Accepts patched compiler despite old folder name'
    Assert (Test-VersionRange ([version]'14.44.35210') '14.44.0-14.44.35210') 'Version rejection includes upper boundary'
    Put (Join-Path $project 'shooter.uproject') '{"EngineAssociation":"5.8"}'
    Put (Join-Path $project 'Source/demo.cpp') 'source v1'
    Put (Join-Path $project 'Config/DefaultInput.ini') 'input v1'
    $first = Get-BuildFingerprint $project $engine '14.44.35219|10.0.22621.0'
    Assert ($first -eq (Get-BuildFingerprint $project $engine '14.44.35219|10.0.22621.0')) 'Unchanged checkout produces identical fingerprint'
    Put (Join-Path $project 'Saved/Logs/log.txt') 'unrelated output'
    Put (Join-Path $project 'Source/Intermediate/noise.cpp') 'generated noise'
    Assert ($first -eq (Get-BuildFingerprint $project $engine '14.44.35219|10.0.22621.0')) 'Generated caches and logs do not invalidate the build'
    $state = Join-Path $project 'Saved/state.json'
    Assert (!(Test-BuildCurrent $project $first $state)) 'First checkout requires build'
    foreach ($file in @('Binaries/Win64/UnrealEditor-shooter.dll', 'Binaries/Win64/UnrealEditor.modules', 'Binaries/Win64/shooterEditor.target')) { Put (Join-Path $project $file) 'built output' }
    $dll = Join-Path $project 'Binaries/Win64/UnrealEditor-shooter.dll'
    Put $state (@{Fingerprint=$first; BinaryHash=(Get-FileHash -LiteralPath $dll).Hash} | ConvertTo-Json)
    Assert (Test-BuildCurrent $project $first $state) 'Ready checkout skips build'
    Put (Join-Path $project 'Source/demo.cpp') 'source v2'
    $second = Get-BuildFingerprint $project $engine '14.44.35219|10.0.22621.0'
    Assert ($first -ne $second) 'Source changes invalidate build'
    Assert (!(Test-BuildCurrent $project $second $state)) 'Stale manifest cannot skip a required build'
    Put (Join-Path $project 'Source/demo.cpp') 'source v1'
    Put (Join-Path $project 'Config/DefaultInput.ini') 'input v2'
    Assert ($first -ne (Get-BuildFingerprint $project $engine '14.44.35219|10.0.22621.0')) 'Configuration changes invalidate build'
    Put (Join-Path $project 'Config/DefaultInput.ini') 'input v1'
    Assert ($first -ne (Get-BuildFingerprint $project $engine '14.44.35220|10.0.22621.0')) 'Compiler/SDK changes invalidate build'
    Put $dll 'externally replaced binary'
    Assert (!(Test-BuildCurrent $project $first $state)) 'Replaced binary cannot be trusted by old manifest'
    Remove-Item -LiteralPath $dll
    Assert (!(Test-BuildCurrent $project $first $state)) 'Deleted binaries trigger recovery build'
    Put $state '{broken'
    Assert (!(Test-BuildCurrent $project $first $state)) 'Interrupted state write recovers by rebuilding'
    $content = Join-Path $project 'Content/demo'
    Put (Join-Path $content 'Missing.umap') "version https://git-lfs.github.com/spec/v1`noid sha256:test`nsize 4000"
    Put (Join-Path $content 'Present.uasset') 'binary-like test asset'
    Assert (@(Get-LfsPointers $content).Count -eq 1) 'Detects LFS placeholders without confusing real assets'
    Assert ((ConvertTo-NativeArgument 'C:\Friend Project\shooter.uproject') -eq '"C:\Friend Project\shooter.uproject"') 'Launch arguments preserve paths with spaces'
    Assert ((ConvertTo-NativeArgument 'C:\trailing space\') -eq '"C:\trailing space\\"') 'Native quoting preserves trailing slash'
    Assert ((ConvertTo-NativeArgument '') -eq '""') 'Empty argument remains an argument'
    $report = Join-Path $fixture 'report.json'
    Put $report '{"succeeded":3,"succeededWithWarnings":0,"failed":0,"notRun":0,"inProcess":0}'
    Assert ((Assert-GameplayReport $report) -eq 3) 'Checks actual Unreal report instead of trusting process exit code'
    Put $report '{"succeeded":2,"succeededWithWarnings":0,"failed":1,"notRun":0,"inProcess":0}'
    $rejected = $false
    try { Assert-GameplayReport $report | Out-Null } catch { $rejected = $true }
    Assert $rejected 'Failed gameplay test makes launcher fail even if Unreal exits zero'
    Put $report '{"succeeded":0,"succeededWithWarnings":0,"failed":0,"notRun":0,"inProcess":0}'
    $rejected = $false
    try { Assert-GameplayReport $report | Out-Null } catch { $rejected = $true }
    Assert $rejected 'Zero tests is not a successful test run'
    $module = Get-Module Launcher.Core
    $installerResults = & $module {
        param($Fixture)
        $script:InstallerCodes = [Collections.Generic.Queue[int]]::new()
        $script:InstallerCalls = 0
        function script:Start-Process {
            param($FilePath, $ArgumentList, $Verb, $WindowStyle, [switch]$Wait, [switch]$PassThru)
            $script:InstallerCalls++
            [pscustomobject]@{ ExitCode = $script:InstallerCodes.Dequeue() }
        }
        function script:Start-Sleep { param($Seconds) }
        $script:InstallerCodes.Enqueue(0)
        Invoke-ElevatedInstaller 'mock-installer.exe' @('--installPath', 'C:\Example Path')
        $success = $script:InstallerCalls -eq 1
        $script:InstallerCalls = 0
        $script:InstallerCodes.Enqueue(1618); $script:InstallerCodes.Enqueue(0)
        Invoke-ElevatedInstaller 'mock-installer.exe' @('--passive')
        $recovered = $script:InstallerCalls -eq 2
        $script:InstallerCodes.Enqueue(3010)
        $reboot = $false
        try { Invoke-ElevatedInstaller 'mock-installer.exe' @('--passive') } catch { $reboot = $_.Exception.Message -like '*restart*' }
        $script:InstallerCodes.Enqueue(1603)
        $failed = $false
        try { Invoke-ElevatedInstaller 'mock-installer.exe' @('--passive') } catch { $failed = $_.Exception.Message -like '*1603*' }
        [pscustomobject]@{ Success=$success; Recovered=$recovered; Reboot=$reboot; Failed=$failed }
    } $fixture
    Assert $installerResults.Success 'Installer success does not cause another installation'
    Assert $installerResults.Recovered 'Busy Windows installer is retried once'
    Assert $installerResults.Reboot 'Restart-required code produces actionable restart message'
    Assert $installerResults.Failed 'Installer failure stops setup with its error code'
    # Restore real commands after the isolated installer mock tests.
    Import-Module (Join-Path $PSScriptRoot '../Launcher.Core.psm1') -Force -DisableNameChecking
    foreach ($scriptPath in @((Join-Path $PSScriptRoot '../Dev.ps1'), (Join-Path $PSScriptRoot '../Launcher.Core.psm1'))) {
        $tokens = $null; $parseErrors = $null
        [Management.Automation.Language.Parser]::ParseFile((Resolve-Path $scriptPath), [ref]$tokens, [ref]$parseErrors) | Out-Null
        Assert ($parseErrors.Count -eq 0) "PowerShell parser accepts $(Split-Path $scriptPath -Leaf)"
    }
    Write-Host "`n  $script:passed launcher checks passed." -ForegroundColor Cyan
} finally {
    # Delete only this run's uniquely named, resolved fixture directory.
    if (Test-Path -LiteralPath $fixture) {
        $resolved = (Resolve-Path -LiteralPath $fixture).Path
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or (Split-Path $resolved -Leaf) -notlike 'Fassemblant-LauncherTests-*') { throw 'Unsafe fixture cleanup path' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
