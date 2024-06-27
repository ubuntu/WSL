// package launchertester runs end-to-end tests on Ubuntu's WSL launcher
package launchertester

import (
	"context"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

// TestBasicSetup runs a battery of assertions after installing with the distro launcher.
func TestBasicSetup(t *testing.T) {
	wslSetup(t)

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
	defer cancel()
	// TODO: try to inject user/password to stdin to avoid --root arg.
	out, err := launcherCommand(ctx, "install", "--root").CombinedOutput() // Installing as root to avoid Stdin
	require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", out, err)

	testCases := map[string]func(t *testing.T){
		"SystemdEnabled":          testSystemdEnabled,
		"SystemdUnits":            testSystemdUnits,
		"CorrectUpgradePolicy":    testCorrectUpgradePolicy,
		"UpgradePolicyIdempotent": testUpgradePolicyIdempotent,
		"InteropIsEnabled":        testInteropIsEnabled,
		"HelpFlag":                testHelpFlag,
	}

	for name, tc := range testCases {
		t.Run(name, tc)
	}
}

// TestSetupWithCloudInit runs a battery of assertions after installing with the distro launcher and cloud-init.
func TestSetupWithCloudInit(t *testing.T) {
	testCases := map[string]struct {
		install_root bool
		wantUser     string
		wantFile     string
	}{
		"With default user in conf":    {wantUser: "testuser", wantFile: "/etc/with_default_user.done"},
		"Without default user in conf": {wantUser: "testuser", wantFile: "/home/testuser/with_default_user.done"},
		"Without checking user":        {install_root: true, wantUser: "root", wantFile: "/home/testuser/with_default_user.done"},
	}

	home, err := os.UserHomeDir()
	require.NoError(t, err, "Cannot get user home directory")
	cloudinitdir := filepath.Join(home, ".cloud-init")
	backupDir := filepath.Join(t.TempDir(), "cloud-init-backup")
	if _, err = os.Stat(cloudinitdir); err == nil {
		require.NoError(t, os.Rename(cloudinitdir, backupDir), "Failed to backup cloud-init directory")
	}
	require.NoError(t, os.MkdirAll(cloudinitdir, 0755), "Cannot create cloud-init directory")
	t.Cleanup(func() {
		if err := os.RemoveAll(cloudinitdir); err != nil {
			t.Logf("Failed to remove user-data file after test: %v", err)
		}
		if err = os.Rename(backupDir, cloudinitdir); err != nil {
			t.Logf("Failed to restore cloud-init directory after test: %v", err)
		}
	})

	for name, tc := range testCases {
		t.Run(name, func(t *testing.T) {
			wslSetup(t)

			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
			defer cancel()

			userdata, err := os.ReadFile(TestFixturePath(t))
			require.NoError(t, err, "Cannot read test user-data file")
			err = os.WriteFile(filepath.Join(cloudinitdir, "default.user-data"), userdata, 0755)
			require.NoError(t, err, "Cannot write user-data file to the right location")

			var args []string
			if tc.install_root {
				args = append(args, "--root")
			}
			out, err := launcherCommand(ctx, "install", args...).CombinedOutput() // Using the "install" command to avoid the shell after installation.
			require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", out, err)

			testSystemdEnabled(t)
			testInteropIsEnabled(t)
			testFileExists(t, tc.wantFile)
			testDefaultUser(t, tc.wantUser)
		})
	}
}
