#requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet('Menu', 'Play', 'Editor', 'Build', 'Setup', 'Doctor', 'Test', 'VisualTest', 'Generate', 'Sync', 'LauncherTest')]
    [string]$Action = 'Menu',
    [string]$EngineRoot,
    [switch]$NoInstall,
    [switch]$ForceBuild
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'Launcher.Core.psm1') -Force -DisableNameChecking
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$projectRoot = Join-Path $repoRoot 'game/shooter'
$project = Join-Path $projectRoot 'shooter.uproject'
$work = Join-Path $projectRoot 'Saved/Launcher'
$logs = Join-Path $work 'Logs'
$downloads = Join-Path $work 'Downloads'
foreach ($folder in @($logs, $downloads)) { [IO.Directory]::CreateDirectory($folder) | Out-Null }
$projectInfo = Read-JsonFile $project
if (!$projectInfo) { throw "Missing/invalid project: $project" }
$association = $projectInfo.EngineAssociation
$statePath = Join-Path $work 'BuildState.json'
$engineCache = Join-Path $work 'Engine.json'

function Resolve-Engine {
    $preferred = $EngineRoot
    if (!$preferred -and $env:UE_ROOT) { $preferred = $env:UE_ROOT }
    if (!$preferred) { $cache = Read-JsonFile $engineCache; if ($cache) { $preferred = $cache.Root } }
    $engine = Find-UnrealEngine $association $preferred
    if (!$engine -and !$NoInstall) {
        Write-Host "Unreal $association is missing. Opening Epic's official engine installer." -ForegroundColor Yellow
        $epic = Join-Path ${env:ProgramFiles(x86)} 'Epic Games/Launcher/Portal/Binaries/Win64/EpicGamesLauncher.exe'
        if (!(Test-Path -LiteralPath $epic)) {
            $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
            if (!$winget) {
                Start-Process 'https://www.unrealengine.com/download'
            } else {
                Invoke-NativeLogged $winget.Source @('install', '--id', 'EpicGames.EpicGamesLauncher', '--exact', '--source', 'winget', '--accept-package-agreements', '--accept-source-agreements') (Join-Path $logs 'EpicInstall.log') 'Installing Epic Games Launcher'
            }
        }
        if (Test-Path -LiteralPath $epic) { Start-Process 'com.epicgames.launcher://ue/library' }
        Write-Host "Sign in to Epic, accept its terms, and install Unreal $association. No path configuration is needed." -ForegroundColor Cyan
        Write-Host 'Detection resumes automatically. Press Q to return while the engine downloads.'
        do {
            Start-Sleep -Seconds 3
            $engine = Find-UnrealEngine $association $preferred
            if (!$engine -and ![Console]::IsInputRedirected -and [Console]::KeyAvailable) {
                if ([Console]::ReadKey($true).Key -eq 'Q') { throw 'Engine download can continue in Epic. Run the launcher again when ready.' }
            }
        } until ($engine)
    }
    if (!$engine) { throw "Unreal $association not found. Install it using Epic Games Launcher, or pass -EngineRoot for an unregistered source build." }
    $engine | ConvertTo-Json | Set-Content -LiteralPath $engineCache -Encoding UTF8
    Write-Host ("  Unreal {0}.{1}  {2}" -f $engine.Version, $engine.Patch, $engine.Root) -ForegroundColor Green
    return $engine
}

function Ensure-Content {
    $pointers = @(Get-LfsPointers (Join-Path $projectRoot 'Content'))
    if (!$pointers.Count) { return }
    $git = Ensure-Git
    Push-Location $repoRoot
    try {
        Invoke-NativeLogged $git.Source @('lfs', 'install', '--local') (Join-Path $logs 'LfsInstall.log') 'Configuring local Git LFS'
        Invoke-NativeLogged $git.Source @('lfs', 'pull') (Join-Path $logs 'LfsPull.log') 'Downloading missing project content'
    } finally { Pop-Location }
    if (@(Get-LfsPointers (Join-Path $projectRoot 'Content')).Count) { throw 'Content is still missing. Check access to the repository and the LFS download log.' }
}

