# GitStatus for Unreal

A lightweight, **editor-only C++ Unreal Engine plugin** showing the current Git repository, branch and working-tree status alongside the Level Editor's Play controls. No Blueprint nodes, buttons, GitHub login or network requests.

GitHub's **git-branch Octicon** indicates local status:

- Green: `ExampleProject / main (Clean)`
- Amber: `ExampleProject / feature/example (Modified)`
- Red: `Git: unavailable` (Git missing or project not in a Git checkout)

Hover for Git status details. The number of changed/untracked entries is shown correctly as **1 entry** or **N entries**. The status refreshes automatically when the editor starts and every 15 seconds; the interval is not displayed in the tooltip.

The repository name is obtained from the local `origin` remote URL when available, with a fallback to the local repository folder. Supports Git worktrees and nested Unreal project folders. **Read-only:** does not switch branches, fetch, stage, pull, push or modify files.

## Precompiled Windows test build — Unreal 5.7

[Download GitStatus for Unreal 5.7 (Win64, binary-only test package)](Distributions/GitStatus-UE5.7-Win64-Test.zip)

This build was supplied after a successful Unreal `BuildPlugin` run. It includes the compiled Windows x64 editor DLL, `UnrealEditor.modules`, the plugin descriptor and the branch-icon resources; it omits debugging symbols and intermediate build files. The archive reports **Unreal Engine 5.7.0**, Build ID `47537391` and GitStatus version `0.2.1`. The complete packaged build has **not yet been smoke-tested in a running Unreal Editor**.

To test: **close Unreal**, preserve any edits to an existing `Plugins/GitStatus` directory, then extract the archive so that `GitStatus.uplugin` is located at `<UnrealProject>/Plugins/GitStatus/GitStatus.uplugin`. Launch that project with the matching Unreal 5.7 Windows installation. If it reports a different engine/module Build ID, don't manually force the build ID—use an appropriately compiled package instead.

**Do not install this DLL in an Unreal 5.8 project.** The source-based installation instructions below remain suitable for compiling on other engine versions. This test archive is deliberately stored under `Distributions/`, outside the active plugin's `Binaries/` directory.

## Install into a project (recommended: normal project files)

**Copy the plugin into the Unreal project and commit the files with that project.** Unreal must have the actual `GitStatus.uplugin` file on disk when it opens. A Git submodule can also work, but its files must first be initialised separately; otherwise Unreal displays a **Missing Plugin** dialog. Normal tracked files remove that extra setup step for people cloning and pulling projects with SourceTree or GitHub Desktop.

1. Close Unreal Editor.
2. On this GitHub repository, select **Code → Download ZIP** (or clone the repository separately).
3. Create `Plugins/GitStatus/` in the directory containing the target `.uproject`.
4. Copy the **contents** of this repository into that directory, including `GitStatus.uplugin`, `Source/`, `Resources/` and the README. Do **not** copy the source checkout's `.git/` directory.
5. Check the layout:

   ```text
   ExampleProject/
     ExampleProject.uproject
     Plugins/
       GitStatus/
         GitStatus.uplugin
         Source/
         Resources/
         README.md
   ```

6. **Commit the files** in your Unreal project repository, so other contributors get them via a normal pull (no submodule initialisation required).
7. Ensure Git is installed and `git` is on the operating system's PATH.
8. Open the `.uproject` in Unreal Editor. Allow the plugin to compile when asked, and check **Edit → Plugins → GitStatus** if it doesn't appear. Restart if required.

The plugin declares `EnabledByDefault: true`. Projects may also explicitly enable `GitStatus` in their `.uproject`. Because this is a C++ editor module, **a compatible C++ compiler and Windows SDK are required on Windows**, even if the game uses Blueprints only.

### Already seeing a Missing Plugin message?

Close the editor and verify the actual file exists at `<ProjectFolder>/Plugins/GitStatus/GitStatus.uplugin`. The GitHub Desktop or SourceTree branch list alone is not enough; the files must exist locally.

If your project was set up with GitStatus as a **submodule**, initialise it with:

