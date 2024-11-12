!/bin/bash
set -e

# Export the built git-crypt binary to PATH
export PATH="$PWD:$PATH"

# Create a temporary directory for testing
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT
cd $TEST_DIR

# Configure git
git init
git config user.email "fake-email@gmail.com"
git config user.name "Fake Name"

# Initialize git-crypt
git crypt init

# Set up .gitattributes
echo "*.txt filter=git-crypt diff=git-crypt" > .gitattributes

# Create test files
echo "Hello, world!" > nonempty.txt

# Add and commit files
git add .gitattributes nonempty.txt
git commit -m 'Add files'

# Lock files
git crypt lock

