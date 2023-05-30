# Github actions for WSL
You may be here looking for a way to test your applications on WSL via Github actions. If that is
the case, you're in luck! Look no further that [../workflows/wsl-example.yaml](../workflows/wsl-example.yaml).
The setup process is a bit involved, and this readme will guide you through it.

## Why so convoluted?
Github runners do not support WSL. You'll need a Windows Azure machine. You'd think that you can
use it as a self-hosted runner, but no. The problem is that Github connects to the runner logged
on as session 0. The consequence of that is that you cannot use the store version of WSL--the
latest (and recommended) version--.

## Our actions
- `wsl-install` installs WSL from the store, plus a distro of your choice.
- `wsl-checkout` clones your repo into WSL.
- `wsl-bash` runs arbitrary bash scripts in a WSL distro.

## Setup

### 1. Create a Windows 11 VM on Azure
Just follow Azure's instructions, no special customization is necessary.

### 2. Prepare your runner
Install the following Microsoft utilities.
- Enable Hyper-V to be able to use WSL. [Documentation here](https://learn.microsoft.com/en-us/azure/lab-services/how-to-enable-nested-virtualization-template-vm-using-script).
- Install winget
- Install Az CLI

### 3. Add your runner to your repo
Head to your repository's page on Github > Settings > Actions > Runners > New self-hosted runner.
Follow the instructions. Make sure you enable running as a service.

### 4. Set your runner as a startup application
Use the registry to set up your machine to log on automatically. [Explanation here](https://learn.microsoft.com/en-us/troubleshoot/windows-server/user-profiles-and-logon/turn-on-automatic-logon).
Then, add the `actions-runner\run.cmd` script to your startup applications.

### 5. Get secrets
To add a new secret, head to your repository's page on Github > Settings > Secrets > Actions > New repository secret.
You'll need the following secrets:
- AZURE_VM_CREDS: See the documentation [here](https://github.com/Azure/login#configure-deployment-credentials).

### 5. Create a github action
For an example, check out [wsl-example.yaml](../workflows/wsl-example.yaml)
