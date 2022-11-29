package test_runner

import (
	"bufio"
	"bytes"
	"context"
	"os/exec"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"gopkg.in/ini.v1"
)

// testUserNotRoot ensures the default user is not root.
func testUserNotRoot(t *testing.T) {
	t.Parallel()
	out, err := wslCommand(context.Background(), "whoami").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing whoami: %s", out)

	require.NotContains(t, string(out), "root", "Default user should not be root.")
}

// testLanguagePacksMarked ensures the subiquity either installs or marks for installation the relevant language packs for the chosen language.
func testLanguagePacksMarked(t *testing.T) {
	t.Skip("Skipping: This is a known bug") // TODO: Investigate and fix
	t.Parallel()

	out, err := wslCommand(context.Background(), "apt-mark", "showinstall", "language-pack\\*").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing apt-mark: %s", out)
	require.NotEmpty(t, string(out), "At least one language pack should have been installed or marked for installation, but apt-mark command output is empty.")
}

// Proper release. Ensures the right tarball was used as rootfs.
func testCorrectReleaseRootfs(t *testing.T) {
	t.Parallel()
	var expectedRelease string

	distroNameToRelease := map[string]string{
		"Ubuntu":         "22.04",
		"Ubuntu22.04LTS": "22.04",
		"Ubuntu20.04LTS": "20.04",
		"Ubuntu18.04LTS": "18.04",
		"UbuntuPreview":  "22.10",
	}

	expectedRelease, ok := distroNameToRelease[*distroName]
	if !ok {
		expectedRelease = distroNameToRelease["UbuntuPreview"]
	}

	out, err := wslCommand(context.Background(), "lsb_release", "-r").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing lsb_release: %s", out)

	// Example 'out': "Release:        22.04"
	k, v, found := strings.Cut(string(out), ":")
	require.True(t, found, "Failed to parse '<key>: <value>' from input %q", out)
	require.Equal(t, k, "Release", "Failed to parse 'Release: <value>' from input %q", out)
	release := strings.TrimSpace(v)

	require.Equal(t, expectedRelease, release, "Mismatching releases")
}

// testSystemdEnabled ensures systemd was enabled.
func testSystemdEnabled(t *testing.T) {
	if !systemdExpected() {
		t.Skipf("Skipping systemd checks on %s", *distroName)
	}
	t.Parallel()

	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
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

// testSystemdUnits ensures the list of failed units does not regress
func testSystemdUnits(t *testing.T) {
	if !systemdExpected() {
		// Enable systemd in config file
		wslCommandAsUser(context.Background(), "root", "bash", "-c", `printf '\n[boot]\nsystemd=true\n' >> /etc/wsl.conf`)
		// Restore config file
		defer wslCommandAsUser(context.Background(), "root", "bash", "-c", `head -n -2 "/etc/wsl.conf" > "/etc/wsl.conf"`)
	}

	// Terminating to starts systemd, and to ensure no services from previous tests are running
	terminateDistro(t)

	distroNameToFailedUnits := map[string][]string{
		"Ubuntu18.04LTS": {"user@0.service", "atd.service", "systemd-modules-load.service"},
		"Ubuntu20.04LTS": {"user@0.service", "atd.service", "ssh.service", "systemd-remount-fs.service", "multipathd.socket"},
		"Ubuntu22.04LTS": {"user@0.service", "systemd-sysusers.service"},
		"Ubuntu":         {"user@0.service", "systemd-sysusers.service"},
		"UbuntuPreview":  {"user@0.service"},
	}

	expectedFailure, ok := distroNameToFailedUnits[*distroName]
	if !ok { // Development version
		expectedFailure = distroNameToFailedUnits["UbuntuPreview"]
	}

	// Reading failed units
	out, err := wslCommand(context.Background(), "systemctl", "list-units", "--state=failed", "--plain", "--no-legend", "--no-pager").CombinedOutput()
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

// testCorrectUpgradePolicy ensures upgrade policy matches the one expected for the app
func testCorrectUpgradePolicy(t *testing.T) {
	t.Parallel()

	/* Ubuntu always upgrade to next lts */
	wantPolicy := "lts"
	if strings.HasPrefix(*distroName, "Ubuntu") && strings.HasSuffix(*distroName, "LTS") {
		wantPolicy = "never"
	} else if *distroName != "Ubuntu" {
		// Preview and Dev
		wantPolicy = "normal"
	}

	out, err := launcherCommand(context.Background(), "run", "cat", "/etc/update-manager/release-upgrades").CombinedOutput()
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

// testUpgradePolicyIdempotent enures /etc/update-manager/release-upgrades is not modified every boot
func testUpgradePolicyIdempotent(t *testing.T) {
	terminateDistro(t)

	wantsDate, err := launcherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", wantsDate)

	terminateDistro(t)

	time.Sleep(5 * time.Second) // Allowing time to pass so it'll show up in the last modification date

	gotDate, err := launcherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", gotDate)

	require.Equal(t, string(wantsDate), string(gotDate), "Launcher is modifying release upgrade every boot")
}

// systemdExpected returns true if systemd is expected to be enabled by default on this distro
func systemdExpected() bool {
	distroNameToExpectSystemd := map[string]bool{
		"Ubuntu":         false,
		"Ubuntu22.04LTS": false,
		"Ubuntu20.04LTS": false,
		"Ubuntu18.04LTS": false,
		"UbuntuPreview":  true,
	}

	expectSystemd, ok := distroNameToExpectSystemd[*distroName]
	if !ok { // Development version
		return distroNameToExpectSystemd["UbuntuPreview"]
	}
	return expectSystemd
}