function Ensure-Git {
    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($git) { return $git }
    if ($NoInstall) { throw 'Git is required to fetch LFS content or sync. Run Setup/Sync without -NoInstall.' }
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if ($winget) {
        Invoke-NativeLogged $winget.Source @('install', '--id', 'Git.Git', '--exact', '--source', 'winget', '--accept-package-agreements', '--accept-source-agreements') (Join-Path $logs 'GitInstall.log') 'Installing Git for Windows with LFS'
    } else {
        # Official Git for Windows release metadata; use a signed x64 installer.
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $release = Invoke-RestMethod 'https://api.github.com/repos/git-for-windows/git/releases/latest' -UseBasicParsing
        $asset = $release.assets | Where-Object { $_.name -match '^Git-.*-64-bit\.exe$' -and $_.browser_download_url -like 'https://github.com/git-for-windows/git/releases/download/*' } | Select-Object -First 1
        if (!$asset) { throw 'Official Git x64 installer was not found. Install Git for Windows and rerun.' }
        $installer = Get-SignedInstaller $asset.browser_download_url (Join-Path $downloads $asset.name) 'Microsoft Corporation|Johannes Schindelin'
        Invoke-ElevatedInstaller $installer @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-')
    }
    $env:PATH += ';' + (Join-Path $env:ProgramFiles 'Git/cmd')
    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (!$git) { throw 'Git installation completed but git.exe was not found. Close this launcher and rerun to refresh PATH.' }
    return $git
}

function Ensure-Environment($Engine) {
    $toolchain = Get-WindowsToolchain $Engine.Root
    if (!$toolchain.Compiler -or !$toolchain.SDK) {
        if ($NoInstall) { throw 'C++ toolchain/Windows SDK missing or rejected by this engine. Run Setup without -NoInstall.' }
        $toolchain = Install-Toolchain $Engine.Root $downloads
    }
    Write-Host "  MSVC $($toolchain.Compiler.Version)  /  Windows SDK $($toolchain.SDK)" -ForegroundColor Green
    $bundledDotNet = Join-Path $Engine.Root 'Engine/Binaries/ThirdParty/DotNet'
    if (!@(Get-ChildItem -LiteralPath $bundledDotNet -Recurse -Filter dotnet.exe -ErrorAction SilentlyContinue).Count) {
        throw 'Bundled .NET runtime is missing. Use Verify on this engine in Epic Launcher; no unrelated system .NET installation is needed.'
    }
    $runtime = Get-ItemProperty 'HKLM:/SOFTWARE/Microsoft/VisualStudio/14.0/VC/Runtimes/x64' -ErrorAction SilentlyContinue
    if (!$runtime -or !$runtime.Installed -or [version]$runtime.Version.TrimStart('v') -lt [version]'14.38') {
        if ($NoInstall) { throw 'Visual C++ runtime missing. Run Setup without -NoInstall.' }
        $redist = Join-Path $Engine.Root 'Engine/Extras/Redist/en-us/vc_redist.x64.exe'
        if (!(Test-Path -LiteralPath $redist)) { $redist = Get-SignedInstaller 'https://aka.ms/vs/17/release/vc_redist.x64.exe' (Join-Path $downloads 'vc_redist.x64.exe') }
        Invoke-ElevatedInstaller $redist @('/install', '/quiet', '/norestart')
    }
    Ensure-Content
    return $toolchain
}

