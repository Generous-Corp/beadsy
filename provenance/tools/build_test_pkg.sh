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
# The first component carries a preinstall step that removes the bundles the
# project shipped under its former name (Beadsy, Beadsy Lab, Beadsy FX,
# Beadsy Lab FX). They share the plug-in identities with the Meru bundles,
# so leaving them in place would register each identity twice.
SCRIPTS="$STAGE/scripts"
mkdir -p "$SCRIPTS"
cat > "$SCRIPTS/preinstall" <<'PRE'
#!/bin/sh
for dir in /Library/Audio/Plug-Ins/Components /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/CLAP /Applications; do
  for old in "Beadsy" "Beadsy FX" "Beadsy Lab" "Beadsy Lab FX"; do
    for ext in component vst3 clap app; do
      rm -rf "$dir/$old.$ext"
    done
  done
done
console_user=$(stat -f %Su /dev/console 2>/dev/null)
if [ -n "$console_user" ] && [ "$console_user" != root ]; then
  home=$(dscl . -read "/Users/$console_user" NFSHomeDirectory 2>/dev/null | awk '{print $2}')
  for old in "Beadsy" "Beadsy FX" "Beadsy Lab" "Beadsy Lab FX"; do
    [ -n "$home" ] && rm -rf "$home/Applications/$old.app" "$home/Library/Audio/Plug-Ins/Components/$old.component" "$home/Library/Audio/Plug-Ins/VST3/$old.vst3" "$home/Library/Audio/Plug-Ins/CLAP/$old.clap"
  done
fi
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
PRE
chmod 755 "$SCRIPTS/preinstall"
component_with_scripts() { # name bundle install-location
  local name="$1" bundle="$2" location="$3" root="$STAGE/root-$name"
  mkdir -p "$root"
  ditto "$bundle" "$root/$(basename "$bundle")"
  pkgbuild --root "$root" --install-location "$location" --scripts "$SCRIPTS" \
    --identifier "com.independent-audio.beadsy.$name" --version "$VERSION" \
    "$STAGE/$name.pkg" >/dev/null
  ARGS+=(--package "$STAGE/$name.pkg")
}
component_with_scripts app "$BUILD/Meru.app" /Applications
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
