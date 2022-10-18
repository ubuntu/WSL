package test_runner

import (
	"fmt"
	"strings"
	"testing"
	"time"
)

// Parses the output of wsl -l -v to find the STATE of the current distro
// Fails if the state cannot be parsed
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
// Fails if the state cannot be parsed
func assertWaitStateTransition(tester *Tester, distro string, fromState string, toState string) string {
	tester.Logf("Awaiting state transition: %s -> %s", fromState, toState)
	state := assertGetDistroState(tester, distro)
	for state == fromState {
		time.Sleep(1 * time.Second)
		state = assertGetDistroState(tester, distro)
	}
	if state != toState {
		tester.Logf("In transition from '%s' to '%s': Unexpected state '%s'", fromState, toState, state)
		tester.Fatal("Unexpected Distro state transition")
	}
	return state
}

// Waits until the subiquity server log indicates that the installation
// has finsihed.
// Considerations:
// - It fails if the log cannot be accessed.
// - The server may still run for a small amount of time after the log
//   says it has finished. Exeperimentally, it seems to be less than a second.
// - The State of the ditro is assumed to be either Running or Stopped when called
func assertWaitForInstaller(tester *Tester) {
	tester.Logf("Waiting for installer to finish")

	for {
		linuxCmd := fmt.Sprintf("cat %s | tail -1", ServerLogPath)
		output := tester.AssertWslCommand("-u", "root", "bash", "-ec", linuxCmd)

		if strings.Contains(string(output), "finish: subiquity/SetupShutdown/shutdown: SUCCESS") {
			return
		}

		time.Sleep(1 * time.Second)
	}
}

// This tests the experience that most users have:
// opening WSL from the store or from the Start menu
func TestDefaultExperience(t *testing.T) {
	tester := WslTester(t)

	commandText := []string{"wt.exe", "--window", "0", "-d", ".",
		"powershell.exe", "-noExit", *launcherName, "--hide-console"}
	tester.AssertOsCommand("powershell.exe", commandText...)

	// Completing installation
	assertWaitStateTransition(&tester, *distroName, "DistroNotFound", "Installing")
	assertWaitStateTransition(&tester, *distroName, "Installing", "Running")
	assertWaitForInstaller(&tester)

	tester.Logf("Waiting for interactive session to start")
	time.Sleep(30 * time.Second)

	state := assertGetDistroState(&tester, *distroName)
	if state != "Running" {
		tester.Logf("Unexpected state '%s' after installing", state)
		tester.Fatal("Distro state wasn't kept 'Running' after install. Did the interactive session start?")
	}

	tester.Logf("Terminating interactive session")
	tester.AssertOsCommand("wsl.exe", "-t", *distroName)

	assertWaitStateTransition(&tester, *distroName, "Running", "Stopped")
}
