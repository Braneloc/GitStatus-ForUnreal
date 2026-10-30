#pragma once

// Processes one allowlisted editor command from the project's local Saved folder.
// Runs on the editor thread from the GitStatus scheduler (never from a watcher).
namespace GitStatusCommands
{
    void Poll();
}
