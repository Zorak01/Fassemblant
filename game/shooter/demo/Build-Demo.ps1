param(
    [string]$EngineRoot,
    [switch]$Open
)
$ErrorActionPreference = 'Stop'
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../tools/Dev.ps1'))
$options = @{ Action = $(if ($Open) { 'Editor' } else { 'Generate' }) }
if ($EngineRoot) { $options.EngineRoot = $EngineRoot }
& $launcher @options
