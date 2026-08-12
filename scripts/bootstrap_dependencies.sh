#!/usr/bin/env bash
set -Eeuo pipefail

readonly LIBCSP_REVISION="5a95e5c48826c3602bf4a34aa5d60e2af9e4a004"
readonly LIBPARAM_REVISION="d78da56e7cbc8b6be30d8890b6f01cdf80d0b9b9"
readonly LIBCSP_TAG="csh/2.10"
readonly LIBPARAM_TAG="csh/2.10"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
library_root="$(cd "$script_dir/.." && pwd)"
dependency_root="$library_root/lib"
source_root=""
check_only=false

usage() {
    printf '%s\n' \
        "Usage: bootstrap_dependencies.sh [--source-root DIR] [--check-only]" \
        "" \
        "Clone and verify the exact libcsp and libparam csh/2.10 releases." \
        "" \
        "  --source-root DIR  Clone from DIR/{libcsp,libparam} instead of GitHub." \
        "  --check-only       Verify existing checkouts without changing them." \
        "  -h, --help         Show this help."
}

fail() {
    echo "ERROR: $*" >&2
    exit 2
}

while (($# > 0)); do
    case "$1" in
        --source-root)
            (($# >= 2)) || fail "--source-root needs a directory"
            source_root="$2"
            shift 2
            ;;
        --check-only)
            check_only=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
done

command -v git >/dev/null || fail "git is required"
if [[ -n "$source_root" ]]; then
    [[ -d "$source_root" ]] || fail "source root does not exist: $source_root"
fi

clone_or_verify() {
    local name="$1"
    local github_url="$2"
    local revision="$3"
    local tag="$4"
    local destination="$dependency_root/$name"
    local source="$github_url"
    local created=false

    if [[ -n "$source_root" ]]; then
        source="$source_root/$name"
        [[ -d "$source/.git" ]] || fail "local source is not a Git checkout: $source"
    fi

    if [[ ! -e "$destination" ]]; then
        $check_only && fail "missing dependency: $destination"
        mkdir -p "$dependency_root"
        if [[ -n "$source_root" ]]; then
            git clone --no-hardlinks --no-checkout "$source" "$destination"
        else
            git clone --filter=blob:none --no-checkout "$source" "$destination"
        fi
        created=true
    fi

    [[ -d "$destination/.git" ]] || fail "dependency path is not a Git checkout: $destination"

    local revision_present=true
    git -C "$destination" cat-file -e "${revision}^{commit}" 2>/dev/null || revision_present=false
    local tag_present=true
    git -C "$destination" rev-parse "${tag}^{commit}" >/dev/null 2>&1 || tag_present=false

    if ! $revision_present || ! $tag_present; then
        $check_only && fail "$name does not contain required revision $revision"
        if [[ -z "$source_root" ]]; then
            local current_origin
            current_origin="$(git -C "$destination" remote get-url origin 2>/dev/null || true)"
            if [[ "$current_origin" != "$github_url" ]]; then
                [[ -z "$(git -C "$destination" status --porcelain)" ]] ||
                    fail "$name has local changes; refusing to change its release source"
                git -C "$destination" remote set-url origin "$github_url"
            fi
        fi
        git -C "$destination" fetch origin "refs/tags/$tag:refs/tags/$tag"
    fi

    local tag_revision
    tag_revision="$(git -C "$destination" rev-parse "${tag}^{commit}" 2>/dev/null)" ||
        fail "$name tag $tag is missing"
    [[ "$tag_revision" == "$revision" ]] ||
        fail "$name tag $tag resolves to $tag_revision, expected $revision"

    local head_revision
    head_revision="$(git -C "$destination" rev-parse HEAD 2>/dev/null || true)"
    if $created; then
        git -C "$destination" checkout --detach "$revision"
    elif [[ "$head_revision" != "$revision" ]]; then
        $check_only && fail "$name is at ${head_revision:-<no HEAD>}, expected $revision"
        [[ -z "$(git -C "$destination" status --porcelain)" ]] ||
            fail "$name has local changes; refusing to replace its checkout"
        git -C "$destination" checkout --detach "$revision"
    fi

    head_revision="$(git -C "$destination" rev-parse HEAD)"
    [[ "$head_revision" == "$revision" ]] || fail "$name revision verification failed"
    [[ -z "$(git -C "$destination" status --porcelain --untracked-files=normal)" ]] ||
        fail "$name has local changes; pinned-source verification requires a clean checkout"
    printf '%-9s %s\n' "$name" "$head_revision"
}

clone_or_verify libcsp https://github.com/spaceinventor/libcsp.git "$LIBCSP_REVISION" "$LIBCSP_TAG"
clone_or_verify libparam https://github.com/spaceinventor/libparam.git "$LIBPARAM_REVISION" "$LIBPARAM_TAG"

echo "Dependency revisions verified."
