#!/bin/zsh
# Sign, package, notarize and staple one installer set from a build directory.
# Usage: release_signed.sh <build-dir> <version> <out-dir>
# Secrets come from ~/.config/pulp (pulp-sign.sh + secrets/notary.env); nothing
# here prompts. The out dir must be new or empty.
set -euo pipefail
BUILD=${1:?build dir}; VERSION=${2:?version}; OUT=${3:?out dir}
HERE=${0:a:h}
STAGE="$OUT/signed-stage"
mkdir -p "$STAGE/AU" "$STAGE/VST3" "$STAGE/CLAP" "$OUT/signed"
NAMES=("Meru" "Meru FX"); PKG="$OUT/signed/Meru-$VERSION.pkg"
for n in "${NAMES[@]}"; do
  ditto "$BUILD/$n.app" "$STAGE/$n.app"
  ditto "$BUILD/AU/$n.component" "$STAGE/AU/$n.component"
  ditto "$BUILD/VST3/$n.vst3" "$STAGE/VST3/$n.vst3"
  ditto "$BUILD/CLAP/$n.clap" "$STAGE/CLAP/$n.clap"
done
~/.config/pulp/pulp-sign.sh "$STAGE"/*.app "$STAGE"/AU/*.component "$STAGE"/VST3/*.vst3 "$STAGE"/CLAP/*.clap
for b in "$STAGE"/*.app "$STAGE"/AU/*.component "$STAGE"/VST3/*.vst3 "$STAGE"/CLAP/*.clap; do
  codesign --verify --deep --strict "$b" && echo "verified: $(basename "$b")"
done
"$HERE/build_test_pkg.sh" "$STAGE" "$VERSION" "$OUT/signed"
BUILT=$(ls -t "$OUT"/signed/*.pkg | head -1)
[[ "$BUILT" == "$PKG" ]] || mv "$BUILT" "$PKG"
~/.config/pulp/pulp-sign.sh --installer "$PKG"
set -a; . ~/.config/pulp/secrets/notary.env; set +a
xcrun notarytool submit "$PKG" --key "$PULP_NOTARY_KEY_PATH" --key-id "$PULP_NOTARY_KEY_ID" \
  --issuer "$PULP_NOTARY_ISSUER_ID" --wait
xcrun stapler staple "$PKG"
spctl --assess --type install -v "$PKG"
shasum -a 256 "$PKG"
ls -la "$PKG"
