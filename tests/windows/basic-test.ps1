# tests/windows/basic-test.ps1
$ErrorActionPreference = "Stop"

# Export the built git-crypt.exe to PATH
$env:PATH = "$PWD;" + $env:PATH

# Create a temporary directory for testing
$TEMP_DIR = [System.IO.Path]::GetTempPath()
$TEST_DIR = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
Write-Host "TEST_DIR: $TEST_DIR"

New-Item -ItemType Directory -Path $TEST_DIR | Out-Null
Push-Location $TEST_DIR

try {
    # Configure git
    git config --global init.defaultBranch main
    git init
    git config user.email "fake-email@gmail.com"
    git config user.name "Fake Name"
    # Enable worktree configuration to allow git-crypt to work in worktrees
    git config extensions.worktreeConfig true  

    # Initialize git-crypt
    git crypt init
    # export the key to $TEMP_DIR
    git crypt export-key $TEMP_DIR/key.gitcrypt  
    # Set up .gitattributes
    Set-Content -Path .gitattributes -Value "*.txt filter=git-crypt diff=git-crypt"

    # Create test files
    Set-Content -Path nonempty.txt -Value "Hello, world!"

    # Add and commit files
    git add .gitattributes nonempty.txt
    git commit -m 'Add files'
    write-host "Locking files with git-crypt..."
    # Lock files
    git crypt lock

    # Verify that nonempty.txt is encrypted
    Write-Host "Current directory: $(Get-Location)"
    $nonemptyFilePath = "$TEST_DIR\nonempty.txt"
    Write-Host "nonempty.txt path: $nonemptyFilePath"

    $bytes = [System.IO.File]::ReadAllBytes($nonemptyFilePath)[0..8]

    # Convert the bytes to a string
    $headerString = [System.Text.Encoding]::ASCII.GetString($bytes)

    write-host "Header: $headerString"
    # Check if the header matches the git-crypt magic header
    if ($headerString -eq "`0GITCRYPT") {
        Write-Host "nonempty.txt is encrypted"
    } else {
        Write-Error "nonempty.txt is not encrypted"
    }

    # Unlock files
    Write-Host "Unlocking files with git-crypt..."
    git crypt unlock "$TEMP_DIR\key.gitcrypt"

    # Verify that nonempty.txt is decrypted correctly
    $content = Get-Content -Path $nonemptyFilePath
    if ($content -eq "Hello, world!") {
        Write-Host "nonempty.txt is decrypted correctly"
    } else {
        Write-Error "nonempty.txt is not decrypted correctly"
    }

    echo "::notice:: ✅ Passed Basic Test"

    # Create a new worktree
    $WORKTREE_DIR = Join-Path $TEMP_DIR "worktree"
    git worktree add -b test-wt $WORKTREE_DIR

    # Switch to the worktree directory
    Push-Location $WORKTREE_DIR

    # Check git-crypt status
    git crypt status

    # Create and commit a test file in the worktree
    Set-Content -Path "nonempty2.txt" -Value "Hello from worktree!"
    git add nonempty2.txt
    git commit -m 'Add files to worktree'

    # Lock files in the worktree
    git crypt lock

    # Verify that nonempty2.txt is encrypted in the worktree
    $nonemptyFilePath = "$WORKTREE_DIR\nonempty2.txt"
    $bytes = [System.IO.File]::ReadAllBytes($nonemptyFilePath)[0..8]
    $headerString = [System.Text.Encoding]::ASCII.GetString($bytes)
    if ($headerString -eq "`0GITCRYPT") {
        Write-Host "nonempty2.txt is encrypted in worktree"
    } else {
        Write-Error "nonempty2.txt is not encrypted in worktree"
    }

    # Unlock files in the worktree
    git crypt unlock "$TEMP_DIR\key.gitcrypt"

    # Verify that nonempty2.txt is decrypted correctly
    $content = Get-Content -Path $nonemptyFilePath
    if ($content -eq "Hello from worktree!") {
        Write-Host "nonempty2.txt is decrypted correctly in worktree"
    } else {
        Write-Error "nonempty2.txt is not decrypted correctly in worktree"
    }

    # Lock files in the worktree again
    git crypt lock

    # Return to the original directory
    Pop-Location

    # Verify that original repository remains decrypted
    $originalFilePath = "$TEST_DIR\nonempty.txt"
    $content = Get-Content -Path $originalFilePath
    if ($content -eq "Hello, world!") {
        Write-Host "nonempty.txt remains decrypted in original repo"
    } else {
        Write-Error "nonempty.txt is not decrypted correctly in original repo"
    }

    Write-Host "::notice:: ✅ Passed worktree test"

} finally {
    Pop-Location
    Remove-Item -Recurse -Force $TEST_DIR
}

