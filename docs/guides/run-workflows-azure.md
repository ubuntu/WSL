# How to run your WSL GitHub workflow on Azure
> Read more: [How we improved testing Ubuntu on WSL – and how you can too!](https://ubuntu.com/blog/improved-testing-ubuntu-wsl)

Most of the time, what works on Ubuntu desktop works on WSL as well. However, there are some exceptions. Furthermore, you may want to test software that lives both on Windows and inside WSL. In these cases, you may want to run your automated testing on a Windows machine with WSL rather than a regular ubuntu machine.

There exist Windows GitHub runners, but they do not support the latest version of WSL. The reason is that WSL is now a Microsoft Store application, which requires a logged-in user. GitHub runners, however, run as a service. This means that they are not on a user session, hence they cannot run WSL (or any other store application).
> Read more: [What’s new in the Store version of WSL?](https://devblogs.microsoft.com/commandline/the-windows-subsystem-for-linux-in-the-microsoft-store-is-now-generally-available-on-windows-10-and-11/)

## Summary

We propose you run your automated tests on a Windows virtual machine hosted on Azure. This machine will run the GitHub actions runner not as a service, but as a command-line application.

## Step-by-step

This guide will show you how to set up an Azure VM to run your WSL workflows.

1.	Create a Windows 11 VM on Azure: follow Azure's instructions, no special customisation is necessary.
	> Note: You can use any other hosting service. We use Azure in this guide because that is what we use for our CI.

1.	Install WSL with `wsl --install`.

1.	Enable automatic logon: use the registry to set up your machine to log on automatically. [Explanation here](https://learn.microsoft.com/en-us/troubleshoot/windows-server/user-profiles-and-logon/turn-on-automatic-logon).

1.	Add your runner to your repository: head to your repository's page on GitHub > Settings > Actions > Runners > New self-hosted runner.
	Follow the instructions. Make sure you do not enable running it as a service.

1.	Set up your runner as a startup application:
	1. Go to the directory you installed the GitHub runner.
	2. Right-click on the `run.cmd` file, and click _Show more options_ > _Send to_ > _Desktop (create shortcut)_.
	3. Press Win+R, type `shell:startup` and press **OK**. A directory will open.
	4. Find the shortcut in the desktop and drag it to the startup directory.

1.	Set up your repository secrets
	To add a new secret, head to your repository's page on GitHub > Settings > Secrets > Actions > New repository secret.
	You'll need the following secret:
	- `AZURE_VM_CREDS`: See the documentation [here](https://github.com/Azure/login#configure-deployment-credentials).

1.	Create your GitHub workflow. This workflow must have at least three jobs which depend each on the previous one.
	1. Start up the VM
	1. Your workflow(s)
	1. Stop the VM

	It is also recommended to add a `concurrency` directive to prevent different workflows from interleaving steps 1 and 3.

1.	Use our actions. We developed some actions to help you build your workflow. They are documented in the [WSL GitHub actions reference](reference::actions).

## Example repositories
The following repositories use some variation of the workflow explained here.
- [Ubuntu/WSL-example hello world example](https://github.com/ubuntu/wsl-actions-example/blob/main/.github/workflows/test_wsl.yaml)
- [Ubuntu/WSL-example cloud-init testing](https://github.com/ubuntu/wsl-actions-example/blob/main/.github/workflows/test_cloud_init.yaml)
- [Ubuntu/WSL end-to-end tests](https://github.com/ubuntu/WSL/blob/main/.github/workflows/e2e.yaml)
<!-- [Canonical/Ubuntu-Pro-for-WSL end-to-end tests](https://github.com/canonical/ubuntu-pro-for-wsl/blob/main/.github/workflows/qa-azure.yaml) -->