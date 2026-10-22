# GitStatus for Unreal

A lightweight, **editor-only C++ Unreal Engine plugin** showing the current Git repository, branch, working-tree status, and how far the current branch is ahead of or behind the team's shared branch alongside the Level Editor's Play controls. No Blueprint nodes, buttons, GitHub login or network requests.

GitHub's **git-branch Octicon** indicates local status:

- Green: `ExampleProject / main (Clean)`
- Amber: `ExampleProject / feature/example (Modified)`
- Red: `Git: unavailable` (Git missing or project not in a Git checkout)

## Shared-branch status (source version 0.3.0)

Next to the existing repository / branch / Clean-or-Modified display, GitStatus shows the divergence from the shared branch, for example:

- `Keith-DragonCare / Clean    Development ↓14 ↑3`: 14 commits from Development are missing locally, and your branch has three commits not yet in Development.
- `Keith-DragonCare / Clean    Development ↓0 ↑4`: your branch contains everything currently fetched from Development.
- Amber comparison text means shared updates are waiting; green means none are waiting.

The comparison **prefers `origin/Development`** when that remote-tracking branch exists, otherwise uses the remote's default `origin/HEAD` reference (e.g. `main`). If neither is available it hides the comparison rather than inventing a status.

**Fetch Origin in GitHub Desktop or SourceTree to update the remote-tracking references.** GitStatus itself does not fetch or merge: it reads your local Git state using `git rev-list --left-right --count origin/Development...HEAD`. The display refreshes on relevant file-system events, when Unreal regains focus, or at the two-minute fallback. It requires no Git upstream reconfiguration. Pull Origin still tracks each person's own personal remote branch.

Hover for detailed repository, file and divergence information. Changed/untracked entries are shown correctly as **1 entry** or **N entries**. The status refreshes on relevant changes, focus return, and a two-minute fallback; the interval is not displayed in the tooltip.

**The existing precompiled Unreal 5.7 ZIP is version 0.2.1 and does not include this feature.** Compile GitStatus 0.3.0 for Unreal 5.7 / 5.8 and test it before replacing any installed binaries.

The repository name is obtained from the local `origin` remote URL when available, with a fallback to the local repository folder. Supports Git worktrees and nested Unreal project folders. **Read-only:** does not switch branches, fetch, stage, pull, push or modify files.

### Automatic refresh (source version 0.3.1)

- **Git metadata watchers** detect branch changes, commits, merges, and updates to fetched remote references. Worktree-specific Git metadata and shared refs are watched separately.
- **Project folder watchers** cover Unreal project files, Content, Config, Source, and Plugins. Generated Saved, Intermediate, Binaries, and build output changes are ignored.
- A **0.75-second debounce** coalesces bursts of changes before Git status is read. Watcher callbacks only mark the state as dirty; Git commands run on the editor ticker.
- Regaining focus in Unreal requests a refresh.
- A **two-minute fallback check** catches missed events, directories that could not be watched, and changes outside those project folders.
- A light one-second scheduling tick checks for pending work; **it does not run Git once a second**.

GitStatus remains **read-only**: it does not fetch, merge, pull, push, or change Git upstream configuration. Use Fetch Origin in SourceTree/GitHub Desktop to update remote references, and GitStatus will notice the changes. The previous 15-second Git polling has been removed.

**Version 0.3.1 has not yet been compiled and runtime-tested on Unreal 5.7 or 5.8.** The published 5.7 ZIP is still the working 0.2.1 binary. Do not replace the team build until 0.3.1 has been tested.

## GitStatus 0.3.1 — Unreal 5.7 test package

[Download 0.3.1 Unreal 5.7 Win64 test package](Distributions/GitStatus-UE5.7-Win64-v0.3.1-Test.zip)

A new compiled **Win64 Unreal 5.7** plugin was supplied after the 0.3.1 source update. It includes the `Development ↓behind ↑ahead` indicator, directory-watcher refresh, Unreal focus refresh and two-minute fallback. The package contains the plugin descriptor, DLL, `UnrealEditor.modules`, branch icon and icon licence; no debugging symbols or intermediate build files are included.

The package has passed ZIP-integrity, source-revision, version/architecture and Unreal Build ID checks. **It has not yet been opened and tested in an Unreal 5.7 Editor.** The earlier working 0.2.1 package remains below. Do not deploy 0.3.1 to the entire team until that test succeeds.

## Precompiled Windows test build — Unreal 5.7

[Download GitStatus for Unreal 5.7 (Win64, binary-only test package)](Distributions/GitStatus-UE5.7-Win64-Test.zip)

This build was supplied after a successful Unreal `BuildPlugin` run. It includes the compiled Windows x64 editor DLL, `UnrealEditor.modules`, the plugin descriptor and the branch-icon resources and the Plugin Browser icon; it omits debugging symbols and intermediate build files. The archive reports **Unreal Engine 5.7.0**, Build ID `47537391` and GitStatus version `0.2.1`. The 5.7 binary **loaded successfully in a local Unreal 5.7 editor test**. That confirms the test installation, not compatibility with every Unreal Engine build or PC.

