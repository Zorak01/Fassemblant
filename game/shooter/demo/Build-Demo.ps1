param(
    [string]$EngineRoot = 'C:\Unreal\UE_5.8',
    [switch]$Open
)
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../shooter.uproject'))
$build = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $build)) { throw "Unreal build tool missing: $build" }
& $build shooterEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal compilation failed ($LASTEXITCODE)." }
$script = Join-Path $PSScriptRoot 'Scripts/create_demo.py'
& $editor $project -run=pythonscript "-script=$script" -unattended -nosplash -nop4 -NullRHI -stdout -FullStdOutLogOutput
if ($LASTEXITCODE -ne 0) { throw "Demo asset generation failed ($LASTEXITCODE). See Saved/Logs." }
if ($Open) {
    Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList @("`"$project`"") -WindowStyle Hidden
}
