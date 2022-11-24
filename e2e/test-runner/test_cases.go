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

// tcUserNotRoot ensures the default user is not root.
func tcUserNotRoot(t *testing.T) {
	t.Parallel()
	out, err := WslCommand(context.Background(), "whoami").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing whoami: %s", out)

	usr := strings.Fields(string(out))[0]
	require.NotEqualf(t, "root", usr, "Default user should not be root.")
}

// tcLanguagePacksMarked ensures the subiquity either installs or marks for installation the relevant language packs for the chosen language.
func tcLanguagePacksMarked(t *testing.T) {
	t.Skip("Skipping: This is a known bug")
	t.Parallel()
	out, err := WslCommand(context.Background(), "apt-mark", "showinstall", "language-pack\\*").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing apt-mark: %s", out)

	t.Logf(string(out))
	packs := strings.Fields(string(out))
	require.Greater(t, len(packs), 0, "At least one language pack should have been installed or marked for installation, but apt-mark command output is empty")
}

// Proper release. Ensures the right tarball was used as rootfs.
func tcCorrectReleaseRootfs(t *testing.T) {
	t.Parallel()
	var expectedRelease string
	switch *distroName {
	case "Ubuntu":
		expectedRelease = "22.04"
	case "Ubuntu22.04LTS":
		expectedRelease = "22.04"
	case "Ubuntu20.04LTS":
		expectedRelease = "20.04"
	case "Ubuntu18.04LTS":
		expectedRelease = "18.04"
	default: // Preview and Dev
		expectedRelease = "22.10"
	}

	out, err := WslCommand(context.Background(), "lsb_release", "-r").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected failure executing lsb_release: %s", out)

	key, value, found := strings.Cut(string(out), ":")
	require.True(t, found, "Failed to parse '<key>: <value>' from input %q", out)
	require.Equal(t, key, "Release", "Failed to parse 'Release: <value>' from input %q", out)
	release := strings.TrimSpace(value)

	require.Equal(t, expectedRelease, release, "Mismatching releases")
}

// tcSystemdEnabled ensures systemd was enabled.
func tcSystemdEnabled(t *testing.T) {
	t.Parallel()
	out, err := WslCommand(context.Background(), "busctl", "list", "--no-pager").CombinedOutput()
	require.NoError(t, err, "Unexpected failure executing busctl: %s", out)

	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	out, err = WslCommand(ctx, "systemctl", "is-system-running", "--wait").CombinedOutput()
	if err == nil {
		return // Success
	}
	var target *exec.ExitError
	require.ErrorAs(t, err, &target, "systemctl is-system-running: Only acceptable error is ExitError (caused by systemd being degraded)")
	require.Equal(t, target.ExitCode(), 1, "systemctl is-system-running: Only acceptable non-zero exit code is 1 (degraded)")
	require.Contains(t, string(out), "degraded")
}

// tcSysusersServiceWorks ensures the sysusers service is fixed
func tcSysusersServiceWorks(t *testing.T) {
	t.Parallel()
	out, err := WslCommand(context.Background(), "systemctl", "status", "systemd-sysusers.service").CombinedOutput()
	require.NoError(t, err, "Unexpected failure checking for status of systemd-sysusers.service: %s", out)
}

// tcCorrectUpgradePolicy ensures upgrade policy matches the one expected for the app
func tcCorrectUpgradePolicy(t *testing.T) {
	t.Parallel()
	out, err := LauncherCommand(context.Background(), "run", "cat", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Unexpected failure executing cat: %s", out)
	require.Greater(t, len(out), 0, "Release upgrades file is empty")

	cfg, err := ini.Load(out)
	require.NoError(t, err, "Failed to parse ini file: %s", out)

	section, err := cfg.GetSection("DEFAULT")
	require.NoError(t, err, "Failed to find section DEFAULT in /etc/update-manager/release-upgrades: %s", out)

	key, err := section.GetKey("Prompt")
	require.NoError(t, err, "Failed to find key 'Prompt' in DEFAULT section of /etc/update-manager/release-upgrades: %s", out)

	gotPolicy := key.String()

	var wantPolicy string
	if *distroName == "Ubuntu" {
		wantPolicy = "lts"
	} else if strings.HasPrefix(*distroName, "Ubuntu") && strings.HasSuffix(*distroName, "LTS") {
		wantPolicy = "never"
	} else /* Preview and Dev */ {
		wantPolicy = "normal"
	}

	require.Equal(t, wantPolicy, gotPolicy, "Wrong upgrade policy")
}

// tcUpgradePolicyIdempotent enures /etc/update-manager/release-upgrades is not modified every boot
func tcUpgradePolicyIdempotent(t *testing.T) {
	out, err := exec.Command("wsl.exe", "--terminate", *distroName).CombinedOutput()
	require.NoError(t, err, "Failed to shut down WLS: %s", out)

	out, err = LauncherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", out)
	wantsDate := string(out)

	out, err = exec.Command("wsl.exe", "--terminate", *distroName).CombinedOutput()
	require.NoError(t, err, "Failed to shut down WLS: %s", out)

	time.Sleep(5 * time.Second) // Allowing time to pass so it'll show up in the last modification date

	out, err = LauncherCommand(context.Background(), "run", "date", "-r", "/etc/update-manager/release-upgrades").CombinedOutput()
	require.NoError(t, err, "Failed to execute date: %s", out)
	gotDate := string(out)

	require.Equal(t, wantsDate, gotDate, "Launcher is modifying release upgrade every boot")
}
