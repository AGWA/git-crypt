#!/usr/bin/env bash
#
# Release asset bookkeeping for .github/workflows/release.yml.
#
# Usage: release-assets.sh expected  VERSION      print the expected binary names
#        release-assets.sh checksums TAG VERSION  (re)generate and upload SHA256SUMS
#        release-assets.sh verify    TAG VERSION  fail unless every expected binary
#                                                 and a matching SHA256SUMS are attached
#
# The GitHub repository is taken from $GITHUB_REPOSITORY.  gh must be
# authenticated (GH_TOKEN).

set -euo pipefail

# Keep in sync with the build matrix in release.yml.
expected_assets ()
{
	local version=$1
	echo "git-crypt-$version-linux-x86_64"
	echo "git-crypt-$version-linux-aarch64"
	echo "git-crypt-$version-x86_64.exe"
	echo "git-crypt-$version-macos-arm64"
	echo "git-crypt-$version-macos-x86_64"
}

sha256 ()
{
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum "$@"
	else
		shasum -a 256 "$@"
	fi
}

repo=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}

case "${1:-}" in
	expected)
		expected_assets "$2"
		;;

	checksums)
		tag=$2 version=$3
		dir=$(mktemp -d)
		trap 'rm -rf "$dir"' EXIT
		gh release download "$tag" --repo "$repo" --dir "$dir" --pattern "git-crypt-$version-*"
		(cd "$dir" && sha256 git-crypt-* > SHA256SUMS && cat SHA256SUMS)
		gh release upload "$tag" --repo "$repo" --clobber "$dir/SHA256SUMS"
		;;

	verify)
		tag=$2 version=$3
		attached=$(gh release view "$tag" --repo "$repo" --json assets --jq '.assets[].name')
		missing=0
		for asset in $(expected_assets "$version") SHA256SUMS; do
			if grep -qxF "$asset" <<< "$attached"; then
				echo "present: $asset"
			else
				echo "::error::release $tag is missing $asset"
				missing=1
			fi
		done
		if [ "$missing" -ne 0 ]; then
			exit 1
		fi

		dir=$(mktemp -d)
		trap 'rm -rf "$dir"' EXIT
		gh release download "$tag" --repo "$repo" --dir "$dir" --pattern "git-crypt-$version-*" --pattern SHA256SUMS
		for asset in $(expected_assets "$version"); do
			if ! grep -q "  $asset\$" "$dir/SHA256SUMS"; then
				echo "::error::SHA256SUMS does not cover $asset"
				exit 1
			fi
		done
		(cd "$dir" && sha256 -c SHA256SUMS)
		echo "All expected assets of release $tag are present and match SHA256SUMS."
		;;

	*)
		echo "Usage: $0 expected VERSION | checksums TAG VERSION | verify TAG VERSION" >&2
		exit 2
		;;
esac