To test: **close Unreal**, preserve any edits to an existing `Plugins/GitStatus` directory, then extract the archive so that `GitStatus.uplugin` is located at `<UnrealProject>/Plugins/GitStatus/GitStatus.uplugin`. Launch that project with the matching Unreal 5.7 Windows installation. If it reports a different engine/module Build ID, don't manually force the build ID—use an appropriately compiled package instead.

**Do not install this DLL in an Unreal 5.8 project.** The C++ source may compile without changes in 5.8, but an Unreal 5.8 binary **must be built with a matching 5.8 toolchain and engine build**. Use the build helper below and test the resulting plugin separately. This test archive is deliberately stored under `Distributions/`, outside the active plugin's `Binaries/` directory.

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

The script detects `EngineAssociation` from the adjacent project's `.uproject` when present (e.g. 5.7 or 5.8). When run from a standalone source checkout, it defaults to **5.7**. Unreal is expected under `C:\Program Files\Epic Games\UE_5.7` or its matching version folder. To override:

```powershell
.\Tools\Build-GitStatus.ps1 -EngineVersion 5.7 -EngineRoot 'D:\Epic Games\UE_5.7'
```

The helper invokes Unreal's `RunUAT.bat BuildPlugin` for `Win64` from a **clean staged source copy**, then checks that the packaged `GitStatus.uplugin` and compiled DLLs exist. On success, it writes a timestamped `Saved/GitStatusBuildOutput/GitStatus-UE.../` folder **outside the installed plugin** (when run inside an Unreal project), containing a `GitStatus/` package and a ZIP. When run from the standalone plugin checkout, it uses its local, ignored `BuildOutput/` folder. It **does not delete, replace, or overwrite** the project's installed plugin or existing build outputs.

**To try the compiled package:** close Unreal, back up any local changes to `Plugins/GitStatus`, then copy the **packaged** `GitStatus/` folder into the Unreal project's `Plugins/` directory. For a team-ready release, test the package on a clean Windows machine with the matching engine, **without Visual Studio**. Only then commit the packaged binaries or distribute that tested package. Precompiled 5.7 and 5.8 binaries are not interchangeable.

The repository includes the **Unreal 5.7 Win64 test binary** under `Distributions/`; it does **not** currently include a 5.8 binary. The build helper can produce an engine-specific package on a Windows development machine.



### Building from a binary-only installation (including Unreal 5.8)

**The Unreal 5.7 precompiled ZIP deliberately omits `Source/`.** If you installed it into a project and then run `Tools/Build-GitStatus.cmd` in Unreal 5.8, the original build helper reports `Could not find definition for module 'GitStatusEditor'`. UBT needs the module's `Source/GitStatusEditor/GitStatusEditor.Build.cs` file to build. That error occurs **before any C++ compilation**.

The updated helper handles this automatically: if the required module source files are missing (including an empty `Source/` directory), it downloads the latest **main** branch of this repository into an isolated build directory (under the Unreal project's `Saved/GitStatusBuildOutput/` when applicable), stages only `GitStatus.uplugin`, `Source/` and `Resources/` (plus the README), and runs BuildPlugin on that clean staging copy. The installed plugin and any 5.7 DLL remain untouched. Git and internet access are required **only when local source is absent**.

If `Source/` exists but is incomplete, the script stops with a missing-file error instead of silently mixing local code with downloaded code. Restore the source from this repository in that case. An Unreal Engine 5.8 build still needs the appropriate **Windows C++ compiler/toolchain**; finding the module does not guarantee that the plugin source will compile without additional 5.8 API changes.

After a successful build, close Unreal and install the packaged `GitStatus/` folder from `Saved/GitStatusBuildOutput/` in your 5.8 project, replacing only the plugin folder after safeguarding local edits. Do **not** install or reuse a 5.7 `Binaries/` folder in Unreal 5.8.

## Technical notes

- Source: `Source/GitStatusEditor/`, an editor module with no runtime/game dependencies.
- Icons: official GitHub Primer Octicons `git-branch-16.svg` for the toolbar, plus `Resources/Icon128.png` (128×128) for the Unreal Plugin Browser; the icon source is covered by the MIT licence in `Resources/LICENSE-OCTICONS.txt`.
- Git executable must be on PATH; the plugin reads local Git status and cached remote-tracking refs only (never fetches).
- Toolbar placement requests the right/end of the Play toolbar, but Unreal determines final layout.
- Untracked files count as changes; ignored files don't.
- Git runs synchronously. Exceptionally large working trees may briefly pause the editor during a status check.
- The **Unreal 5.7 test DLL has loaded successfully in the editor**. Unreal 5.8 requires its own compilation and load test; version-specific binary compatibility must not be assumed.
- Close Unreal before changing branches that replace open `.uasset` or `.umap` files.

## Notes

- Disclaimer: Parts of this project were co-created by ChatGPT.

## Party on dudes  
![](https://avatars.githubusercontent.com/u/9757397?s=96&v=4)
