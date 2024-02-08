# How to run GitHub workflows on WSL
> Read more: [How we improved testing Ubuntu on WSL – and how you can too!](https://ubuntu.com/blog/improved-testing-ubuntu-wsl)

Most of the time, what works on Ubuntu desktop works on WSL as well. However, there are some exceptions. Furthermore, you may want to test software that lives both on Windows and inside WSL. In these cases, you may want to run your automated testing on a Windows machine with WSL rather than a regular ubuntu machine.

There exist Windows GitHub runners, but they do not support the latest version of WSL. The reason is that WSL is now a Microsoft Store application, which requires a logged-in user. GitHub runners, however, run as a service. This means that they are not on a user session, hence they cannot run WSL (or any other store application).
> Read more: [What’s new in the Store version of WSL?](https://devblogs.microsoft.com/commandline/the-windows-subsystem-for-linux-in-the-microsoft-store-is-now-generally-available-on-windows-10-and-11/)

We propose you run your automated tests on a Windows virtual machine hosted on Azure. This machine will run the GitHub actions runner not as a service, but as a command-line application.

1.	Create a Windows 11 VM on Azure
	- Follow Azure's instructions, no special customisation is necessary.
	- Take note of the _resource group_ and _machine name_ that you use.
	> Note: You can use any other hosting service. We use Azure in this guide because that is what we use for our CI.

1.	Log into your VM. You can use remote-desktop with Azure.

1.	Install WSL with `wsl --install`.

1.	Enable automatic logon
	> See more: [Microsoft Learn | Turn on automatic logon in Windows](https://learn.microsoft.com/en-us/troubleshoot/windows-server/user-profiles-and-logon/turn-on-automatic-logon).

1.	Add your runner to your repository
	1. Head to GitHub, navigate to your repository settings, and click _Actions_ > _Runners_ > _New self-hosted runner_ > _Windows_.
	2. Follow the instructions to install the runner in your VM. Make sure you do not enable running it as a service.
	> See more: [GitHub Docs | Adding self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners)

1.	Set up your runner as a startup application:
	1. Go to the directory you installed the GitHub runner.
	2. Right-click on the `run.cmd` file, and click _Show more options_ > _Send to_ > _Desktop (create shortcut)_.
	3. Press Win+R, type `shell:startup` and press **OK**. A directory will open.
	4. Find the shortcut in the desktop and drag it to the startup directory.

1.	Set up your repository secrets
	- Obtain your azure credentials.
      > See more: [Azure/login action | Login With OpenID Connect](https://github.com/Azure/login/blob/master/README.md#login-with-openid-connect-oidc-recommended).
	- Head to GitHub, navigate to your repository settings, and click _Secrets and variables_ > _Actions_ > _New repository secret_. Fill the form with the following information:
    	- Name: Any name you like. In our workflows we use `AZURE_VM_CREDS`.
    	- Secret: Copy-paste the azure credentials.

1.	Create a GitHub workflow.
	- The workflow should have this structure:
		1. Start up the VM: 
			- Configure it so that it runs on a GitHub runner.
			- Use the [Azure/login action](https://github.com/Azure/login) to log into Azure.
			- Use this line to start the machine 
			  ```
			  az vm start --name ${AZ_MACHINE_NAME} --resource-group ${AZ_RESOURCE_GROUP}
			  ```
		2. Run your job
      		- Configure it so that it run on a `self-hosted` runner (your VM).
      		- Set a dependency to the previous step.
		3. Stop the VM
      		- Configure it so that it runs on a GitHub runner.
			- Set a dependency to the previous step.
			- Set it up so that it always runs. Read more: [GitHub Docs | Expressions](https://docs.github.com/en/actions/learn-github-actions/expressions#always).
      		- Log in the same way you did to start the machine.
      		- Use this script to stop the machine:
			  ```
			  az vm deallocate --name ${AZ_MACHINE_NAME} --resource-group ${AZ_RESOURCE_GROUP}
			  ```
	- Add a `concurrency` directive to prevent different workflows from interleaving steps 1 and 3.
	  > See more: [GitHub Docs | Using concurrency](https://docs.github.com/en/actions/using-jobs/using-concurrency).
	- We developed some actions to help you build your workflow. 
	  > See more: [WSL GitHub actions reference](reference::actions).

## Example repositories
The following repositories use some variation of the workflow explained here.
- [Ubuntu/WSL-example hello world example](https://github.com/ubuntu/wsl-actions-example/blob/main/.github/workflows/test_wsl.yaml)
- [Ubuntu/WSL-example cloud-init testing](https://github.com/ubuntu/wsl-actions-example/blob/main/.github/workflows/test_cloud_init.yaml)
- [Ubuntu/WSL end-to-end tests](https://github.com/ubuntu/WSL/blob/main/.github/workflows/e2e.yaml)
<!-- [Canonical/Ubuntu-Pro-for-WSL end-to-end tests](https://github.com/canonical/ubuntu-pro-for-wsl/blob/main/.github/workflows/qa-azure.yaml) -->