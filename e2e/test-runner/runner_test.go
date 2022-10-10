package test_runner

import (
	"strings"
	"testing"
)

func TestBasicSetup(t *testing.T) {
	// Wrapping testing.T to get the friendly additions to its API as declared in `wsl_tester.go`.
	tester := WslTester(t)

	// Invoke the launcher as if the user did: `<launcher>.exe install --ui=gui`
	outStr := tester.AssertLauncherCommand("install --ui=gui") // without install the launcher will run bash.
	if len(outStr) == 0 {
		tester.Fatal("Failed to install the distro: No output produced.")
	}

	// After completing the setup, the OOBE must have created a new user and the distro launcher must have set it as the default.
	outputStr := tester.AssertWslCommand("whoami")
	notRoot := strings.Fields(outputStr)[0]
	if notRoot == "root" {
		tester.Logf("%s", outputStr)
		tester.Fatal("Default user should not be root.")
	}

	// Subiquity either installs or marks for installation the relevant language packs for the chosen language.
	outputStr = tester.AssertWslCommand("apt-mark", "showinstall", "language-pack\\*")
	packs := strings.Fields(outputStr)
	tester.Logf("%s", outputStr) // I'd like to see what packs were installed in the end, just in case.
	if len(packs) == 0 {
		tester.Fatal("At least one language pack should have been installed or marked for installation, but apt-mark command output is empty.")
	}

	// TODO: Assert more. We have other expectations about the most basic setup.
	// Systemd enabled. AssertWslCommand failure is allowed because degraded status exits with code 3
	outputStr = tester.AssertWslCommand("bash", "-ec", "systemctl is-system-running || exit 0")
	lines := strings.Fields(outputStr)
	if len(lines) == 0 {
		tester.Fatal("systemctl is-system-running printed nothing")
	}
	if lines[0] != "degraded" && lines[0] != "running" {
		tester.Logf("%s", lines[0])
		tester.Fatal("Systemd should have been enabled")
	}

	// Sysusers service fix
	tester.AssertWslCommand("systemctl", "status", "systemd-sysusers.service")
}
