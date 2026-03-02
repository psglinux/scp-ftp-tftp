#!/usr/bin/env bash
set -euo pipefail

# ===================================================================
# GotiKinesis — Build, Package & Release Management
#
# Keeps only two kinds of packages in release/:
#   release/latest/   — always overwritten with the newest build
#   release/<tag>/    — preserved for every git-tagged commit
#
# Usage:
#   ./scripts/release.sh            # auto-detect platform
#   ./scripts/release.sh --clean    # also wipe latest/ before build
# ===================================================================

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE_DIR="$PROJECT_DIR/release"
CLEAN_LATEST=false

for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN_LATEST=true ;;
    esac
done

# --- Detect platform ---
if [[ "$OSTYPE" == darwin* ]]; then
    PLATFORM="macOS"
    BUILD_DIR="$PROJECT_DIR/build-macos"
    PKG_GLOB="*.dmg"
    CPACK_GEN="DragNDrop"
else
    PLATFORM="Linux"
    BUILD_DIR="$PROJECT_DIR/build"
    PKG_GLOB="*.deb"
    CPACK_GEN="DEB"
fi

echo "===================================="
echo " GotiKinesis Release ($PLATFORM)"
echo "===================================="
echo ""

# --- Configure & build ---
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF

cmake --build . --config Release --parallel

# --- macOS: deploy Qt frameworks into bundle ---
if [[ "$PLATFORM" == "macOS" ]]; then
    MACDEPLOYQT="$(which macdeployqt 2>/dev/null || true)"
    if [ -n "$MACDEPLOYQT" ] && [ -x "$MACDEPLOYQT" ]; then
        echo "Running macdeployqt..."
        "$MACDEPLOYQT" gotikinesis.app
    fi
fi

# --- Package ---
echo ""
echo "Running CPack ($CPACK_GEN)..."
cpack -G "$CPACK_GEN" -C Release

# --- Locate the generated package ---
PKG=""
if [[ "$PLATFORM" == "Linux" ]]; then
    PKG=$(find "$BUILD_DIR" -maxdepth 1 -name "$PKG_GLOB" -printf '%T@ %p\n' 2>/dev/null \
          | sort -rn | head -1 | cut -d' ' -f2-)
else
    PKG=$(ls -t "$BUILD_DIR"/$PKG_GLOB 2>/dev/null | head -1)
fi

if [ -z "$PKG" ]; then
    echo "ERROR: No package ($PKG_GLOB) found in $BUILD_DIR"
    exit 1
fi

BASENAME=$(basename "$PKG")
echo ""
echo "Package: $BASENAME"

# ===================================================================
# Release directory management
# ===================================================================

# 1. Always update latest/
mkdir -p "$RELEASE_DIR/latest"
if $CLEAN_LATEST; then
    rm -f "$RELEASE_DIR/latest/"*
fi
cp -f "$PKG" "$RELEASE_DIR/latest/"
echo "  -> latest/$BASENAME"

# 2. If HEAD is a tagged commit, preserve a copy under the tag name
TAG=$(git -C "$PROJECT_DIR" describe --exact-match --tags HEAD 2>/dev/null || true)
if [ -n "$TAG" ]; then
    mkdir -p "$RELEASE_DIR/$TAG"
    cp -f "$PKG" "$RELEASE_DIR/$TAG/"
    echo "  -> $TAG/$BASENAME"
fi

# 3. Clean: remove directories that are neither latest/ nor a valid git tag
for d in "$RELEASE_DIR"/*/; do
    [ ! -d "$d" ] && continue
    dir_name=$(basename "$d")

    [ "$dir_name" = "latest" ] && continue

    # CPack staging leftovers
    if [ "$dir_name" = "_CPack_Packages" ]; then
        echo "  Removing CPack staging: $dir_name/"
        rm -rf "$d"
        continue
    fi

    # Keep if it matches an existing git tag
    if git -C "$PROJECT_DIR" rev-parse "refs/tags/$dir_name" >/dev/null 2>&1; then
        continue
    fi

    echo "  Removing untagged: $dir_name/"
    rm -rf "$d"
done

# 4. Remove any loose files in release/ root (except .gitkeep)
find "$RELEASE_DIR" -maxdepth 1 -type f ! -name '.gitkeep' -delete 2>/dev/null || true

# --- Summary ---
echo ""
echo "===================================="
echo " release/ contents"
echo "===================================="
ls -R "$RELEASE_DIR" 2>/dev/null || echo "(empty)"
echo ""
echo "Done."
