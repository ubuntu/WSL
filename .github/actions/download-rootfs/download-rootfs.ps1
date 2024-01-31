param (
    [Parameter(Mandatory = $true, HelpMessage = "The path where to download the tarball")]
    [string]$Path,
    [Parameter(Mandatory = $true, HelpMessage = "The URL of the tarball")]
    [Uri]$URL
)

# Global variables
$RootFS = "${Path}"
$Directory = Split-Path -Path "${RootFS}" -Parent
$Checksums = "${Directory}\SHA256SUMS"

# Helper functions

function Get-URIParent {
    param (
        [Parameter(Mandatory=$true)] [Uri]$URI
    )

    $segments = $URI.Segments
    $end = $segments.Length - 2
    $trailing = [string]::Join("", $segments[0..$end])

    return "$($URI.Scheme)://$($URI.Authority)${trailing}"
}

function Remove-File {
    param (
        [Parameter(Mandatory=$true)] [string]$Path
    )
    if (!(Test-Path -Path $Path)) {
        return
    }
    Remove-Item -Path "${Path}" -Force 2>&1 | Out-Null
}

# Test-Checksums returns true if the current rootfs checksum matches the remote one.
# Returns false otherwise, or if any error arises.
function Test-Checksums {
    # Get Hash of the current RootFS
    if (!(Test-Path -Path ${RootFS})) {
        return $false
    }

    $oldSHA256 = (Get-FileHash "${Rootfs}" -Algorithm SHA256).Hash
    if ($oldSHA256 -eq "") {
        Write-Warning "Could not compute hash of ${Rootfs}"
        return $false
    }

    # Download remote checksum
    $checksumURL = "$(Get-URIParent -URI "${URL}")SHA256SUMS"
    pwsh.exe -Command pwsh.exe -Command Invoke-WebRequest -Uri "${checksumURL}" -OutFile "${Checksums}" -Resume -MaximumRetryCount 5 -RetryIntervalSec 2
    if (! $?) {
        Write-Warning "Could not download checksums"
        return $false
    }
    
    if (!(Test-Path "${Checksums}")) {
        Write-Warning "Could not download checksums"
        return $false
    }

    # Parse checksum file
    $image = $URL.Segments[$Url.Segments.Length - 1]
    $pattern = "(\w+)  ${image}"

    $newSHA256 = (Select-String -Pattern "${pattern}" -Path "${Checksums}").Matches[0].Groups[1]
    if ($newSHA256 -eq "") {
        Write-Warning "Could not find $image in checksums file"
        return $false
    }

    # Compare checksums
    if ($oldSHA256 -ne $newSHA256) {
        return $false
    }    
    return $true
}

function main {
    New-Item -Type Directory -Force "${Directory}" 2>&1 | Out-Null

    # Validate checksum
    if ( Test-Checksums ) {
        Write-Warning "Cache hit: skipping download"
        return 0
    }

    # Download Rootfs into temp file
    pwsh.exe -Command Invoke-WebRequest -Uri "${URL}" -OutFile "${RootFS}.tmp" -Resume -MaximumRetryCount 5 -RetryIntervalSec 2
    if ( ! $? ) {
        Write-Error "Error: Failed download"
        return 1
    }

    # Move downloaded tmp file into position
    Move-Item -Force "${RootFS}.tmp" "${RootFS}"
    return 0
}

# Main function
$exitCode = main

# Clean artefacts
Remove-File -Path "${RootFS}.tmp"
Remove-File -Path "${Checksums}"

Exit($exitCode)

