# Contributing

## Pick a task

Create or claim an issue before substantial work so teammates do not duplicate effort. Include the goal, proposed scope, and acceptance criteria. Discuss changes to the engine or core design before implementing them.

## Make a change

```sh
git clone https://github.com/Zorak01/Fassemblant.git
cd Fassemblant
git switch -c feature/short-description
# Make and check your changes.
git add <files-you-changed>
git commit -m "Describe the change"
git push -u origin feature/short-description
```

Open a pull request against `main`. Use `fix/`, `docs/`, or `art/` branches when appropriate. Keep unrelated changes separate.

## Before merging

- Explain the player-facing or team-facing result.
- Link the issue and state what you tested.
- Include visuals when appearance or interaction changes.
- Check that no credentials, personal files, generated caches, or unlicensed assets are included.
- Get a teammate's review when available and resolve feedback.

These are team conventions; branch protection is not configured by these files.

## Unreal assets

Use Git LFS for the formats listed in `.gitattributes`. Lock `.uasset` and `.umap` files before editing, then unlock after pushing. See [Git in Unreal](docs/unreal-git.md) for commands.

Do not commit generated caches, packaged builds, or whole downloaded asset packs. Check asset licenses before adding source files or plugins.
