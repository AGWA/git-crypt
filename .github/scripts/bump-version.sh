#!/usr/bin/env bash
#
# Bumps the version for a new release, for
# .github/workflows/prepare-release.yml, and prints the new version.
#
# Usage: bump-version.sh major|minor|patch
#
# Run it from the root of the repository.  It increments the VERSION in
# git-crypt.hpp (MAJOR.MINOR.PATCH) and updates the other places that name
# the current version: the "## Unreleased" heading of NEWS.md becomes
# "## vVERSION (today's date)", plus the man page and README.md.  It fails,
# without changing anything, if NEWS.md has no non-empty "## Unreleased"
# section or one of those places isn't where it expects.

set -euo pipefail

die ()
{
	echo "::error::$*" >&2
	exit 1
}

# Usage: edit FILE SED-EXPRESSION (extended regex); fails unless FILE changes.
edit ()
{
	local file=$1 expr=$2
	sed -E "$expr" "$file" > "$tmp/$file"
	if cmp -s "$file" "$tmp/$file"; then
		die "couldn't update the version in $file"
	fi
}

if [ $# -ne 1 ]; then
	echo "Usage: $0 major|minor|patch" >&2
	exit 2
fi
bump=$1

current=$(sed -n 's/^#define VERSION "\(.*\)"$/\1/p' git-crypt.hpp)
if [[ ! $current =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
	die "VERSION \"$current\" in git-crypt.hpp isn't MAJOR.MINOR.PATCH"
fi
major=${BASH_REMATCH[1]} minor=${BASH_REMATCH[2]} patch=${BASH_REMATCH[3]}
case "$bump" in
	major)	next="$((major + 1)).0.0" ;;
	minor)	next="$major.$((minor + 1)).0" ;;
	patch)	next="$major.$minor.$((patch + 1))" ;;
	*)	echo "Usage: $0 major|minor|patch" >&2; exit 2 ;;
esac
date=$(date -u +%Y-%m-%d)

unreleased=$(awk '
	$0 == "## Unreleased" { found = 1; next }
	found && /^## / { exit }
	found { print }
' NEWS.md)
if ! grep -q '[^[:space:]]' <<< "$unreleased"; then
	die "NEWS.md has no \"## Unreleased\" section listing the changes in $next"
fi

# Write every file to a temporary directory first, so that nothing changes
# unless every edit succeeds.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/man"
cur=${current//./\\.}
edit git-crypt.hpp "s/^#define VERSION \"$cur\"\$/#define VERSION \"$next\"/"
edit NEWS.md "s/^## Unreleased\$/## v$next ($date)/"
edit man/git-crypt.xml "s|<productname>git-crypt $cur</productname>|<productname>git-crypt $next</productname>|"
edit README.md "
	s/^(The latest version of git-crypt is )\[$cur\](\(NEWS\.md\), released on)\$/\1[$next]\2/
	s/^[0-9]{4}-[0-9]{2}-[0-9]{2}(\.  git-crypt aims to be bug-free)/$date\1/
"
if [ "$(diff README.md "$tmp/README.md" | grep -c '^>')" -ne 2 ]; then
	die "couldn't update both the version and the date in README.md"
fi

for file in git-crypt.hpp NEWS.md man/git-crypt.xml README.md; do
	cat "$tmp/$file" > "$file"
done
echo "$next"
