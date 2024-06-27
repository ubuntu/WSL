package launchertester

import (
	"bufio"
	"bytes"
	"context"
	"os/exec"
	"regexp"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"gopkg.in/ini.v1"
)

// testDefaultUser ensures the default user matches the expected.
func testDefaultUser(t *testing.T, expected string) { //nolint: thelper, this is a test
	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	out, err := wslCommand(ctx, "whoami").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing whoami: %s", out)
	got := strings.TrimSpace(string(out))
	require.Equal(t, expected, got, "Default user should be %s, got %s", expected, got)
}

// testSystemdEnabled ensures systemd was enabled.
func testSystemdEnabled(t *testing.T) { //nolint: thelper, this is a test
	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	out, err := wslCommand(ctx, "systemctl", "is-system-running", "--wait").CombinedOutput()
	if err == nil {
		return // Success: Non-deterministic
	}

	// Only acceptable alternative is "degraded"
	var target *exec.ExitError
	require.ErrorAs(t, err, &target, "systemctl is-system-running: Only acceptable error is ExitError (caused by systemd being degraded)")
	require.Equal(t, target.ExitCode(), 1, "systemctl is-system-running: Only acceptable non-zero exit code is 1 (degraded)")
	require.Contains(t, string(out), "degraded", "systemd output should be degraded")
}

// testSystemdUnits ensures the list of failed units does not regress.
func testSystemdUnits(t *testing.T) { //nolint: thelper, this is a test
	// Terminating to start systemd, and to ensure no services from previous tests are running
	terminateDistro(t)

	distroNameToFailedUnits := map[string][]string{
		"Ubuntu-18.04":   {"user@0.service", "atd.service"},
		"Ubuntu-20.04":   {"user@0.service", "atd.service"},
		"Ubuntu-22.04":   {"user@0.service"},
		"Ubuntu":         {"user@0.service"},
		"Ubuntu-Preview": {"user@0.service"},
	}

	expectedFailure, ok := distroNameToFailedUnits[*distroName]
	if !ok { // Development version
		expectedFailure = distroNameToFailedUnits["Ubuntu-Preview"]
	}

	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	// Reading failed units
	out, err := wslCommand(ctx, "systemctl", "list-units", "--state=failed", "--plain", "--no-legend", "--no-pager").CombinedOutput()
	require.NoError(t, err)

	s := bufio.NewScanner(bytes.NewReader(out))
	var failedUnits []string
	for s.Scan() {
		data := strings.Fields(s.Text())
		if len(data) == 0 {
			continue
		}
		unit := strings.TrimSpace(data[0])
		failedUnits = append(failedUnits, unit)
	}
	require.NoError(t, s.Err(), "Error scanning output of systemctl")

	for _, u := range failedUnits {
		assert.Contains(t, expectedFailure, u, "Unexpected failing unit")
	}
}

// testCorrectUpgradePolicy ensures upgrade policy matches the one expected for the app.
func testCorrectUpgradePolicy(t *testing.T) { //nolint: thelper, this is a test
	/* Ubuntu always upgrade to next lts */
	wantPolicy := "lts"
	ltsRegex := regexp.MustCompile(`^Ubuntu-\d{2}\.\d{2}$`) // Ubuntu-WX.YZ
	if ltsRegex.MatchString(*distroName) {
		wantPolicy = "never"
	} else if *distroName != "Ubuntu" {
		// Preview and Dev
		wantPolicy = "normal"
	}

	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	out, err := launcherCommand(ctx, "run", "cat", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Unexpected failure executing cat: %s", out)
	require.NotEmpty(t, out, "Release upgrades file is empty")

	cfg, err := ini.Load(out)
	require.NoError(t, err, "Failed to parse ini file: %s", out)

	section, err := cfg.GetSection("DEFAULT")
	require.NoError(t, err, "Failed to find section DEFAULT in /etc/update-manager/release-upgrades: %s", out)

	gotPolicy, err := section.GetKey("Prompt")
	require.NoError(t, err, "Failed to find key 'Prompt' in DEFAULT section of /etc/update-manager/release-upgrades: %s", out)

	require.Equal(t, wantPolicy, gotPolicy.String(), "Wrong upgrade policy")
}

// testUpgradePolicyIdempotent enures /etc/update-manager/release-upgrades is not modified every boot.
func testUpgradePolicyIdempotent(t *testing.T) { //nolint: thelper, this is a test
	terminateDistro(t)

	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	wantsDate, err := launcherCommand(ctx, "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", wantsDate)

	terminateDistro(t)

	time.Sleep(5 * time.Second) // Allowing time to pass so it'll show up in the last modification date

	ctx, cancel = context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	gotDate, err := launcherCommand(ctx, "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", gotDate)

	require.Equal(t, string(wantsDate), string(gotDate), "Launcher is modifying release upgrade every boot")
}

// testInteropIsEnabled ensures interop works fine.
// See related issue: https://github.com/ubuntu/WSL/issues/334
func testInteropIsEnabled(t *testing.T) { //nolint: thelper, this is a test
	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	got, err := wslCommand(ctx, "powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-Command", `Write-Output "Hello, world!"`).CombinedOutput()
	require.NoError(t, err, "Failed to launch powershell from WSL. Does interop work? %s", got)
	require.Equal(t, "Hello, world!\r\n", string(got), "Unexpected output from powershell")
}

func testHelpFlag(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	// usageFirstLine is the first line printed after '> laucher.exe --help'.
	// The usage message is not localized so it's safe to assert on it.
	const usageFirstLine = "Launches or configures a Linux distribution."

	out, err := launcherCommand(ctx, "run", "help").CombinedOutput()
	require.NoError(t, err, "could not run '%s run help': %v, %s", *launcherName, err, out)
	require.NotContains(t, string(out), usageFirstLine, "help command should not have been picked up by the launcher")

	out, err = launcherCommand(ctx, "-c", "help").CombinedOutput()
	require.NoError(t, err, "could not run '%s -c help': %v, %s", *launcherName, err, out)
	require.NotContains(t, string(out), usageFirstLine, "help command should not have been picked up by the launcher")

	out, err = launcherCommand(ctx, "help").CombinedOutput()
	require.NoError(t, err, "could not run '%s help': %v, %s", *launcherName, err, out)
	require.Contains(t, string(out), usageFirstLine, "help command should have been picked up by the launcher")
}

func testFileExists(t *testing.T, linuxPath string) {
	ctx, cancel := context.WithTimeout(context.Background(), systemdBootTimeout)
	defer cancel()

	out, err := launcherCommand(ctx, "run", "test", "-e", linuxPath).CombinedOutput()
	require.NoError(t, err, "Unexpected error checking file existence: %s", out)
}
