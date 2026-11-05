#pragma once

namespace beadsy {

/// Make the standalone window deliver the click that activates it.
///
/// Pulp's standalone window view keeps AppKit's inspect-surface default and
/// answers NO to acceptsFirstMouse:, so the tap that brings an inactive
/// Beadsy window to the front is swallowed and a toggle needs a second tap.
/// An instrument must act on that first tap, as the plug-in views already
/// do. Until the SDK exposes a first-mouse policy for standalones, replace
/// the answer on the window host view classes compiled into this binary.
void accept_first_mouse_in_standalone();

} // namespace beadsy
