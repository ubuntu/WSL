(reference::actions)=
# GitHub actions

(reference::actions::download-rootfs)=
## Download rootfs
Download the latest Rootfs tarball for a particular release of Ubuntu WSL.
This can be used when you need better granularity than what is offered by [wsl-install](reference::actions::wsl-install), or you want to cache the rootfs.

Its arguments are:
- `distros`: a comma-separated list of distros to download. Use the names as shown in WSL. Read more: [Ubuntu WSL distributions](reference::distros). Defaults to `Ubuntu`.
- `path`: the path where to store the tarball. The tarball will end up as `${path}\${distro}.tar.gz`. PowerShell-style environment variables will be expanded. If there already exists a tarball at the download path, a checksum comparison will be made to possibly skip the download.

Example usage:
```yaml
 - name: Download Jammy rootfs
   uses: Ubuntu/WSL/.github/actions/download-rootfs@main
   with:
    distro: Ubuntu-22.04
    path: '${env:UserProfile}\Downloads\rootfs'
```

(reference::actions::wsl-install)=
## WSL install
> See also: [download-rootfs](reference::actions::download-rootfs)

This action installs the Windows Subsystem for Linux application, and optionally an Ubuntu WSL application.

Its arguments are:
- `distro`: Optional argument
  - Blank (default): don't install any Ubuntu WSL distro
  - Distro name: any of the available distros in the Microsoft store. Write its name as shown in WSL. Read more: [Ubuntu WSL distributions](reference::distros)

Example usage:
```yaml
 - name: Install or update WSL
   uses: Ubuntu/WSL/.github/actions/wsl-install@main
   with:
    distro: Ubuntu-20.04
```

## WSL checkout
This action checks out your repository in a WSL distro. If you want to check it out on the Windows file system, use the regular `actions/checkout` action instead. Example usage:

Its arguments are:
- `distro`: an installed WSL distro. Write its name as it would appear on WSL. Read more: [Ubuntu WSL distributions](reference::distros)
- `working-dir`: the path where the repository should be cloned. Set to `~` by default.
- `submodules:`: Whether to fetch sub-modules or not. False by default.

Example usage:
```yaml
 - name: Check out the repository
   uses: Ubuntu/WSL/.github/actions/wsl-checkout@main
   with:
    distro: Ubuntu-20.04
    working-dir: /tmp/github/
    submodules: true
```

## WSL bash
This action runs arbitrary bash code in your distro.

Its arguments are:
 - `distro`: an installed WSL distro. Write its name as it would appear on WSL. Read more: [Ubuntu WSL distributions](reference::distros)
 - `exec`: the script to run.
 - `working-dir`: path to the WSL directory to run the script in. Set to `~` by default.

Example usage:
```yaml
 - name: Install pip
   uses: Ubuntu/WSL/.github/actions/wsl-bash@main
   with:
    distro: Ubuntu-20.04
    working-dir: /tmp/github/
    exec: |
        DEBIAN_FRONTEND=noninteractive sudo apt update
        DEBIAN_FRONTEND=noninteractive sudo apt install python3-pip
```

