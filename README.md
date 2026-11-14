# Fassemblant

An Unreal Engine game project. Game design is still being decided.

[Git and Unreal setup](docs/unreal-git.md) · [Project brief](docs/project-brief.md) · [Roadmap](docs/roadmap.md)

## Setup

```sh
git clone https://github.com/Zorak01/Fassemblant.git
cd Fassemblant
git lfs install --local
git lfs pull
```

Open `game/shooter/shooter.uproject` with **Unreal Engine 5.8**. The current project is Blueprint-only. No content assets or maps have been saved yet.

Maps and Unreal assets use Git LFS with file locking. Generated caches and build output are ignored. Commit project settings, source, content, and plugin descriptors.

## Folders

| Folder | Contents |
| --- | --- |
| `game/` | Unreal project |
| `assets/` | Source art and audio |
| `docs/` | Setup, design, and project decisions |

See [CONTRIBUTING.md](CONTRIBUTING.md) for the branch and asset workflow.