function Build-Project($Engine, $Toolchain) {
    $toolId = "$($Toolchain.Compiler.Version)|$($Toolchain.SDK)"
    $fingerprint = Get-BuildFingerprint $projectRoot $Engine.Root $toolId
    if (!$ForceBuild -and (Test-BuildCurrent $projectRoot $fingerprint $statePath)) {
        Write-Host '  Build is current. Skipping UnrealBuildTool.' -ForegroundColor Green
        return
    }
    $arguments = @('shooterEditor', 'Win64', 'Development', "-Project=$project", '-WaitMutex', '-NoHotReloadFromIDE',
        "-CompilerVersion=$($Toolchain.Compiler.Version)", "-WindowsSdkVersion=$($Toolchain.SDK)")
    Assert-ProjectClosed $projectRoot
    Invoke-NativeLogged (Join-Path $Engine.Root 'Engine/Build/BatchFiles/Build.bat') $arguments (Join-Path $logs 'Build.log') 'Building project (incremental)'
    # Hash again: Unreal may add project configuration during its first build.
    @{
        Fingerprint = Get-BuildFingerprint $projectRoot $Engine.Root $toolId
        BinaryHash = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/UnrealEditor-shooter.dll') -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
}

function Ensure-Map($Engine) {
    $map = Join-Path $projectRoot 'Content/demo/Maps/DemoRange.umap'
    if (Test-Path -LiteralPath $map) { Write-Host '  Demo map ready; preserving existing assets.' -ForegroundColor Green; return }
    $scriptPath = Join-Path $projectRoot 'demo/Scripts/create_demo.py'
    $arguments = @($project, '-run=pythonscript', "-script=$scriptPath", '-unattended', '-nosplash', '-nop4', '-NullRHI', '-NoLiveCoding')
    Invoke-NativeLogged (Join-Path $Engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $arguments (Join-Path $logs 'Generate.log') 'Creating demo content'
    if (!(Test-Path -LiteralPath $map)) { throw 'Editor exited without creating the demo map. See Generate.log and Saved/Logs/shooter.log.' }
}

function Show-Doctor {
    Write-Host "`nFASSEMBLANT / environment check" -ForegroundColor Cyan
    $engine = Find-UnrealEngine $association $EngineRoot
    if (!$engine) { Write-Host "  MISSING Unreal $association" -ForegroundColor Yellow; return }
    Write-Host "  Engine: $($engine.Root)"
    $toolchain = Get-WindowsToolchain $engine.Root
    Write-Host "  Compiler: $(if ($toolchain.Compiler) {$toolchain.Compiler.Version} else {'MISSING / unsupported'})"
    Write-Host "  SDK: $(if ($toolchain.SDK) {$toolchain.SDK} else {'MISSING'})"
    Write-Host "  Git: $(if (Get-Command git.exe -ErrorAction SilentlyContinue) {'installed'} else {'optional; missing'})"
    Write-Host "  LFS pointer assets: $(@(Get-LfsPointers (Join-Path $projectRoot 'Content')).Count)"
    Write-Host "  Logs: $logs"
    if ($toolchain.Compiler -and $toolchain.SDK) {
        $fingerprint = Get-BuildFingerprint $projectRoot $engine.Root "$($toolchain.Compiler.Version)|$($toolchain.SDK)"
        Write-Host "  Build cache current: $(Test-BuildCurrent $projectRoot $fingerprint $statePath)"
    }
}

function Ensure-FirstPerson($Engine) {
    $manifestPath = Join-Path $projectRoot 'demo/FirstPersonAssets.json'
    $manifest = Read-JsonFile $manifestPath
    $missing = @()
    if ($manifest -and $manifest.PSObject.Properties['assets']) {
        foreach ($asset in $manifest.assets) {
            if (!$asset.StartsWith('/Game/demo/')) { throw 'Invalid first-person asset manifest path.' }
            $file = Join-Path $projectRoot ('Content/' + $asset.Substring(6) + '.uasset')
            if (!(Test-Path -LiteralPath $file)) { $missing += $asset }
        }
        if (!$missing.Count) { Write-Host '  Rifle, mannequin arms and audio ready.' -ForegroundColor Green; return }
    }
    Assert-ProjectClosed $projectRoot
    $scriptPath = Join-Path $projectRoot 'demo/Scripts/prepare_first_person.py'
    $arguments = @($project, '-run=pythonscript', "-script=$scriptPath", '-unattended', '-nosplash', '-nop4', '-NullRHI', '-NoLiveCoding')
    Invoke-NativeLogged (Join-Path $Engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $arguments (Join-Path $logs 'FirstPersonSetup.log') 'Preparing first-person art and animation assets'
    if (!(Test-Path -LiteralPath $manifestPath)) { throw 'First-person setup did not complete. Inspect FirstPersonSetup.log and Saved/Logs.' }
}

function Invoke-Task([string]$Task) {
    if ($Task -eq 'Doctor') { Show-Doctor; return }
    if ($Task -eq 'LauncherTest') { & (Join-Path $PSScriptRoot 'Tests/Launcher.Tests.ps1'); return }
    if ($Task -eq 'Sync') {
        if (!(Test-Path -LiteralPath (Join-Path $repoRoot '.git'))) { throw 'This is an archive, not a Git checkout. Download a new archive or clone the repository to sync.' }
        $git = Ensure-Git
        Push-Location $repoRoot
        try { Invoke-NativeLogged $git.Source @('pull', '--ff-only') (Join-Path $logs 'Sync.log') 'Updating checkout without merge commits'; Ensure-Content }
        finally { Pop-Location }
        return
    }
    $engine = Resolve-Engine
    $toolchain = Ensure-Environment $engine
    if ($Task -eq 'Setup') { Write-Host '  Environment ready.' -ForegroundColor Green; return }
    Build-Project $engine $toolchain
    if ($Task -eq 'Build') { return }
    Ensure-Map $engine
    Ensure-FirstPerson $engine
    if ($Task -eq 'Generate') { return }
    if ($Task -eq 'VisualTest') {
        $reportRoot = Join-Path $projectRoot ('Saved/Automation/Presentation/Run-' + [guid]::NewGuid().ToString('N'))
        $arguments = @($project, '/Game/demo/Maps/DemoRange', '-game', '-RenderOffscreen', '-unattended', '-NoSound', '-ResX=1280', '-ResY=720', '-nosplash', '-nop4', '-NoLiveCoding', '-DemoPresentationTest', "-DemoReportDir=$reportRoot", '-ExecCmds=t.MaxFPS 60,r.MotionBlurQuality 0,r.ScreenPercentage 100')
        Invoke-NativeLogged (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $arguments (Join-Path $logs 'Presentation.log') 'Running rendered play test (GPU required)'
        $report = Read-JsonFile (Join-Path $reportRoot 'report.json')
        if (!$report -or !$report.passed) { throw "Rendered play test failed or did not finish. Report/screenshots: $reportRoot" }
        Write-Host "  Rendered play checks passed. Screenshots: $reportRoot" -ForegroundColor Green
        return
    }
    if ($Task -eq 'Test') {
        $reportRoot = Join-Path $projectRoot ('Saved/Automation/Demo/Run-' + [guid]::NewGuid().ToString('N'))
        $arguments = @($project, '-unattended', '-nosplash', '-nop4', '-NullRHI', '-NoLiveCoding', '-ExecCmds=Automation RunTests Demo.Gameplay', '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$reportRoot")
        Invoke-NativeLogged (Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $arguments (Join-Path $logs 'Tests.log') 'Running Unreal gameplay checks'
        $count = Assert-GameplayReport (Join-Path $reportRoot 'index.json')
        Write-Host "  $count gameplay checks passed. Report: $reportRoot" -ForegroundColor Green
        return
    }
    $editor = Join-Path $engine.Root 'Engine/Binaries/Win64/UnrealEditor.exe'
    $arguments = @($project, '/Game/demo/Maps/DemoRange', '-NoLiveCoding', '-nosplash', '-nop4')
    if ($Task -eq 'Play') { $arguments += @('-game', '-windowed', '-ResX=1600', '-ResY=900') }
    # Interactive editor/game is intentionally visible; helper installers stay hidden.
    Start-Process -FilePath $editor -ArgumentList (($arguments | ForEach-Object { ConvertTo-NativeArgument $_ }) -join ' ')
    Write-Host "  $Task launched." -ForegroundColor Green
}

try {
    if ($Action -ne 'Menu') { Invoke-Task $Action; exit 0 }
    do {
        Write-Host "`n  FASSEMBLANT   /   DEVELOPMENT" -ForegroundColor Cyan
        Write-Host "  Unreal $association | automatic setup | incremental builds`n"
        Write-Host '  [Enter / 1] Play demo       [2] Open editor'
        Write-Host '  [3] Build                  [4] Set up / repair prerequisites'
        Write-Host '  [5] Check environment      [6] Run gameplay tests'
        Write-Host '  [7] Download latest        [8] Launcher self-tests'
        Write-Host '  [9] Open logs              [Q] Quit'
        Write-Host '  [V] Rendered play test with screenshots'
        $choice = Read-Host "`n  Select"
        if ($choice -eq 'q') { break }
        if ($choice -eq '9') { Start-Process explorer.exe -ArgumentList (ConvertTo-NativeArgument $logs); continue }
        $tasks = @{ ''='Play'; '1'='Play'; '2'='Editor'; '3'='Build'; '4'='Setup'; '5'='Doctor'; '6'='Test'; '7'='Sync'; '8'='LauncherTest'; 'v'='VisualTest' }
        if (!$tasks.ContainsKey($choice)) { Write-Host '  Choose one of the options above.' -ForegroundColor Yellow; continue }
        try { Invoke-Task $tasks[$choice] } catch { Write-Host "`n  $($_.Exception.Message)" -ForegroundColor Red; Write-Host '  Your project files and local changes have been preserved.' }
    } while ($true)
} catch {
    Write-Host "`n  $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "  Logs: $logs"
    exit 1
}
