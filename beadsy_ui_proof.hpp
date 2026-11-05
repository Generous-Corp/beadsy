#pragma once

namespace pulp::state {
class StateStore;
}

namespace beadsy {

class BeadsyEditor;

/// Env-gated in-process UI proof driver (macOS only). When `BEADSY_UI_PROOF`
/// names a step, the host process that just created the editor sends real
/// AppKit mouse events to Pulp's own NSView a few frames later, logs the
/// resulting parameter, menu, and window state as JSON lines on stderr, and
/// leaves pixels to the host's capture path (`PULP_SCREENSHOT` in the
/// standalone). A launch without the variable installs nothing.
void install_ui_proof(pulp::state::StateStore &store, BeadsyEditor &editor);

} // namespace beadsy
