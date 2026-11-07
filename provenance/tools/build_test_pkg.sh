#!/bin/zsh
# Build an installer from the worktree's built bundles, matching the layout of
# the earlier Beadsy releases (standalone apps, AU, VST3, CLAP for both the
# instrument and the FX). Unsigned unless SIGN_INSTALLER_IDENTITY is set.
# Usage: build_test_pkg.sh <build-dir> <version> <out-dir>
set -euo pipefail
BUILD=${1:?build dir}; VERSION=${2:?version}; OUT=${3:?out dir}
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/beadsy-pkg-XXXXXX")
mkdir -p "$OUT"
typeset -a ARGS
component() { # name bundle install-location
  local name="$1"
  local bundle="$2"
  local location="$3"
  local root="$STAGE/root-$name"
  mkdir -p "$root"
  ditto "$bundle" "$root/$(basename "$bundle")"
  pkgbuild --root "$root" --install-location "$location" \
    --identifier "com.independent-audio.beadsy.$name" --version "$VERSION" \
    "$STAGE/$name.pkg" >/dev/null
  ARGS+=(--package "$STAGE/$name.pkg")
}
component app "$BUILD/Meru.app" /Applications
component fx-app "$BUILD/Meru FX.app" /Applications
component au "$BUILD/AU/Meru.component" /Library/Audio/Plug-Ins/Components
component fx-au "$BUILD/AU/Meru FX.component" /Library/Audio/Plug-Ins/Components
component vst3 "$BUILD/VST3/Meru.vst3" /Library/Audio/Plug-Ins/VST3
component fx-vst3 "$BUILD/VST3/Meru FX.vst3" /Library/Audio/Plug-Ins/VST3
component clap "$BUILD/CLAP/Meru.clap" /Library/Audio/Plug-Ins/CLAP
component fx-clap "$BUILD/CLAP/Meru FX.clap" /Library/Audio/Plug-Ins/CLAP
PKG="$OUT/Meru-$VERSION.pkg"
if [[ -n "${SIGN_INSTALLER_IDENTITY:-}" ]]; then
  productbuild "${ARGS[@]}" --sign "$SIGN_INSTALLER_IDENTITY" ${SIGN_KEYCHAIN:+--keychain "$SIGN_KEYCHAIN"} "$PKG" >/dev/null
else
  productbuild "${ARGS[@]}" "$PKG" >/dev/null
fi
rm -rf "$STAGE"
shasum -a 256 "$PKG"
