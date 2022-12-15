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
You are welcome to [create a new issue](https://github.com/ubuntu/WSL/issues/new/choose) on this repository if you find bugs you believe are particular to Ubuntu running on WSL.

You may otherwise head over to [Microsoft's WSL page](https://github.com/microsoft/WSL/issues/) on GitHub if your report is not specific to Ubuntu but rather more general to WSL.

We also have a [page on Launchpad](https://launchpad.net/ubuntuwsl); we check it less often but you may find part of the community there. [Ubuntu's Discourse page](https://discourse.ubuntu.com/c/wsl2/27) has news and interesting discussions about Ubuntu on WSL.