# End-to-End testing for Ubuntu WSL application packages

This subdirectory contains the infrastructure to allow a CI workflow (or developers) to run a full end-to-end test against an Ubuntu WSL appx.

Two key components to allow that happening are materialized in the form of Go packages:

- `launchertester` is the high level testing code, where we invoke the distro launcher with certain command line parameters and asserts that the registered instance fulfills our expectations.
- `ui-driver` is a replacement for the `ubuntu_wsl_setup` Flutter binary, invoked by the distro launcher as if it was the OOBE GUI itself and responsible for translating the distro launcher's intention into a suitable `flutter test` command able to run the GUI in an automated way.

This testing infrastructure requires the OOBE to be a single Windows application. Currently only Ubuntu-Preview supports it, but soon we expect to port that feature to 22.04 as well.

To test this locally, one needs to be able to build the Ubuntu-Preview appx with the `ui-driver.exe` binary bundled. That is achived by copying the `ui-driver/ui-driver.props` file into the build tree `DistroLauncher-Appx/` subdirectory. Doing so allows MSBuild to compile the Go application and import it as a deployment content. Building the application package follows the existing practice. See the [build-wsl](../.github/workflows/build-wsl.yaml) workflow for further details on the meta build system.

The resulting appx must be installed as the normal package would do.

Then one can:

```powershell
$env:LAUNCHER_REPO_ROOT=<path_where_you_checked_out_ubuntu_wsl>
cd .\e2e\
go test .\launchertester --distro-name Ubuntu-Preview --launcher-name ubuntupreview.exe
```

The test cases will drive the distro launcher, run the OOBE in integration testing mode, register the distro, perform the proper setup, restart the distro and perform relevant assertions according to the prescriptions of the test case. In the end, successfully or not, the instance is unregistered, so we can avoid dependencies between different test cases.

Since those tests registers and unregisters WSL instances with the same name, this is impossible to parallelize on the same machine.
