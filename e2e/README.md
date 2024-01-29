# End-to-End testing for Ubuntu WSL application packages

This subdirectory contains the infrastructure to allow a CI workflow (or developers) to run a full end-to-end test against an Ubuntu WSL appx.

The key component to allow that happening is materialized in the form of a Go package:

- `launchertester` is the high level testing code, where we invoke the distro launcher with certain command line parameters and asserts that the registered instance fulfills our expectations.

A sideload version of the appx to be tested must be built and installed as it would normally be done locally. Assuming the distro application under test is `Ubuntu-Preview`, then one can:

```powershell
cd .\e2e\
go test .\launchertester --distro-name Ubuntu-Preview --launcher-name ubuntupreview.exe
```

The test cases will drive the distro launcher, register the distro, perform the proper setup, restart the distro and perform relevant assertions according to the prescriptions of the test case. In the end, successfully or not, the instance is unregistered, so we can avoid dependencies between different test cases.

Since those tests registers and unregisters WSL instances with the same name, this is impossible to parallelize on the same machine.

Note that WSL itself is shutdown during tests, so it's advisable to stop working on any WSL instance during the time the end to end tests are running.
