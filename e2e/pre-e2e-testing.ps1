# Builds and installs the appx for end-to-end testing.
#
# Parameters:
#   AppID = UbuntuPreview, Ubuntu22.04LTS etc etc.
#   RootFsX64 = URL for the rootfs used for this build
#
# Preconditions:
#   PWD=$env:LAUNCHER_REPO_ROOT
#   No ARM64 testing
#
# Postconditions:
#   e2e testing enabled appx is installed.

param (
    [Parameter(Mandatory=$true, ValueFromPipeline=$true)]
    [string]$AppID,
    [Parameter(Mandatory=$true, ValueFromPipeline=$true)]
    [string]$RootFsX64
)

function FailFast($exitCode){
    If ($exitCode) {
        exit $exitCode
    }
}

#
# Main script entry point
#

# Builds the meta build system
go build ./wsl-builder/prepare-build
FailFast ($LastExitCode)

# Prepares the build system for the desired AppID
./prepare-build prepare build.txt $AppID "$RootFsX64::amd64"
FailFast ($LastExitCode)

# Remove ARM64 contents out of the solution file
Set-Content -Path "DistroLauncher.sln" -Value (get-content -Path "DistroLauncher.sln" | Select-String -Pattern 'ARM64' -NotMatch)

# Actions should inherit Secrets even in the case of self-hosted runners. Not touching it here.

# Copies the ui-driver.props file to force MSBuild to bundle that binary inside the appx.
cp ./e2e/ui-driver/ui-driver.props DistroLauncher-Appx/

# Build the appx bundle for E2E testing
msbuild .\DistroLauncher.sln /t:Build /m /nr:false /p:Configuration=Debug /p:AppxBundle=Always /p:AppxBundlePlatforms="x64" /p:UapAppxPackageBuildMode=SideloadOnly -verbosity:normal /p:OOBE_E2E_TESTING=True
FailFast ($LastExitCode)

# Install the appx
./AppPackages/Ubuntu/*/Install.ps1 -Force
FailFast ($LastExitCode)
