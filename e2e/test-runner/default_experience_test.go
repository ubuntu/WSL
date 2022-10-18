package test_runner

import (
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

	tester.Logf("Waiting for installer to finish")
	time.Sleep(60 * time.Second)

	state := assertGetDistroState(&tester, *distroName)
	if state != "Running" {
		tester.Logf("Unexpected state '%s' after installing", state)
		tester.Fatal("Distro wasn't kept Running after install")
	}

	tester.Logf("Shutting down WSL")
	tester.AssertOsCommand("wsl.exe", "--shutdown")
	assertWaitStateTransition(&tester, *distroName, "Running", "Stopped")
}
