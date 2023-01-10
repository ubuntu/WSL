# Github actions for WSL
You may be here looking for a way to test your applications on WSL via Github actions. If that is
the case, you're in luck! Look no further that [../workflows/wsl-example.yaml](../workflows/wsl-example.yaml).
The setup process is a bit complicated, and this readme will guide you through it.

## Why so convoluted?
Github runners do not support WSL. You'll need a Windows Azure machine. You'd think that you can
use it as a self-hosted runner, but no. The problem is that Github connects to the runner logged
on as session 0. The consequence of that is that you cannot use the store version of WSL--the
latest (and recommended) version--.

## Our actions
VM management:
- `vm-setup`: Starts an Azure VM and connects to it via RDP to create session 2.
- `vm-stop`: Stops the Azure VM.
Feel free to use your own setup and stop actions if your workflows are more complicated, all you
need is the VM with a session greater than 1 active.

The following actions connect to this session greater than 1 to perform various tasks:
- `wsl-install` installs WSL from the store, plus a distro of your choice.
- `wsl-checkout` clones your repo into WSL.
- `wsl-bash` runs arbitrary bash scripts in a WSL distro.
- `wsl-powershell` runs arbitrary powershell scripts in Windows, with WSL available.

## Setup
Setting up your runner can be a bit complicated, so the rest of the document explains how to do so.

### 1. Create a VM on Azure
Just follow Azure's instructions. You'll need to allow Remote-Desktop connections. You can leave
all other options  as defaults or customize at your leisure. Take note of:
- The name of the VM
- Its resource group
- The name of the user
- The password of the user

This information will be needed later.

### 2. Prepare your runner
Install the following Microsoft utilities. Make sure you don't install them from the Microsoft
Store, as Github connects via session 0 which means that store applications cannot run.
- Enable Hyper-V to be able to use WSL. See the documentation [here](https://learn.microsoft.com/en-us/azure/lab-services/how-to-enable-nested-virtualization-template-vm-using-script).
- Install winget
- Install Az CLI
- Sysinternals:
  * Download it and unzip it to `C:\SysinternalsSuite`. You can find downloads [here](https://learn.microsoft.com/en-us/sysinternals/downloads/sysinternals-suite). 
  * Run `C:\SysinternalsSuite\psexec.exe` to accept the terms and conditions. This requires using
    a GUI, you can use Remote Desktop for instance.
  * Run `C:\SysinternalsSuite\logonsessions.exe` to accept the terms and conditions. This requires
    using a GUI as well.

### 3. Add your runner to your repo
Head to your repository's page on Github > Settings > Actions > Runners > New self-hosted runner.
Follow the instructions. Make sure you enable running as a service.

### 4. Get secrets
To add a new secret, head to your repository's page on Github > Settings > Secrets > Actions > New repository secret.
You'll need the following secrets:
- AZURE_VM_CREDS: See the documentation [here](https://github.com/Azure/login#configure-deployment-credentials).
- AZURE_VM_AUTHORITY: The public IP address for the runner, with the port to connect to via RDP.
- AZURE_VM_UN: The windows user's name.
- AZURE_VM_UP: The user's password.

### 5. Create a github action
For an example, check out [wsl-example.yaml](../workflows/wsl-example.yaml)