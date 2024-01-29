package launchertester

import (
	"bufio"
	"bytes"
	"context"
	"flag"
	"os/exec"
	"regexp"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

const (
	serverLogPath       = "/var/log/installer/systemsetup-server-debug.log"
	clientLogPath       = "ubuntu-desktop-installer/packages/ubuntu_wsl_setup/build/windows/runner/Debug/.ubuntu_wsl_setup.exe/ubuntu_wsl_setup.exe.log"
	subiquityAnswerFile = "/var/log/prefill-system-setup.yaml"
)

// These timeouts have been decided on experimentally, according to the time each action
// could reasonably need on an Azure runner.
const (
	installTimeout     = 10 * time.Minute // Timeout to use for registering a new distro
	systemdBootTimeout = 2 * time.Minute  // Timeout to use for booting up a distro using systemd
	commandTimeout     = 10 * time.Second // Timeout to use for "instantaneous" commands such as "echo" or "exit"
)

var launcherName = flag.String("launcher-name", DefaultLauncherName, "WSL distro launcher under test.")
var distroName = flag.String("distro-name", DefaultDistroName, "WSL distro instance registered for testing.")

// wslSetup validates the test environment and ensures the distro is unregistered at the end.
func wslSetup(t *testing.T) {
	t.Helper()

	checkValidTestbed(t)

	t.Cleanup(func() {
		if err := exec.Command("wsl.exe", "--shutdown").Run(); err != nil {
			t.Logf("Failed to shut distro down after test: %v", err)
		}

		if err := exec.Command("wsl.exe", "--unregister", *distroName).Run(); err != nil {
			t.Logf("Failed to unregister distro after test: %v", err)
		}
	})
}

// wslCommand mocks exec.CommandContext with WSL commands.
func wslCommand(ctx context.Context, linuxCmd ...string) *exec.Cmd {
	args := append([]string{"-d", *distroName, "--"}, linuxCmd...)
	return exec.CommandContext(ctx, "wsl.exe", args...)
}

// wslCommandAsUser mocks exec.CommandContext with WSL commands, executed as the specified user.
func wslCommandAsUser(ctx context.Context, user string, linuxCmd ...string) *exec.Cmd {
	args := append([]string{"-d", *distroName, "-u", user, "--"}, linuxCmd...)
	return exec.CommandContext(ctx, "wsl.exe", args...)
}

// launcherCommand mocks exec.CommandContext with Launcher commands.
func launcherCommand(ctx context.Context, verb string, args ...string) *exec.Cmd {
	args = append([]string{*launcherName, verb}, args...)
	return exec.CommandContext(ctx, "powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-command", strings.Join(args, " "))
}

// checkValidTestbed checks that the test environment is valid.
func checkValidTestbed(t *testing.T) {
	t.Helper()

	status := distroState(t)
	require.Equal(t, "DistroNotFound", status, "Setup: the tested distro is registered. Make a backup and unregister it before running the tests.")
}

func terminateDistro(t *testing.T) {
	t.Helper()
	out, err := exec.Command("wsl.exe", "--terminate", *distroName).CombinedOutput()
	require.NoError(t, err, "Failed to shut down WLS: %s", out)
}

// distroState parses the output of "wsl -l -v" to find the state of the current distro.
// Fails if the state cannot be parsed.
func distroState(t *testing.T) string {
	t.Helper()

	const distroNotFoundMsg = "DistroNotFound"

	// wsl -l -v outputs UTF-16 (See https://github.com/microsoft/WSL/issues/4607)
	// We use $env:WSL_UTF8=1 to prevent this (Available from 0.64.0 onwards https://github.com/microsoft/WSL/releases/tag/0.64.0)
	out, err := exec.Command("powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-command", "$env:WSL_UTF8=1 ; wsl -l -v").CombinedOutput()
	if err != nil {
		// This error shows up when there is no distro installed
		require.Containsf(t, string(out), "WSL_E_DEFAULT_DISTRO_NOT_FOUND", "Unexpected error calling 'wsl -l -v'. Error: %v\nOutput: %s", err, out)
		return distroNotFoundMsg
	}

	// Example line:
	// * Ubuntu-22.04                      Stopped         2
	// ↑ ↑~~~~~~~~~~~                      ↑~~~~~~
	// | 2: Distro name                    3: Status
	// 1: Default[*| ] (ignored)
	pattern := regexp.MustCompile(`^(\*| ) ([a-zA-Z-_0-9.]+)\s+([a-zA-Z]+)\s+[0-9]$`)
	const DistroNameIdx = 2
	const DistroStateIdx = 3

	scanner := bufio.NewScanner(bytes.NewReader(out))
	for scanner.Scan() {
		line := scanner.Text()
		m := pattern.FindStringSubmatch(line)
		if len(m) != 4 {
			continue
		}
		if m[DistroNameIdx] != *distroName {
			continue
		}
		return m[DistroStateIdx]
	}

	require.NoErrorf(t, scanner.Err(), "Unexpected error in scanner: %v", err)

	return distroNotFoundMsg
}
