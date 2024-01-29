# Automatic setup with cloud-init
*Authored by Carlos Nihelton ([carlos.santanadeoliveira@canonical.com](mailto:carlos.santanadeoliveira@canonical.com))*

Cloud-init is an industry-standard multi-distribution method for cross-platform cloud instance initialisation.
Ubuntu WSL users can now leverage it to perform an automatic setup to get a working instance with minimal touch.
That feature is currently experimental and, thus only available on the UbuntuPreview app. Soon that will be available for
the latest LTS application as well.

## What you will learn:

- How to write cloud-config user data to a specific WSL instance.
- How to automatically set up a WSL instance with cloud-init.
- How to verify that cloud-init succeeded with the configuration supplied.

## What you will need:

- Windows 11 with WSL 2 already enabled
- The latest UbuntuPreview application from Microsoft Store.

## Write the cloud-config file

Locate your Windows user home directory. It typically is `C:\Users\<YOUR_USER_NAME>`.

> You can be sure about that path by running `echo $env:USERPROFILE` in PowerShell.

Inside your Windows user home directory, create a new folder named `.cloud-init` (notice the `.` à la Linux
configuration directories), and inside the new directory, create an empty file named `Ubuntu-Preview.user-data`. That file name must
match the name of the distro instance that will be created in the next step.

Open that file with your text editor of choice (`notepad.exe` is just fine) and paste in the following contents:

```yaml
#cloud-config
locale: pt_BR
users:
- name: jdoe
  gecos: John Doe
  groups: [adm,dialout,cdrom,floppy,sudo,audio,dip,video,plugdev,netdev]
  sudo: ALL=(ALL) NOPASSWD:ALL
  shell: /bin/bash

write_files:
- path: /etc/wsl.conf
  append: true
  content: |
    [user]
    default=jdoe

packages: [ginac-tools, octave]

runcmd:
   - git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
   - /opt/vcpkg/bootstrap-vcpkg.sh
```

Save it and close it.

> That example will create a user named `u` and set it as default via `/etc/wsl.conf`, install the packages
> `ginac-tools` and `octave` and install `vcpkg` from the git repository, since there is no deb or snap of that
> application (hence the reason for being included in this tutorial - it requires an unusual setup).

## Register a new Ubuntu-Preview instance

In PowerShell, run:

```powershell
ubuntupreview.exe install --root
```

We skip the user creation since we expect cloud-init to do it.

> If you want to be sure that there is now an Ubuntu-Preview instance, run `wsl -l -v`.
> Notice that the application is named `UbuntuPreview` but the WSL instance created is named `Ubuntu-Preview`.
> See more about that naming convention in [our reference documentation](../reference/distributions.md#naming).

## Check that cloud-init is running

In PowerShell again run:


```powershell
ubuntupreview run cloud-init status --wait
```

That will wait until cloud-init completes configuring the new instance we just created. When done, you should see an
output similar to the following:

```powershell
..............................................................................
..............................................................................
..............................................................................
...................
status: done
```


## Verify that it worked

Restart the distro just to make sure the changes in `/etc/wsl.conf` made by cloud-init will take effect, as listed
below:

```powershell
> wsl -t Ubuntu-Preview
The operation completed successfully.
> ubuntupreview
To run a command as administrator (user "root"), use "sudo <command>".
See "man sudo_root" for details.

Welcome to Ubuntu Noble Numbat (development branch) (GNU/Linux 5.15.137.3-microsoft-standard-WSL2 x86_64)

 * Documentation:  https://help.ubuntu.com
 * Management:     https://landscape.canonical.com
 * Support:        https://ubuntu.com/pro


This message is shown once a day. To disable it please create the
/home/cn/.hushlogin file.
u@mib:~$
```

Once logged in the new distro instance's shell, verify that:

1. The default user matches what was configured in the user data file (in our case `u`).

```sh
u@mib:~$ whoami
u
```

2. The supplied cloud-config user data was approved by cloud-init validation.

```sh
u@mib:~$ sudo cloud-init schema --system
Valid schema user-data
```

3. The locale is set

```sh
u@mib:~$ locale
LANG=pt_BR
LANGUAGE=
LC_CTYPE="pt_BR"
LC_NUMERIC="pt_BR"
LC_TIME="pt_BR"
LC_COLLATE="pt_BR"
LC_MONETARY="pt_BR"
LC_MESSAGES="pt_BR"
LC_PAPER="pt_BR"
LC_NAME="pt_BR"
LC_ADDRESS="pt_BR"
LC_TELEPHONE="pt_BR"
LC_MEASUREMENT="pt_BR"
LC_IDENTIFICATION="pt_BR"
LC_ALL=

```

4. The packages were installed and the commands they provide are available

```sh
u@mib:~$ apt list --installed | egrep 'ginac|octave'

WARNING: apt does not have a stable CLI interface. Use with caution in scripts.

ginac-tools/noble,now 1.8.7-1 amd64 [installed]
libginac11/noble,now 1.8.7-1 amd64 [installed,automatic]
octave-common/noble,now 8.4.0-1 all [installed,automatic]
octave-doc/noble,now 8.4.0-1 all [installed,automatic]
octave/noble,now 8.4.0-1 amd64 [installed]
```

5. Verify that the commands requested were also run. In this case we set up `vcpkg` from git, as recommended by its
   documentation (there is no deb or snap available for that program).

```sh
u@mib:~$ /opt/vcpkg/vcpkg version
vcpkg package management program version 2024-01-11-710a3116bbd615864eef5f9010af178034cb9b44

See LICENSE.txt for license information.
```

## Enjoy!

That’s all folks! In this tutorial, we’ve shown you how to use cloud-init to automatically set up Ubuntu on WSL 2 with minimal touch.

This workflow will guarantee a solid foundation for your next Ubuntu WSL project.

We hope you enjoy using Ubuntu inside WSL!

### Further Reading

To know more about cloud-init support for WSL check out the [WSL data source reference in cloud-init's docs](https://cloudinit.readthedocs.io/en/latest/reference/datasources/wsl.html).
To learn more about cloud-init in general visit [cloud-init's official documentation](https://cloudinit.readthedocs.io/en/latest/index.html).

