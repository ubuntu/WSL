# Ubuntu WSL

Install a complete Ubuntu terminal environment in minutes with Windows Subsystem for Linux (WSL). Develop cross-platform applications, improve your data science or web development workflows and manage IT infrastructure without leaving Windows.

Key features:
  - Efficient command line utilities including bash, ssh, git, apt, npm, pip and many more
  - Manage Docker containers with improved performance and startup times
  - Leverage GPU acceleration for AI/ML workloads with NVIDIA CUDA
  - A consistent development to deployment workflow when using Ubuntu in the cloud
  - 5 years of security patching with Ubuntu Long Term Support (LTS) releases

For more information about Ubuntu WSL and how Canonical supports developers please visit:

https://ubuntu.com/wsl

# Ubuntu's applications on WSL
These are the applications we develop and maintain:
- [Ubuntu](https://apps.microsoft.com/store/detail/ubuntu/9PDXGNCFSCZV) ships the latest stable LTS release of Ubuntu. When new LTS versions are released, Ubuntu can be upgraded once the first point release is available
- [Ubuntu 18.04.5 LTS](https://apps.microsoft.com/store/detail/ubuntu-18045-lts/9PNKSF5ZN4SW), [Ubuntu 20.04.5 LTS](https://apps.microsoft.com/store/detail/ubuntu-20045-lts/9MTTCL66CPXJ), [Ubuntu 22.04.1 LTS](https://apps.microsoft.com/store/detail/ubuntu-22041-lts/9PN20MSR04DW) are the LTS versions of Ubuntu and receive updates for five years. Upgrades to future LTS releases will not be proposed.
- [Ubuntu (Preview)](https://apps.microsoft.com/store/detail/ubuntu-preview/9P7BDVKVNXZ6) is a daily build of the latest development version of Ubuntu previewing new features as they are developed. It does not receive the same level of QA as stable releases and should not be used for production workloads.

# This repository
This repository contains code relevant to Ubuntu on WSL. It builds on top of [Microsoft's reference implementation](https://github.com/microsoft/WSL-DistroLauncher). Head there for any documentation needs.

# Issues & Contact
Any bugs or problems discovered with the Launcher should be filed in this project's Issues list. You may alternatively submit them to [Microsoft's repository](https://github.com/microsoft/WSL-DistroLauncher/issues) if you believe the issue is not particular to Ubuntu. We also have a page on Launchpad 