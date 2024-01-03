# Auto Installation Support

The [Ubuntu Preview app](https://apps.microsoft.com/detail/9P7BDVKVNXZ6?hl=en-us&gl=US) exposes a rather incomplete and highly experimental subset of the automatic installation feature offered by Subiquity. In the following sections, we’ll explore what can be done with that feature and how to do it.

From now on we assume the reader had just installed the Ubuntu Preview app from Microsoft Store and didn’t register an instance of Ubuntu Preview just yet.

## Divergences from the existing autoinstall documentation

Cloud-init plays a big role in Subiquity’s autoinstall feature, starting from the fact that the autoinstall configuration is provided via cloud-init configuration by design on servers. At the time of this writing, though, cloud-init is not supported in WSL, which causes a major divergence between the existing autoinstallation documentation and the feature exposed for WSL. Thus, the autoinstall config must be passed directly to Subiquity, instead of under a cloud-config key in the YAML file, as shown in the example below. Failing to comply with that causes the setup process to block forever waiting on Cloud-init.

<table border=1>
<tr>
<th> Server autoinstall config file </th>
<th> WSL autoinstall config file    </th>
</tr>
<tr>
<td>

```yaml
#cloud-config:
autoinstall:
  version: 1
  identity:
    hostname: UbuntuPC
    username: ubuntu
    password: "$6$Y7Cajk..."
...
```
</td>
<td>

```yaml
version: 1
identity:
  username: ubuntu
  password: "$6$Y7Cajk..."
...
```

</td>
</tr>
<tr>
<td>

Note the indentation: the data is nested under the `autoinstall` YAML key.
</td>
<td>

Note the indentation: the autoinstall config data is not nested under any YAML key.
Also, there is no `#cloud-config` heading comment.
</td>
</table>

The second divergent aspect is perhaps the most obvious one: the capabilities of the WSL setup cannot be compared to the server installer, thus the config schema must be different.

The last divergence to call out is how to feed the config data since we cannot use cloud-init. That’s done via the distro launcher executable command line:

```powershell
ubuntupreview.exe install --autoinstall <WINDOWS PATH TO THE YAML FILE>
```

It’s important to note that the config file exists on the Windows filesystem since at this point we didn’t set up the distro instance just yet. Also, the distro launcher is a Windows executable. Thus the path provided to the command line is the Windows path.

## Running the auto installation

The following config creates the “ubuntu” user (automatically set as the default) and password “ubuntu”.
```yaml
version: 1
identity:
  realname: Ubuntu
  username: ubuntu
  # ubuntu
  password: "$6$wdAcoXrU039hKYPd$508Qvbe7ObUnxoj15DRCkzC3qO7edjH0VV7BPNRDYK4QR8ofJaEEF2heacn0QgD.f8pO8SNp83XNdWG6tocBM1"
```

Let’s save that in the local directory (D:\ in the screenshots) named `conf.yaml` and pass it to the Ubuntu Preview distro launcher via the command line.

```powershell
ubuntupreview.exe install --autoinstall .\conf.yaml
```

After some seconds we should have an instance of Mantic Minotaur ready. Notice that the autoinstallation won’t execute a shell after completion, it will just quit the launcher. That behavior fits into unattended setup scenarios.

Running a command with sudo demonstrates that the password was set to “ubuntu” as we would expect.

![image](assets/autoinstall-screenshot-1.png)

Experiment playing with that config by changing the username and password. Don’t forget that this is a setup process, so you’ll need to unregister the existing Ubuntu Preview instance before running the auto-install command again. To generate the encrypted version of the desired password you can run on another instance of Ubuntu the mkpasswd command (part of the whois package) or openssl passwd:

```bash
mkpasswd -m sha512crypt
```

```bash
openssl passwd -6 -stdin
```

A more complete configuration can be found below. It adds support for the pt_BR locale, runs commands early (creating a file) and late in the setup process (installing the x11-apps package via apt), and writes non-default values to the `/etc/wsl.conf` file using the installer lingo. Except for the early and late commands, everything else would be fed to the setup GUI if it was running interactively. Check the WSL setup auto-install reference for more information on the specifics of each field.

```yaml
version: 1

early-commands:
    - touch /etc/i-am-early
locale: pt_BR
late-commands:
    - ["/usr/bin/apt", "install", "-y", "x11-apps"]
identity:
  realname: Ubuntu
  username: ubuntu
  # ubuntu
  password: '$6$wdAcoXrU039hKYPd$508Qvbe7ObUnxoj15DRCkzC3qO7edjH0VV7BPNRDYK4QR8ofJaEEF2heacn0QgD.f8pO8SNp83XNdWG6tocBM1'
wslconfbase:
  automount_root: '/custom_mnt_path'
  automount_options: 'metadata'
  network_generatehosts: false
  network_generateresolvconf: false
wslconfadvanced:
  interop_enabled: false
  interop_appendwindowspath: false
  automount_enabled: false
  automount_mountfstab: false
```

The result should look something like:

![image](assets/autoinstall-screenshot-2.png)

That’s it. Although rather limited, the Ubuntu WSL auto-install feature has the potential to unlock many interesting use cases, such as allowing standardized deployments of WSL instances or installing packages via early and late commands.

## Ubuntu WSL autoinstall reference

### Overall format

The autoinstall file is YAML. At the top level, it must be a mapping containing the keys described in this document. Unrecognized keys are ignored.

### Command lists
Several config keys are lists of commands to be executed. Each command can be a string (in which case it is executed via “sh -c”) or a list, in which case it is executed directly. Any command exiting with a non-zero return code is considered an error and aborts the install (except for `error-commands`, where it is ignored).

### Top-level keys

#### version
- type: integer
- default: no default

A future-proofing config file version field. Currently, this must be “1”.

#### early-commands
- type: command list
- default: no commands

A list of shell commands to invoke as soon as the installer starts. The autoinstall config is available at `/autoinstall.yaml` (irrespective of how it was provided) and the file will be re-read after the `early-commands` have run to allow them to alter the config if necessary. The commands run in the rootfs environment, there is no chroot or equivalent mechanism for the WSL setup.

#### locale
- type: string
- default: en_US.UTF-8

The locale to configure for the installed system.

#### identity
- type: mapping, see below
- default: no default

Configure the initial user for the system. This is mandatory, otherwise the installer assumes it to be interactive and will wait for input from a UI client. Notice that the hostname is not available.

A mapping that can contain keys, all of which take string values:

#### realname
The real name of the user. This field is optional.

#### username
The user name to create.

#### password
The encrypted password for the new user. This is required for use with sudo, even if SSH access is configured.

The encrypted password string must conform to what passwd expects. Depending on the special characters in the password hash, quoting may be required, so it’s safest to just always include the quotes around the hash.

Several tools can generate the encrypted password, such as `mkpasswd` from the `whois` package, or `openssl passwd`.

Example:

```yaml
identity:
  realname: 'Ubuntu User'
  username: ubuntu
  password: '$6$wdAcoXrU039hKYPd$508Qvbe7ObUnxoj15DRCkzC3qO7edjH0VV7BPNRDYK4QR8ofJaEEF2heacn0QgD.f8pO8SNp83XNdWG6tocBM1'
```

#### late-commands
- type: command list
- default: no commands

Shell commands to run after the installation has been completed successfully and any updates and packages installed, just before the system reboots. As with the early commands, they run in the rootfs environment; there is no chroot or equivalent mechanism for the WSL setup.

### WSL specifics
The keys `wslconfbase` and `wslconfspecific` hold configuration data that will be written to the `/etc/wsl.conf` file, overriding the WSL default configuration for the instance being set up. Refer to Microsoft documentation for WSL advanced configuration to find the details of the subkeys. To properly map the configuration keys in the autoinstall YAML and the wsl.conf INI format, consider that `wslconfbase.automount_root` in YAML maps to INI `automount.root`, that is, the underscore in the subkey name delimits the section in the INI. See the example below for further clarification.


<table border=1>
<tr>
<th> Autoinstall YAML syntax    </th>
<th> wsl.conf INI syntax        </th>
</tr>
<tr>
<td>

```yaml
wslconfbase:
  automount_root: ‘/media’
```
</td>
<td>

```yaml
[automount]
root = ‘/media’
```
</td>
</tr>
</table>

#### wslconfbase
- type: mapping, see below
- default: no default

Configuration data that will be written to `/etc/wsl.conf` as part of the setup process.
- automount_root: string
- automount_options: string
- network_generatehosts: boolean
- network_generateresolvconf: boolean

#### wslconfadvanced
- type: mapping, see below
- default: no default
Configuration that will be written to `/etc/wsl.conf` as part of the setup process.
- interop_enabled: boolean
- interop_appendwindowspath: boolean
- automount_enabled: boolean
- automount_mountfstab: boolean


