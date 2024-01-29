package launchertester

import (
	"context"
	"errors"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

// TestDefaultExperience tests the experience that most users have:
// opening WSL from the store or from the Start menu.
func TestDefaultExperience(t *testing.T) {
	t.Skip("Skipped: fails in Azure") // TODO: Fix
	wslSetup(t)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// TODO: redirect powershell.exe output to a file and read from it.
	commandText := []string{"-Command", "wt.exe", "--window", "0", "-d", ".",
		"powershell.exe", "-noExit", *launcherName, "--hide-console"}
	cmd := exec.CommandContext(ctx, "powershell", commandText...)

	err := cmd.Start()
	require.NoErrorf(t, err, "Failed to start new WSL command")

	waitStateTransition(t, "DistroNotFound", "Installing")
	waitStateTransition(t, "Installing", "Running")
	waitForInstaller(t)

	state := distroState(t)
	require.Equal(t, "Running", state, "Unexpected state for after installation")

	time.Sleep(15 * time.Second) // If tty did not start, it should stop after 8 seconds
	state = distroState(t)
	require.Equal(t, "Running", state, "Distro should still be running after 15 seconds, because a shell should still be open")

	cancel()
	err = cmd.Wait()
	require.NoError(t, err, "Unexpected error after finishing command")

	testCases := map[string]func(t *testing.T){
		"UserNotRoot":             testUserNotRoot,
		"SystemdEnabled":          testSystemdEnabled,
		"SystemdUnits":            testSystemdUnits,
		"CorrectUpgradePolicy":    testCorrectUpgradePolicy,
		"UpgradePolicyIdempotent": testUpgradePolicyIdempotent,
		"InteropIsEnabled":        testInteropIsEnabled,
	}

	for name, tc := range testCases {
		t.Run(name, tc)
	}
}

// waitStateTransition waits until the current state (fromState) transitions into toState.
// Fails if any other state is reached.
// Fails if the state cannot be parsed.
func waitStateTransition(t *testing.T, fromState string, toState string) {
	t.Helper()

	t.Logf("Awaiting state transition: %s -> %s", fromState, toState)
	state := distroState(t)
	require.Containsf(t, []string{fromState, toState}, state, "In transition from '%s' to '%s': Unexpected state '%s'", fromState, toState, state)

	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	timeout := time.After(60 * time.Second)

	for {
		select {
		case <-timeout:
			t.Fatalf("Didn't reach expected state in time. Last state: %s", state)
		case <-ticker.C:
			state = distroState(t)
			if state == fromState {
				continue
			}
		}
		break
	}

	require.Equalf(t, toState, state, "After transition '%s' -> '%s': Unexpected final state '%s'", fromState, toState, state)
}

// waitForInstaller waits until the subiquity server log indicates that the installation
// has finsihed.
// Considerations:
//   - The server may still run for a small amount of time after the log
//     says it has finished. Experimentally, it seems to be less than a second.
//   - The State of the distro must be either Running or Stopped when called (i.e. installation must have been started).
func waitForInstaller(t *testing.T) {
	t.Helper()

	require.Contains(t, []string{"Running", "Stopped"}, distroState(t))
	t.Logf("Waiting for installer to finish")
	defer t.Logf("Installation finished")

	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()

	timeout := time.After(2 * time.Minute)

	for {
		select {
		case <-timeout:
			t.Fatal("Timed out waiting for installer to finish")
		case <-ticker.C:
			out, err := wslCommand(context.Background(), "cat", serverLogPath).CombinedOutput()

			var target *exec.ExitError
			if errors.As(err, &target) {
				require.Equal(t, target.ProcessState.ExitCode(), 1, "Unexpected error reading subiquity server log: %s", out)
				continue // Log not yet available
			}
			require.NoErrorf(t, err, "Unexpected error reading subiquity server log: %s", out)

			if strings.Contains(string(out), "finish: subiquity/SetupShutdown/shutdown: SUCCESS") {
				time.Sleep(time.Second) // Offering the server some time to finish
				return
			}
		}
	}
}
