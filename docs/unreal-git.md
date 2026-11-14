# Git in Unreal

## Open the project

Install Git with Git LFS. Clone the repository and run:

```sh
git lfs install --local
git lfs pull
```

Keep the Unreal project under `game/`, inside this Git checkout. Do not initialize another Git repository inside `game/`. Open `game/shooter/shooter.uproject` with Unreal Engine 5.8.

## Connect the editor

1. Open the `.uproject`.
2. In Edit > Plugins, enable Git Source Control if it is available in your engine build. Restart if prompted.
3. Open Revision Control (called Source Control in some versions) > Connect to Revision Control.
4. Select Git. On Windows, the Git executable is usually `C:/Program Files/Git/bin/git.exe`.
5. Connect to the existing repository. Do not create another repository or overwrite the ignore files.

If Git is not listed, the editor needs a compatible Git source-control plugin. Use a plugin compatible with Unreal Engine 5.8. The editor connection has not yet been verified.

GitHub authentication is handled by Git Credential Manager. Do not store tokens in project settings.

## Edit and submit

Before opening Unreal, update your branch:

```sh
git pull --ff-only
git lfs pull
```

Lock a map or asset before editing it:

```sh
git lfs lock game/Content/Maps/Example.umap
```

Save in Unreal, then commit and push from Git or your Git client. The editor's available Git operations depend on the installed plugin.

```sh
git add game/Content/Maps/Example.umap
git commit -m "Update example map"
git push
git lfs unlock game/Content/Maps/Example.umap
```

Use `git lfs locks` to see who owns a lock. Keep the lock until the change has been pushed. Pull and switch branches with the editor closed to avoid stale loaded assets.

## What belongs in Git

Track `.uproject`, `Config`, `Content`, `Source`, and project plugins. Keep project-specific files in `Build`, such as icons and packaging settings.

`Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` are ignored, including generated plugin output. A precompiled-only plugin may need its binaries explicitly included after checking its redistribution license.

Unreal assets and maps use Git LFS. Source-art formats are listed in `.gitattributes`. Lockable files are read-only until locked; a Git plugin without lock support requires the command-line lock commands above.

References: [Unreal Git guide](https://cdn2.unrealengine.com/gdc-ue-edu-version-control-guide-v08-dc5b56be8c13.pdf), [Git LFS locking](https://github.com/git-lfs/git-lfs/wiki/File-Locking).
