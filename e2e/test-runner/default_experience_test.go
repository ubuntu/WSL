package test_runner

import (
	"bytes"
	"context"
	"os/exec"
	"strings"
	"testing"
	"time"
)

// Parses the output of wsl -l -v to find the STATE of the current distro
func assertGetDistroState(t *Tester, distro string) string {
	// wsl -l -v outputs UTF-16 (See https://github.com/microsoft/WSL/issues/4607)
	// We use $env:WSL_UTF8=1 to prevent this (Available from 0.64.0 onwards https://github.com/microsoft/WSL/releases/tag/0.64.0)
	str := t.AssertOsCommand("powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-command",
		"$env:WSL_UTF8=1 ; wsl -l -v")

	for _, line := range strings.Split(str, "\n")[1:] {
		columns := strings.Fields(line)
		// columns = {[*], distroName, state, WSL-version}
		if len(columns) < 3 {
			continue
		}
		idx := 0
		if columns[idx] == "*" {
			idx++
		}
		if columns[idx] == distro {
			return columns[idx+1]
		}
	}
	return "DistroNotFound"
}

// Waits until the current state (fromState) transitions into toState.
// Fails if any other state is reached
func assertWaitStateTransition(tester *Tester, outbuff *bytes.Buffer, distro string, fromState string, toState string) string {
	state := assertGetDistroState(tester, distro)
	for state == fromState {
		time.Sleep(1 * time.Second)
		state = assertGetDistroState(tester, distro)
	}
	if state != toState {
		tester.Logf("In transition from '%s' to '%s': Unexpected state '%s'", fromState, toState, state)
		tester.Logf("Output: %s", outbuff.String())
		tester.Fatal("Unexpected Distro state transition")
	}
	return state
}

// This tests the experience that most users have:
// opening WSL from the store or from the Start menu
func TestDefaultExperience(t *testing.T) {
	tester := WslTester(t)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	shellCommand := []string{"-noninteractive", "-nologo", "-noprofile", "-command",
		*launcherName, "install --root --ui=none"} // TODO: Change to ...*launcherName, "--hide-console")
	cmd := exec.CommandContext(ctx, "powershell.exe", shellCommand...)

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out

	cmd.Start()

	// Completing installation
	assertWaitStateTransition(&tester, &out, *distroName, "DistroNotFound", "Installing")
	assertWaitStateTransition(&tester, &out, *distroName, "Installing", "Running")
	assertWaitStateTransition(&tester, &out, *distroName, "Running", "Stopped")

	// Ensuring proper installation
	if !strings.Contains(out.String(), "Installation successful!") { // TODO: Change this to parse MOTD
		tester.Logf("State: %s", assertGetDistroState(&tester, *distroName))
		tester.Logf("Output: %s", out.String())
		tester.Fatal("Distro was shut down without finishing install")
	}
}
