(reference::distros)=
# Distributions
Our flagship distribution is Ubuntu. This is the one that is installed by default when you install WSL for the first time. However, we develop several flavours. It may be the case that one or more of these flavours fits your needs better.

Each of these flavours corresponds to a different application on the Microsoft Store, and once installed, they'll create different distros in your WSL. These are the applications we develop and maintain:
- [Ubuntu](https://apps.microsoft.com/detail/9PDXGNCFSCZV?hl=en-us&gl=US) ships the latest stable LTS release of Ubuntu. When new LTS versions are released, Ubuntu can be upgraded once the first point release is available.
- [Ubuntu 18.04.6 LTS](https://apps.microsoft.com/detail/9PNKSF5ZN4SW?hl=en-us&gl=US), [Ubuntu 20.04.6 LTS](https://apps.microsoft.com/detail/9MTTCL66CPXJ?hl=en-us&gl=US), and [Ubuntu 22.04.3 LTS](https://apps.microsoft.com/detail/9PN20MSR04DW?hl=en-us&gl=US) are the LTS versions of Ubuntu and receive updates for five years. Upgrades to future LTS releases will not be proposed.
- [Ubuntu (Preview)](https://apps.microsoft.com/detail/9P7BDVKVNXZ6?hl=en-us&gl=US) is a daily build of the latest development version of Ubuntu previewing new features as they are developed. It does not receive the same level of QA as stable releases and should not be used for production workloads.

## Naming

Due to different limitations in different contexts, these applications will have different names in different contexts. Here is a table matching them.

1. App name is the name you'll see in the Microsoft Store and Winget.
2. AppxPackage is the name you'll see in `Get-AppxPackage`.
3. Distro name is the name you'll see when doing `wsl -l -v` or `wsl -l --online`.
4. Executable is the program you need to run to start the distro.

| App name             | AppxPackage name                       | Distro name      | Executable          |
| -------------------- | -------------------------------------- | ---------------- | ------------------- |
| `Ubuntu`             | `CanonicalGroupLimited.Ubuntu`         | `Ubuntu`         | `ubuntu.exe`        |
| `Ubuntu (Preview)`   | `CanonicalGroupLimited.UbuntuPreview`  | `Ubuntu-Preview` | `ubuntupreview.exe` |
| `Ubuntu XX.YY.Z LTS` | `CanonicalGroupLimited.UbuntuXX.YYLTS` | `Ubuntu-XX.YY`   | `ubuntuXXYY.exe`    |
