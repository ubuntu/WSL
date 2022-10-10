package test_runner

import (
	"bytes"
	"context"
	"fmt"
	"os/exec"
	"strings"
	"testing"
	"time"
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
}

// This tests the experience that most users have:
// opening WSL from the store or from the Start menu
func TestDefaultExperience(t *testing.T) {
	tester := WslTester(t)
	timeOut := 60 * time.Second

	ctx, cancel := context.WithTimeout(context.Background(), timeOut)
	defer cancel()

	shellCommand := []string{"-noninteractive", "-nologo", "-noprofile", "-command", *launcherName, "install --root --ui=none"}
	cmd := exec.CommandContext(ctx, "powershell.exe", shellCommand...)
	// cmd := exec.CommandContext(ctx, "echo", "Hello, world")
	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out
	cmd.Start()

	start := time.Now()
	for time.Since(start) < timeOut {
		time.Sleep(2 * time.Second)
		findDistroStatus := fmt.Sprintf("(wsl -l -v) -replace \"`0\" | Out-String -Stream | Select-String -Pattern '%s'", *distroName)
		str := tester.AssertOsCommand("powershell.exe ", "-nologo", "-noprofile", "-command", findDistroStatus)
		println(str)
		if strings.Contains(str, "Running") {
			break
		}
		if !strings.Contains(str, "Installing") {
			tester.Logf("%s", out.String())
			tester.Fatal("Failed to install")
		}

	}
	if time.Since(start) >= timeOut {
		tester.Logf("%s", out.String())
		tester.Fatal("Failed to install: timeout")
	}

	time.Sleep(5 * time.Second) // TODO: Do something smarter

	if !strings.Contains(out.String(), "Installation successful!") {
		tester.Logf("%s", out.String())
		tester.Fatal("Failed to finish install")
	}
}
