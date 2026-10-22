# GitStatus for Unreal

A small, **editor-only C++ plugin** for Unreal Engine that displays the local Git repository, current branch and working-tree state beside the Level Editor's Play toolbar controls. No Blueprint nodes, no buttons, and no GitHub login or network requests.

The official GitHub **git-branch Octicon** is coloured by local status:

- Green: `ExampleProject / main (Clean)`
- Amber: `ExampleProject / feature/example (Modified)`
- Red: `Git: unavailable` (Git not found or project outside a Git working tree)

Hover to see the Git status details and the number of changed or untracked **entries** (correctly displayed as **1 entry** or **N entries**). It updates at editor startup and automatically every 15 seconds. The tooltip does not show a refresh interval.

The repository name comes from the `origin` remote when available, otherwise the local Git root. It supports Git worktrees and Unreal projects in subdirectories. The plugin is **read-only**: it never switches branches, stages files, fetches, pulls, pushes or edits the repository.

## Install using Git submodules (recommended)

Close Unreal Editor first. From the Git repository root of your Unreal project, run:

```powershell
git submodule add https://github.com/Braneloc/GitStatus-ForUnreal.git Plugins/GitStatus
git add .gitmodules Plugins/GitStatus
git commit -m "Add GitStatus editor plugin"
```

The result should look like:

```text
ExampleProject/
  ExampleProject.uproject
  .gitmodules
  Plugins/
    GitStatus/                 <-- Git submodule
      GitStatus.uplugin
      Source/
      Resources/
```

**Important:** If the `.uproject` is nested below the Git repository root, choose the submodule destination inside the Unreal project folder (e.g. `UnrealProject/Plugins/GitStatus`). Git submodule paths are relative to the **Git repository root**.

Then open the `.uproject` with a compatible Unreal Engine installation, allow the editor to build the plugin and check **Edit → Plugins** if it is not already enabled. The plugin is enabled by default. Building a C++ plugin requires the Unreal-compatible Visual Studio C++ toolchain and Windows SDK on Windows, even for projects that otherwise use Blueprints.

When cloning a project which already includes GitStatus:

```powershell
git clone --recurse-submodules <repository-url>
```

For an existing checkout after pulling a commit which added GitStatus:

```powershell
git submodule update --init --recursive
```

To update GitStatus to the latest upstream `main` and record that exact version in your game repository:

```powershell
git submodule update --remote Plugins/GitStatus
git add Plugins/GitStatus
git commit -m "Update GitStatus plugin"
```

For a nested Unreal project, substitute the correct submodule path in the last two commands. The game repository pins a specific GitStatus commit, so teammates receive the same version.

### Replacing an existing copied plugin with a submodule

If `Plugins/GitStatus` was previously committed as normal files, **do not run `git submodule add` over that existing tracked folder**. First preserve any local edits and commit your work, then remove the tracked copy:

```powershell
git rm -r Plugins/GitStatus
git submodule add https://github.com/Braneloc/GitStatus-ForUnreal.git Plugins/GitStatus
git add .gitmodules Plugins/GitStatus
git commit -m "Use GitStatus as a submodule"
```

Replace `Plugins/GitStatus` with your project's actual Git-root-relative path when needed.

## Install without submodules

Copy the **contents of this repository** into `<UnrealProject>/Plugins/GitStatus/` so the descriptor ends up at `Plugins/GitStatus/GitStatus.uplugin`. Do not nest the repository folder one level deeper. This option requires manual updates.

## Technical notes

- Source: `Source/GitStatusEditor/`, editor module only.
- Icon: official GitHub Primer Octicons `git-branch-16.svg`, with third-party MIT licence in `Resources/LICENSE-OCTICONS.txt`.
- Git executable must be available on PATH.
- Toolbar placement requests the end/right side of the Play toolbar but exact layout depends on Unreal.
- Untracked files are counted, ignored files are excluded; changes are local only.
- Checking Git is synchronous; exceptionally large working trees may momentarily pause the editor.
- **Not yet compiled or smoke-tested in Unreal Editor**; validate on your installed engine version.
- Close Unreal before switching Git branches affecting loaded `.uasset` or `.umap` files.

## Notes

- Disclaimer: Parts of this project were co-created by ChatGPT.

## Party on dudes  
![](https://avatars.githubusercontent.com/u/9757397?s=96&v=4)
