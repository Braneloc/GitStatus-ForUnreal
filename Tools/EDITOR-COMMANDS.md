# Local Unreal Editor commands (GitStatus 0.4.0)

GitStatus now supports five **allowlisted, local-only** commands for Codex or other development tools. The plugin does not open a network listener, run arbitrary console commands, or perform Git writes.

The updated C++ plugin **must be rebuilt and installed into the Unreal project** before these commands will work. The existing 0.3.1 packaged binaries do not contain this feature. **Unreal 5.7 and 5.8 compilation and runtime tests have not yet been performed for 0.4.0.** See the main README for the build helper.

## Command examples

Open the Unreal project with the new plugin installed. In a PowerShell terminal:

~~~powershell
# Save a specific loaded level or asset:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action SaveFile -AssetPath '/Game/Maps/Arena'

# A project-relative file path works too:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action SaveFile -AssetPath 'Content\Maps\Arena.umap'

# Save the currently open level:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action SaveCurrentLevel

# Save all dirty maps and assets:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action SaveAll

# Close Unreal only if there are no dirty map or asset packages:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action Shutdown

# Save all existing assets/levels, then request graceful editor shutdown:
.\Tools\Invoke-GitStatusCommand.ps1 -ProjectRoot 'D:\Projects\DragonCare' -Action SaveAndShutdown
~~~

Change the example project directory to the actual folder containing **one** .uproject file. If the helper is copied into the project's Plugins/GitStatus/Tools directory, invoke it from there; commands are independent of the shell's current directory except for the path used to locate the helper.

**SaveFile only saves a package already loaded in the open editor.** It accepts Unreal package paths (for example, /Game/Maps/Arena), project-relative paths (Content/Maps/Arena.umap), or absolute .uasset/.umap filenames. It does not load a different map, overwrite arbitrary text files, or save unloaded packages. Use SaveAll when working with maps containing external actors or other interdependent assets.

**Shutdown refuses to exit if any dirty map or content package is detected.** SaveAndShutdown saves before requesting a normal, non-forced Unreal Editor exit. On save failure, it stays open and reports the error. These commands cannot automatically resolve modal dialogs or save never-before-saved assets that require a filename. Close dialogues and save new assets interactively once, then retry.

The tool returns a one-line JSON response with RequestIdentifier, Action, Success and Message. On failure it returns a nonzero process exit code, which Codex can detect. The default timeout is 45 seconds; a timed-out queued request is removed when possible. The helper checks that the target folder contains one .uproject file to reduce the chance of targeting the wrong project.

## Implementation

- Each request is an atomic JSON file in **<Project>/Saved/GitStatusControl/Commands/**. The plugin processes at most one per existing one-second editor scheduler tick, on the editor thread.
- Each response is written to **<Project>/Saved/GitStatusControl/Results/** and normally removed by the invoking script after it is read.
- Requests expire after 60 seconds, so stale shutdown commands cannot unexpectedly run when the editor is opened later.
- This interface trusts processes with local write access to the project's Saved folder. It is intended for a local development machine, not a shared server or hostile local users.
- The save operations use Unreal's editor package-saving APIs, not direct binary file writes. Unexpected source-control prompts or editor dialogs must be handled separately.
- Keep GitStatusControl under **Saved**, which should stay out of version control.

Commands are deliberately limited to save and shutdown. In particular, no unsaved-work-discarding or forced-kill operation is exposed.
