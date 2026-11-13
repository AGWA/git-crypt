#!/usr/bin/env bash
#
# Creates the tag and a draft GitHub release for a new version, for
# .github/workflows/release.yml, which then attaches the binaries and
# publishes the release.
#
# Usage: create-release.sh VERSION COMMIT
#
# Run it from a checkout of COMMIT.  VERSION must equal the VERSION in
# git-crypt.hpp, and NEWS.md must have a "## vVERSION" section, which
# becomes the release notes.  The tag is named VERSION (e.g. "0.9.0"), like
# upstream's tags.  If the tag already exists (pushed by hand, or left by an
# earlier failed run), it must point at COMMIT.
#
# The GitHub repository is taken from $GITHUB_REPOSITORY.  gh must be
# authenticated (GH_TOKEN) with permission to write contents.

set -euo pipefail

if [ $# -ne 2 ]; then
	echo "Usage: $0 VERSION COMMIT" >&2
	exit 2
fi
version=$1 commit=$2
repo=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}

source_version=$(sed -n 's/^#define VERSION "\(.*\)"$/\1/p' git-crypt.hpp)
if [ "$version" != "$source_version" ]; then
	echo "::error::git-crypt.hpp has VERSION \"$source_version\", not \"$version\"; update it before releasing"
	exit 1
fi

notes=$(mktemp)
trap 'rm -f "$notes"' EXIT
# The section runs from "## vVERSION" (optionally followed by " (date)") to
# the next "## " heading.
awk -v heading="## v$version" '
	$0 == heading || index($0, heading " ") == 1 { found = 1; next }
	found && /^## / { exit }
	found { print }
' NEWS.md > "$notes"
if ! grep -q '[^[:space:]]' "$notes"; then
	echo "::error::NEWS.md has no \"## v$version\" section; rename \"## Unreleased\" before releasing"
	exit 1
fi

if gh api "repos/$repo/git/ref/tags/$version" --silent 2>/dev/null; then
	tagged=$(gh api "repos/$repo/commits/tags/$version" --jq .sha)
	if [ "$tagged" != "$commit" ]; then
		echo "::error::tag $version already exists at $tagged, not $commit"
		exit 1
	fi
	echo "Using the existing tag $version."
else
	gh api "repos/$repo/git/refs" -f ref="refs/tags/$version" -f sha="$commit" --silent
	echo "Created tag $version at $commit."
fi

gh release create "$version" --repo "$repo" --draft --verify-tag \
	--title "$version" --notes-file "$notes"
echo "Created draft release $version."
