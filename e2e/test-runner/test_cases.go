package test_runner

import (
	"context"
	"os/exec"
	"strings"
	"testing"
	"time"

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
	t.Parallel()

	distroNameToAllowDegraded := map[string]bool{
		"Ubuntu":         true,
		"Ubuntu22.04LTS": true,
		"Ubuntu20.04LTS": true,
		"Ubuntu18.04LTS": true,
		"UbuntuPreview":  false,
	}

	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	out, err := wslCommand(ctx, "systemctl", "is-system-running", "--wait").CombinedOutput()

	if distroNameToAllowDegraded[*distroName] {
		var target *exec.ExitError
		require.ErrorAs(t, err, &target, "systemctl is-system-running: Only acceptable error is ExitError (caused by systemd being degraded)")
		require.Equal(t, target.ExitCode(), 1, "systemctl is-system-running: Only acceptable non-zero exit code is 1 (degraded)")
		require.Contains(t, string(out), "degraded", "systemd output should be degraded")
		return
	}

	require.NoErrorf(t, err, "Unexpected failure after systemctl is-system-running. Output: %s", out)
}

// testSysusersServiceWorks ensures the sysusers service is fixed
func testSysusersServiceWorks(t *testing.T) {
	t.Parallel()

	out, err := wslCommand(context.Background(), "systemctl", "status", "systemd-sysusers.service").CombinedOutput()
	require.NoError(t, err, "Unexpected failure checking for status of systemd-sysusers.service: %s", out)
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
	out, err := exec.Command("wsl.exe", "--terminate", *distroName).CombinedOutput()
	require.NoError(t, err, "Failed to shut down WLS: %s", out)

	wantsDate, err := launcherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", wantsDate)

	out, err = exec.Command("wsl.exe", "--terminate", *distroName).CombinedOutput()
	require.NoError(t, err, "Failed to shut down WLS: %s", out)

	time.Sleep(5 * time.Second) // Allowing time to pass so it'll show up in the last modification date

	gotDate, err := launcherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", gotDate)

	require.Equal(t, string(wantsDate), string(gotDate), "Launcher is modifying release upgrade every boot")
}
