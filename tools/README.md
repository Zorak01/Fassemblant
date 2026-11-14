# Development launcher

Double-click `Launch.bat` in the repository root. Press Enter to set up, build
and play. The menu also opens the editor, builds, checks prerequisites, runs
gameplay/launcher tests, updates the checkout, and opens logs. Windows
PowerShell 5.1 ships with Windows 10/11; PowerShell 7 is optional.

```powershell
.\tools\Dev.ps1 -Action Play
.\tools\Dev.ps1 -Action Editor
.\tools\Dev.ps1 -Action Setup
.\tools\Dev.ps1 -Action Doctor -NoInstall
.\tools\Dev.ps1 -Action Test -NoInstall
.\tools\Dev.ps1 -Action VisualTest -NoInstall
.\tools\Dev.ps1 -Action LauncherTest
.\tools\Dev.ps1 -Action Build -ForceBuild
```

`-ForceBuild` invokes UnrealBuildTool again; Unreal still compiles incrementally.
`-NoInstall` fails clearly instead of installing software. No global execution
policy, PATH, Git ownership exceptions or security settings are changed.

## First run

The launcher discovers matching engines from Epic's installation manifest,
registered/source builds, common drive locations and its last valid choice.
It matches the project's `EngineAssociation`; it never silently upgrades the
project. For an unregistered engine in an unusual location, `-EngineRoot`
or the optional `UE_ROOT` environment variable overrides discovery.

If Unreal is absent, it installs/opens Epic Games Launcher and waits for the
matching engine. **You must sign in, accept Epic's terms and choose the engine
download.** Press Q to leave the launcher during a long download and resume
later. Epic does not provide an unrestricted unattended public engine install.
See [Epic's installation instructions](https://dev.epicgames.com/documentation/en-us/unreal-engine/install-unreal-engine).

For C++ builds, the launcher checks the actual compiler file version against
that engine's minimum/banned version ranges and verifies the Windows SDK's
headers, linker library and resource compiler. Missing components are installed
using a signed Microsoft Visual Studio 2022 bootstrapper. Existing VS 2022
installations are modified instead of duplicated. Windows may require UAC
approval or a restart. Already supported tools are left alone. The engine's
bundled .NET is used; no extra system .NET or CMake installation is required.
See [Microsoft's installer options](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio?view=visualstudio).

Git is only needed for Sync or missing LFS content. If necessary, Git for Windows
is installed using winget, with a signed official GitHub installer as fallback.
Archives with complete content build without Git. Sync uses `git pull --ff-only`
and preserves local changes; it never resets, cleans or force-pulls. Git access
to a private repository still requires your own credentials.

## Fast repeat runs and recovery

Successful builds save a local fingerprint of source/configuration, engine and
toolchain, plus the compiled module hash. An unchanged checkout skips the build
tool entirely. Changed files, a changed engine/compiler/SDK, missing binaries,
or an invalid cache trigger an incremental build. Existing demo maps are never
regenerated. Downloads are cached and signatures checked before execution.

The launcher checks for an editor/game holding this project's module open before
a needed build. Save and close it, then retry; the launcher never kills it.
Busy installers receive one bounded retry. Download errors, rejected signatures,
required restarts and failed builds produce actionable messages and log paths.
Unreal's JSON test report is checked even when its process returns exit code zero.

Logs, downloaded installers and build state live in `game/shooter/Saved/Launcher`.
These files are ignored by Git. Gameplay reports live under `Saved/Automation/Demo`.
The legacy `demo/Build-Demo.ps1` now delegates to this launcher.

The **V** menu option runs a rendered standalone play test with real physics,
weapon logic and mannequin animation. It checks sight/grip alignment and saves
hip, firing, ADS, reload, sprint, jump and strafe screenshots, plus a JSON report,
under `Saved/Automation/Presentation`. A working graphics driver/GPU is required;
this is separate from the fast headless gameplay tests.

## Validation

The dependency-free test script uses disposable fixtures for version matching,
missing/corrupt metadata, fresh/stale builds, replaced/deleted binaries, unusual
paths, LFS placeholders, invalid gameplay reports and installer success/failure,
busy/restart responses. Installer responses are mocked: the tests never download
or install software. Real build/setup/skip/test/play flows are also checked on
the development machine. This is not a claim of clean-VM installation coverage
for every Windows/Visual Studio combination.
