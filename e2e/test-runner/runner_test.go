package test_runner

import (
	"bytes"
	"context"
	"os/exec"
	"strings"
	"testing"
	"time"
)

func getDistroStatus(t *Tester) string {
	// wsl -l -v outputs UTF-16 (See https://github.com/microsoft/WSL/issues/4607)
	// We use $env:WSL_UTF8=1 to prevent this (Available from 0.64.0 onwards https://github.com/microsoft/WSL/releases/tag/0.64.0)
	str := t.AssertOsCommand("powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-command",
		"$env:WSL_UTF8=1 ; wsl -l -v")

	for _, line := range strings.Split(str, "\n")[1:] {
		columns := strings.Fields(line)
		// columns = {[*], distroName, status, WSL-version}
		if len(columns) < 3 {
			continue
		}
		idx := 0
		if columns[idx] == "*" {
			idx++
		}
		if columns[idx] == *distroName {
			return columns[idx+1]
		}
	}
	return "DistroNotFound"
}

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
}

// This tests the experience that most users have:
// opening WSL from the store or from the Start menu
func TestDefaultExperience(t *testing.T) {
	tester := WslTester(t)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	shellCommand := []string{"-noninteractive", "-nologo", "-noprofile", "-command",
		*launcherName, "--hide-console"}
	cmd := exec.CommandContext(ctx, "powershell.exe", shellCommand...)

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out

	cmd.Start()

	assertStatusTransition := func(fromStatus string, toStatus string) string {
		status := getDistroStatus(&tester)
		for status == fromStatus {
			time.Sleep(1 * time.Second)
			status = getDistroStatus(&tester)
		}
		if status != toStatus {
			tester.Logf("In transition from '%s' to '%s': Unexpected state '%s'", fromStatus, toStatus, status)
			tester.Logf("Output: %s", out.String())
			tester.Fatal("Unexpected Distro state transition")
		}
		return status
	}

	// Completing installation
	assertStatusTransition("DistroNotFound", "Installing")
	assertStatusTransition("Installing", "Running")
	assertStatusTransition("Running", "Stopped")

	// Ensuring proper installation
	if !strings.Contains(out.String(), "Installation successful!") { // TODO: Change this to parse MOTD
		tester.Logf("Status: %s", getDistroStatus(&tester))
		tester.Logf("Output: %s", out.String())
		tester.Fatal("Distro was shut down without finishing install")
	}
}
