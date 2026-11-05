#!/bin/zsh
# Build an installer from the worktree's built bundles, matching the layout of
# the earlier Beadsy releases (standalone apps, AU, VST3, CLAP for both the
# instrument and the FX). Unsigned unless SIGN_INSTALLER_IDENTITY is set.
# Usage: build_test_pkg.sh <build-dir> <version> <out-dir> [release|lab]
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
SET=${4:-release}
if [[ "$SET" == "lab" ]]; then
  # Exploration bundles only: they install next to the shipping plug-ins.
  component lab-au "$BUILD/AU/Beadsy Lab.component" /Library/Audio/Plug-Ins/Components
  component lab-fx-au "$BUILD/AU/Beadsy Lab FX.component" /Library/Audio/Plug-Ins/Components
  component lab-vst3 "$BUILD/VST3/Beadsy Lab.vst3" /Library/Audio/Plug-Ins/VST3
  component lab-fx-vst3 "$BUILD/VST3/Beadsy Lab FX.vst3" /Library/Audio/Plug-Ins/VST3
  component lab-clap "$BUILD/CLAP/Beadsy Lab.clap" /Library/Audio/Plug-Ins/CLAP
  component lab-fx-clap "$BUILD/CLAP/Beadsy Lab FX.clap" /Library/Audio/Plug-Ins/CLAP
  PKG="$OUT/Beadsy-Lab-$VERSION.pkg"
else
component app "$BUILD/Beadsy.app" /Applications
component fx-app "$BUILD/Beadsy FX.app" /Applications
component au "$BUILD/AU/Beadsy.component" /Library/Audio/Plug-Ins/Components
component fx-au "$BUILD/AU/Beadsy FX.component" /Library/Audio/Plug-Ins/Components
component vst3 "$BUILD/VST3/Beadsy.vst3" /Library/Audio/Plug-Ins/VST3
component fx-vst3 "$BUILD/VST3/Beadsy FX.vst3" /Library/Audio/Plug-Ins/VST3
component clap "$BUILD/CLAP/Beadsy.clap" /Library/Audio/Plug-Ins/CLAP
component fx-clap "$BUILD/CLAP/Beadsy FX.clap" /Library/Audio/Plug-Ins/CLAP
PKG="$OUT/Beadsy-$VERSION.pkg"
fi
if [[ -n "${SIGN_INSTALLER_IDENTITY:-}" ]]; then
  productbuild "${ARGS[@]}" --sign "$SIGN_INSTALLER_IDENTITY" ${SIGN_KEYCHAIN:+--keychain "$SIGN_KEYCHAIN"} "$PKG" >/dev/null
else
  productbuild "${ARGS[@]}" "$PKG" >/dev/null
fi
rm -rf "$STAGE"
shasum -a 256 "$PKG"