```powershell
git submodule update --init --recursive
```

That resolves the missing folder without converting it to a normal copy. If the project maintainer subsequently changes the GitStatus submodule to ordinary tracked files, **pull the new commit first** and let Git update the directory; don't manually overwrite modified plugin files. Preserve any local edits before switching.

If the plugin folder exists but Unreal cannot load it, that is a **different issue** (for example a C++ build failure or incompatible engine version). Check the Unreal build output rather than repeatedly copying the folder.

### Keeping copied plugin versions up to date

The standalone repository is the upstream source. Copied installations are intentionally **versioned within each game repository**: to update, download/clone the new GitStatus revision, replace the tracked plugin files, and commit that change in the game repository. SourceTree and GitHub Desktop will then pull the updated files as normal.

### Alternative: Git submodule (advanced)

If preferred, the plugin **can** be installed as a submodule, provided the project doesn't already track `Plugins/GitStatus`:

```powershell
git submodule add https://github.com/Braneloc/GitStatus-ForUnreal.git Plugins/GitStatus
```

Anyone cloning or pulling that project must also initialise/update its submodules (`git submodule update --init --recursive`), or clone using `git clone --recurse-submodules`. This is why normal file copies are the recommended installation here. An existing tracked plugin folder must be removed from Git before adding a submodule; don't install over it.

## Building Windows binaries (maintainer only)

The plugin source is included in the repository, but **source files are not compiled DLLs**. To distribute a version that doesn't require each recipient to have a compiler, build a binary package against the **same Unreal Engine installation** the recipients use.

From Windows with Unreal Engine installed and Visual Studio C++ build tools available, either double-click `Tools/Build-GitStatus.cmd` or run:

```powershell
.\Tools\Build-GitStatus.ps1
```

The script detects `EngineAssociation` from the adjacent project's `.uproject` when present (e.g. 5.7 for one project or 5.8 for another). When run from a standalone source checkout, it defaults to **5.7**. Unreal is expected under `C:\Program Files\Epic Games\UE_5.7` or its matching version folder. To override:

```powershell
.\Tools\Build-GitStatus.ps1 -EngineVersion 5.7 -EngineRoot 'D:\Epic Games\UE_5.7'
```

The helper invokes Unreal's `RunUAT.bat BuildPlugin` for `Win64`, then checks that the packaged `GitStatus.uplugin` and compiled DLLs exist. On success, it writes a timestamped, ignored `BuildOutput/GitStatus-UE.../` folder containing a `GitStatus/` package and a ZIP. It **does not delete, replace, or overwrite** the project's source plugin or existing build outputs.

**To try the compiled package:** close Unreal, back up any local changes to `Plugins/GitStatus`, then copy the **packaged** `GitStatus/` folder into the Unreal project's `Plugins/` directory. For a team-ready release, test the package on a clean Windows machine with the matching engine, **without Visual Studio**. Only then commit the packaged binaries or distribute that tested package. Precompiled 5.7 and 5.8 binaries are not interchangeable.

The GitHub repository currently contains build sources and this helper; **no compiled binaries have been produced or verified** in this repository yet.

## Technical notes

- Source: `Source/GitStatusEditor/`, an editor module with no runtime/game dependencies.
- Icon: official GitHub Primer Octicons `git-branch-16.svg`, with MIT licence in `Resources/LICENSE-OCTICONS.txt`.
- Git executable must be on PATH; the plugin reads local Git status only.
- Toolbar placement requests the right/end of the Play toolbar, but Unreal determines final layout.
- Untracked files count as changes; ignored files don't.
- Git runs synchronously. Exceptionally large working trees may briefly pause the editor during a status check.
- **The plugin still requires its first compile/runtime check in Unreal Editor**; source publication does not establish binary compatibility.
- Close Unreal before changing branches that replace open `.uasset` or `.umap` files.

## Notes

- Disclaimer: Parts of this project were co-created by ChatGPT.

## Party on dudes  
![](https://avatars.githubusercontent.com/u/9757397?s=96&v=4)
