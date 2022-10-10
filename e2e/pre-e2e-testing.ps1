# Builds and installs the appx for end-to-end testing.
#
# Parameters:
#   AppID = UbuntuPreview, Ubuntu22.04LTS etc etc.
#   RootFsX64 = URL for the rootfs used for this build
#   CertificateThumbprint = Optional: Thumbprint of the local certificate to use when signing the package.
#
# Preconditions:
#   PWD=$env:LAUNCHER_REPO_ROOT
#   No ARM64 testing
#
# Postconditions:
#   e2e testing enabled appx is installed.

param (
    [Parameter(Mandatory=$true, HelpMessage="UbuntuPreview, Ubuntu22.04LTS etc etc.")]
    [string]$AppID,
    [Parameter(Mandatory=$true, HelpMessage="the URL to fetch the rootfs from used for this build")]
    [string]$RootFsX64,
    [Parameter(Mandatory=$false, HelpMessage="Optional: Thumbprint of the local certificate to use when signing the package.")]
    [string]$CertificateThumbprint
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

# Actions should inherit Secrets even in the case of self-hosted runners. Not touching it here, unless another thumbprint is set.
# Useful for local development.
If ( -Not [string]::IsNullOrEmpty($CertificateThumbprint)) {
    Write-Output "Modifying signing certificate"
    $vcxprojFile = "DistroLauncher-Appx/DistroLauncher-Appx.vcxproj"
    $thumbprintEntry = "<PackageCertificateThumbprint>@</PackageCertificateThumbprint>"
    $wildCard = $thumbprintEntry.replace("@", ".*")
    $replacement = $thumbprintEntry.replace("@", $CertificateThumbprint)

    [regex]::replace($(Get-Content -Path $vcxprojFile), $wildCard, $replacement) | Set-Content -Path $vcxprojFile
}

# Copies the ui-driver.props file to force MSBuild to bundle that binary inside the appx.
cp ./e2e/ui-driver/ui-driver.props DistroLauncher-Appx/

# Build the appx bundle for E2E testing
msbuild ./DistroLauncher.sln /t:Build /m /nr:false /p:Configuration=Debug /p:AppxBundle=Always /p:AppxBundlePlatforms="x64" /p:UapAppxPackageBuildMode=SideloadOnly -verbosity:normal /p:OOBE_E2E_TESTING=True
FailFast ($LastExitCode)

# Install the appx
$OutputDir = $(Get-ChildItem -Directory .\AppPackages\Ubuntu\ | Sort-Object -Property {$_.LastWriteTime} -Descending)[0].Name
Invoke-Expression "./AppPackages/Ubuntu/$OutputDir/Install.ps1 -Force"
FailFast ($LastExitCode)
