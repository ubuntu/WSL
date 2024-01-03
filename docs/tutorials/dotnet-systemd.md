# Run a .Net Echo Bot as a systemd service on Ubuntu WSL
*Authored by Oliver Smith ([oliver.smith@canonical.com](mailto:oliver.smith@canonical.com))*

.Net is an open-source development platform from Microsoft that enables developers to build multi-platform applications from a single codebase.

In this tutorial we demonstrate how easy it is to start working with .Net on Ubuntu WSL by creating a simple chatbot accessible from your Windows host. We also take advantage of WSL's new systemd support to run our bot as a systemd service for easier deployment.

## Requirements

* A PC running Windows 11
* The latest version of the WSL Windows Store application
* Ubuntu, Ubuntu 22.04 LTS or Ubuntu Preview installed

Systemd support is a new feature of WSL and only available on WSL version XX or higher.

You can check your current WSL version by running:

> `> wsl -- version`

In your PowerShell terminal.

To enable systemd on your Ubuntu distribution you need to add the following content to `/etc/wsl.conf`​:

> `[boot]`
> `systemd=true`

Make sure to restart your distribution after you have made this change.

## Install .Net

.Net has recently [been added to the Ubuntu repositories](https://ubuntu.com/blog/install-dotnet-on-ubuntu). This means you can now quickly install a bundle with both the SDK and runtime with a single command:

>`$ sudo apt install dotnet6`

Run `dotnet --version` to confirm that the package was installed successfully.

It is recommended to create a new directory for this project, do so now and navigate to it before proceeding:

>`$ sudo mkdir ~/mybot`
>`$ cd mybot`

## Install and run the Bot Framework EchoBot template

Create a new directory for the project and navigate to it before proceeding:

>`$ sudo mkdir ~/mybot`
>`$ cd mybot`

Once inside we can install the EchoBot C# template by running:

>`$ dotnet new -i Microsoft.Bot.Framework.CSharp.EchoBot`

We can verify the template has been installed correctly by running:

>`$ dotnet new --list`

And looking for the `Bot Framework Echo Bot` template in the list.

![|624x153](https://lh6.googleusercontent.com/hC_8ZIlfpv-43hU-rTQZDWEE2SiLoW_YgzDpcgQqKGAZpCf3Q3a24WVmeld0qUDW6N29g2W3KMBtiYfpCHWpT-lw34F_oKU2nvzaPqEkx2hkU-TApiWJhMlIUsjv-uThatjFYL2qvgMsXnzD5f_vicfEpMFogLICs-TGg8fpzS0xIbtXv9z-VVcn1A)

Create a new Echo Bot project using the following command. We use `echoes` as the name for our bot.

>`$ dotnet new echobot -n echoes`

Once this has completed we can navigate into the new directory that has been created.

>`$ cd ~/mybot/echoes`

Once inside, the project should be ready to run. Test it with:

>`$ sudo dotnet run`

If everything was set up correctly you should see a similar output to the one below:

![|624x357](https://lh5.googleusercontent.com/HIIkWmxltlh8Z8Sdut-i5qndnZ1YJ9tcZnlhOZLlcR6c3Gt7PL4R_ZWQBr3tWnM1tVcO_y7pjtnGVp3QnVvH-pRgp7A3Vn0NrloUwhKe9y5BedXIEPK9b0L7OUCuuFGoDowzowwA9Wuz_6pmz69v3D8KVsW819aMytbhlWqkJK1MhNUuLrtvMwrL4g)

Leave the EchoBot App running in WSL for now. Open a new browser window on your Windows host and navigate to `localhost:3978` where you should see the following window:

![|624x347](https://lh3.googleusercontent.com/wBMwGYN0hAwIocrDcbhQW87tKPKz8bku8Hxb-Xk43o5NBjoYbMRpHDJI4hHfO3uNy15KFFFH-RpiTjytJEvHIbwfxKs-GSBGlCyH92P8m0xVW77AUORLsH4xHdfJhlRpuC6tNqoCVwVBi5HUEQ4sCIiNMegmlXqjoQKxZxEbhz33_DmtcHLwwjzd0g)

Leave everything running as we move to the next step.

## Install the Bot Emulator on Windows and connect to your bot

Download the Bot Emulator from the official [Microsoft GitHub](https://github.com/Microsoft/BotFramework-Emulator/releases/tag/v4.14.1), taking care to select [BotFramework-Emulator-4.14.1-windows-setup.exe](https://github.com/microsoft/BotFramework-Emulator/releases/download/v4.14.1/BotFramework-Emulator-4.14.1-windows-setup.exe) and install.

Running it will present you with the following screen, but before you can connect to your bot you need to change a few settings.

![|624x411](https://lh3.googleusercontent.com/7inX9LYw8kQrGtWZnMpDHFRsONq1s9dDsH2V1qsxK61Pe3L2grkXyzjTa8ZQU4WEJt2JPahl-n1kOBSTlguBL9AhqQkQbXCZhTWbq_qAq6EwgwdsKI3kFKefGnYxmd0fWDnNJnVPwJUS_vAVIwIDH-OBqqVJo7e0HkgC9AeYsLZGOKPh4rlBcmMEnQ)

First, get the IP address of your machine by running `ipconfig` in a PowerShell terminal.

![|624x357](https://lh6.googleusercontent.com/lKKzgkZQJ1ln64R-fzI6PPQ5h6TQEot69Ki7i7C_l3aQJLwY-oRVsm350s96ytriXHKnhaoWZptIKcL_A59BhMIkvKBjSEmyZKioVpWkGmqLYRsDDswq10ds1-xlvkwn8B9o_OGcK0wPogyUWLJfIKEMBIh8X3sbToXhKlXffX7aMzz6ZR-axSqqjA)

Then select the setting icon in the corner of the Bot Framework Emulator and enter your IP under ‘localhost override'.

![|624x411](https://lh6.googleusercontent.com/s6wiapLi-IxcfxX85UgfslQVWW9kJI9V7S-8BGA6QLYKSDUkkZ91trfmNOHrjG402xi2fuY2dXAyhhokWToxwG6gsGwydeg1OC3-f3g6ZyNn6DNNpPk8VbMMt4sEva7mmJO-FH1uyZerX8YrTszE_cR-AWgxQ-C8BGdPUAoWcqTH1dic3C0NRSEeSw)

Click save and navigate back to the Welcome tab.

Click ‘Open Bot’ and under ‘Bot URL’ input:
```
http://localhost:3978/api/messages
```

![|624x411](https://lh3.googleusercontent.com/S_D4Q_HSqo4lXrAYa-SrWXzc9PQKjimydwcplyJLug1jsCjo8eE2Xf1Dgp7lTyWiO8-zFK-aDak490Vi3L8TitJiZaw28vobqklM4m6eHyRjoHYKqKXgCEXTyddFh-4siCBki3nMPNRPCCzm-vZldENifIIFGEqrgDdE0mPagmbR2c16WcNf52Vang)

And click ‘Connect’ to connect to your Echo Bot running in WSL and start chatting!![|624x411](https://lh3.googleusercontent.com/HcTemxFbz7xRaYvlvTZziWb-qQktpRTuZyeTBVEqs0TY2XqVfNkYPJcVLxlbueeVIFk4JKpA52fTiWbVE1l01LpjWHLjsXYCUzOF8zocrlW4B2SWsbmIiLxp00hk4E5bzEVoKMUiNK4gb76ttZ2ZmD1Aj5d2qZ3tkC22W8mxJb4V-yWGG_ySGjlGHg)

Congratulations, your Echo Chat Bot App is running on Ubuntu WSL as an App. Now it is time to make it run as a service.

## Running your Echo Bot as a systemd service

Return to your running WSL distro and end the app with `Ctrl+C`.

Then install the .Net systemd extension with:

>`$ sudo dotnet add package Microsoft.Extensions.Hosting.Systemd`

Now we need to open our ‘echoes’ project in VS Code by running:

>`$ code .`

From the ‘echoes’ directory.

Navigate to ‘Program.cs’ and insert `.UserSystemd()` as a new line in the location shown in the screenshot.

![|624x381](https://lh5.googleusercontent.com/fYkhep5H-fBStxkqUtaXb8jfSSZ3YPta5vTu8eQZgQGeDRU7oFERlnEc1JHlY-UtB4HSBcdBqn8SDsMFVf0bxv6UGCy0CMZ6PBztBGh4DNBx6HIaWt5LJ4yRPU5i8XCSkeMSYaZBWvDh7S3QaZVH6PqyEM1c1EUUu-ugnIPKszWb1pnQgWTko5A4Ug)

Save and close the project in VS code and return to your WSL terminal.

Next we need to create a service file for your bot using your favourite editor, for example.

>`$ sudo nano /etc/systemd/system/echoes.service`

Then paste the below taking care to change the path of `WorkingDirectory` to the location of your project folder.

```
[Unit]
Description=The first ever WSL Ubuntu systemd .NET ChatBot Service

[Service]
WorkingDirectory=/home/local-optimum/demos/mybot/echoes
Environment=DOTNET_CLI_HOME=/temp
ExecStart=dotnet run
SyslogIdentifier=echoes

[Install]
WantedBy=multi-user.target
```

![|624x401](https://lh3.googleusercontent.com/u37gfrOD-U3AbnFBRSgNnqUatrvPIaDssbkia5SHhc0osPOtPXIBC4-fxz7HfGUs4CxdK28LvkGvtKRuOOmTxZeWqgBR7QpSoCkTPj9GA6wzY8m9R574PzgQMlIeO1GZhxPopX2AnYR1QrS5xNYG67xSYQ23ATvloEtCevpO4RkwopSeOntBTKcF1A)

Save your file and reload your services with:

>`$ sudo systemctl daemon-reload`

To reload the services. You can check if your service is ready by running:

>`$ systemctl status echoes.service`

You should get the following output:

![|624x77](https://lh5.googleusercontent.com/2M4cbrth9TmT8im85UbOsYJaXgDTiIvBLUgZLTq53cVkW_IMZ70B7BJ5sqCagQ4vAdbz2tv3xnCWX9aUajFZstYEusWCtdbR0dEG6GlK5rfI2A91WZjj67V1kx6Gwc9o5mUnvaeCxY-4kX0gC0SxoENczCmB_8hKou-XW97OYGYP8tvCPFxi_m_PRw)

Now start your service and then check its status again.

>`$ sudo systemctl start echoes.service`
>`$ sudo systemctl status echoes.service`

If everything has been configured correctly you should get an output similar to the below.

![|624x401](https://lh3.googleusercontent.com/vBX3JPcYXL1-6SBPzeROdYlVZXHF-WHN2CFxMw3BXsD-_cD4wICa_jhLWBCD2xaerIDHg-CtJwH73pkDqbGNdvXNvuFUXmYr9DJ9unT7jt73oqwQvv2VfMV25PkS8i8DCGglixpNUMVa1Jrvc7mZIiT45-u-Q-TyHz6SeLNh1owTn3KzHo_U4dAN8A)

Return to your Windows host and reconnect to your Bot Emulator using the same information as before and confirm that your bot is running, but this time as a systemd service!

![|624x411](https://lh4.googleusercontent.com/4-r5Pk2e178d0R4mzJgtGKOVdjzYdzyGehV3-Ff4_tZXh9n6i0sUlv5pTxdEhhPywJSyYxv3Q9lJi_Q4aElpus8HjH9DkPVyfTx3JQHsCyOySxC6g1NK0bCjLk77trC9OxVawnIPo36Xl8YWPIYYYI1_Eqq_9Q4tMYmA3bqNRzjWYjH1B2UD1kiEzA)

You can stop your bot from running at any time with the command:

>`$ sudo systemctl stop echoes.service`

## Tutorial complete!

You now have a simple Echo Bot running as a systemd service on WSL that you can access from your host Windows machine.

If you would like to expand on this example try reviewing some of the more advanced [Bot Framework samples](https://github.com/Microsoft/BotBuilder-Samples/blob/main/README.md) on the Microsoft GitHub.

To read more about how Ubuntu supports .Net developers, making it easier than ever to build multi-platform services and applications, read our [recent announcement](https://ubuntu.com/blog/install-dotnet-on-ubuntu).

### Further Reading

* [.Net on Ubuntu](https://ubuntu.com/blog/install-dotnet-on-ubuntu)
* [Bot Framework samples](https://github.com/Microsoft/BotBuilder-Samples/blob/main/README.md)
* [Enabling GPU acceleration on Ubuntu on WSL2 with the NVIDIA CUDA Platform](https://ubuntu.com/tutorials/enabling-gpu-acceleration-on-ubuntu-on-wsl2-with-the-nvidia-cuda-platform)
* [Working with Visual Studio Code on Ubuntu on WSL2](https://ubuntu.com/tutorials/working-with-visual-studio-code-on-ubuntu-on-wsl2)
* [Windows and Ubuntu interoperability on WSL2](https://ubuntu.com/tutorials/windows-and-ubuntu-interoperability-on-wsl2#1-overview)
* [Microsoft WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)
* [Ask Ubuntu](https://askubuntu.com/)