#!/bin/bash
set -e
REPO_HOME="$PWD"
# Export the built git-crypt binary to PATH
export PATH="$PWD:$PATH"

# Create a temporary directory for the main repository
REPO_DIR="$(mktemp -d)"
echo "REPO_DIR: $REPO_DIR"

# Create a temporary directory for the worktree
WORKTREE_DIR="$(mktemp -d)"
echo "WORKTREE_DIR: $WORKTREE_DIR"

# Create a temporary directory for the key
KEY_DIR="$(mktemp -d)"
echo "KEY: $KEY_DIR"

# Ensure cleanup on exit
cleanup() {
    rm -rf "$REPO_DIR" "$WORKTREE_DIR" "$KEY_DIR"
}
trap cleanup EXIT

# Change to the repository root
cd "$REPO_DIR"

# Configure git
git config --global init.defaultBranch main
git init
git config user.email "fake-email@gmail.com"
git config user.name "Fake Name"
# Enable worktree configuration to allow git-crypt to work in worktrees
git config extensions.worktreeConfig true  
# Initialize git-crypt
git crypt init
# export key
git crypt export-key "$KEY_DIR/key.gitcrypt"

# Set up .gitattributes
echo "*.txt filter=git-crypt diff=git-crypt" > .gitattributes

# Create test files
echo "Hello, world!" > nonempty.txt

# Add and commit files
git add .gitattributes nonempty.txt
git commit -m 'Add files'

# Lock files
git crypt lock

if xxd nonempty.txt | grep -q 'GITCRYPT'; then
  echo "nonempty.txt is encrypted"
else
  echo "nonempty.txt is not encrypted"
  exit 1
fi

# Unlock files
git crypt unlock "$KEY_DIR/key.gitcrypt"

# Verify that files are decrypted
if [ "$(cat nonempty.txt)" = "Hello, world!" ]; then
  echo "nonempty.txt is decrypted correctly"
else
  echo "nonempty.txt is not decrypted correctly"
  exit 1
fi

echo "::notice:: ✅ Passed basic test"
echo "::debug:: starting worktree test"

# Create a new branch and worktree while keeping the main repository unlocked
echo "Creating a new worktree..."
git worktree add -b test-wt "$WORKTREE_DIR"

# Check git-crypt status in the worktree
echo "Checking git-crypt status in worktree..."

cd "$WORKTREE_DIR"
git crypt status

# Create and commit a test file in the worktree
echo "Hello from worktree!" > "$WORKTREE_DIR/nonempty2.txt"
git add nonempty2.txt
git commit -m 'Add files to worktree'

# Lock files in the worktree, which should not affect the original folder
echo "Locking files in worktree..."
git crypt lock

# Verify that nonempty.txt is encrypted in the worktree
NONEMPTY_FILE="$WORKTREE_DIR/nonempty2.txt"
if xxd "$NONEMPTY_FILE" | grep -q 'GITCRYPT'; then
    echo "nonempty2.txt is encrypted in worktree"
else
    echo "nonempty2.txt is not encrypted in worktree"
    exit 1
fi

# Unlock files in the worktree
echo "Unlocking files in worktree..."
git crypt unlock "$KEY_DIR/key.gitcrypt"

# Verify that nonempty.txt is decrypted correctly in the worktree
CONTENT=$(cat "$NONEMPTY_FILE")
if [ "$CONTENT" = "Hello from worktree!" ]; then
    echo "nonempty2.txt is decrypted correctly in worktree"
else
    echo "nonempty2.txt is not decrypted correctly in worktree"
    exit 1
fi

git crypt lock

# verify original repo is still decrypted
cd "$REPO_DIR"
if [ "$(cat nonempty.txt)" = "Hello, world!" ]; then
  echo "nonempty.txt is remain decrypted in original repo"
else
  echo "nonempty.txt is not decrypted correctly in original repo"
  exit 1
fi

echo "::notice:: ✅ Passed worktree test"

echo "test compatibility with git-crypt 0.7.0..."

# move to original repo
cd "$REPO_HOME"

# clean git changes
git reset --hard

git crypt unlock "./tests/key.gitcrypt"
# check if tests/fake.test.secrets is decrypted

if xxd "./tests/fake.test.secrets" | grep -q 'GITCRYPT'; then
    echo "fake.test.secrets is encrypted"
    exit 1
else
    echo "fake.test.secrets is decrypted"
    echo "::notice:: ✅ Passed  0.7.0 compatibility test"
fi

