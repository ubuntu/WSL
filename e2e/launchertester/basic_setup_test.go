// package launchertester runs end-to-end tests on Ubuntu's WSL launcher
package launchertester

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"github.com/ubuntu/gowsl"
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
		"SnapdWorks":              testSnapdWorks,
	}

	for name, tc := range testCases {
		t.Run(name, tc)
	}
}

// TestSetupWithCloudInit runs a battery of assertions after installing with the distro launcher and cloud-init.
func TestSetupWithCloudInit(t *testing.T) {
	testCases := map[string]struct {
		install_root     bool
		withRegistryUser string
		withWSL1         bool
		wantUser         string
		wantFile         string
	}{
		"With default user in conf":     {wantUser: "testuser", wantFile: "/etc/with_default_user.done"},
		"With default user in registry": {withRegistryUser: "testuser", wantUser: "testuser", wantFile: "/etc/with_default_user.done"},
		"With default user in both":     {withRegistryUser: "anotheruser", wantUser: "testuser"},
		"With default user in none":     {wantUser: "testuser", wantFile: "/home/testuser/with_default_user.done"},

		"With only remote users":  {wantUser: "testmail"},
		"With broken passwd file": {wantUser: "testmail"},
		"Without checking user":   {install_root: true, wantUser: "root", wantFile: "/home/testuser/with_default_user.done"},
		"Do not block on WSL1":    {install_root: true, wantUser: "root"},
	}

	home, err := os.UserHomeDir()
	require.NoError(t, err, "Setup: Cannot get user home directory")
	cloudinitdir := filepath.Join(home, ".cloud-init")
	backupDir := filepath.Join(t.TempDir(), "cloud-init-backup")
	if _, err = os.Stat(cloudinitdir); err == nil {
		require.NoError(t, os.Rename(cloudinitdir, backupDir), "Failed to backup cloud-init directory")
	}
	require.NoError(t, os.MkdirAll(cloudinitdir, 0755), "Setup: Cannot create cloud-init directory")
	t.Cleanup(func() {
		if err := os.RemoveAll(cloudinitdir); err != nil {
			t.Logf("Setup: Failed to remove user-data file after test: %v", err)
		}
		if err = os.Rename(backupDir, cloudinitdir); err != nil {
			t.Logf("Setup: Failed to restore cloud-init directory after test: %v", err)
		}
	})

	for name, tc := range testCases {
		t.Run(name, func(t *testing.T) {
			if tc.withWSL1 {
				require.NoError(t, exec.Command("wsl.exe", "--set-default-version", "1").Run(), "Setup: Cannot set WSL1 as default version")
				t.Cleanup(func() {
					t.Log("Cleaning up: Setting WSL2 back as default version")
					require.NoError(t, exec.Command("wsl.exe", "--set-default-version", "2").Run(), "Setup: Cannot set WSL2 back as default version")
				})
			}
			wslSetup(t)

			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
			defer cancel()

			userdata, err := os.ReadFile(TestFixturePath(t))
			require.NoError(t, err, "Setup: Cannot read test user-data file")
			err = os.WriteFile(filepath.Join(cloudinitdir, "default.user-data"), userdata, 0755)
			require.NoError(t, err, "Setup: Cannot write user-data file to the right location")

			var args []string
			if tc.install_root {
				args = append(args, "--root")
			}

			registrySet := make(chan error)
			if len(tc.withRegistryUser) == 0 {
				close(registrySet)
			} else {
				go func() {
					defer close(registrySet)

					d := gowsl.NewDistro(ctx, *distroName)
					for {
						select {
						case <-ctx.Done():
							registrySet <- ctx.Err()
							return
						default:
						}

						state, err := d.State()
						if err != nil {
							// WSL is not ready for concurrent access. Errors are very likely when registering or unregistering.
							t.Logf("Setup: Couldn't to get distro state this time: %v", err)
							time.Sleep(10 * time.Second)
							continue
						}

						if state == gowsl.Uninstalling {
							registrySet <- fmt.Errorf("Setup: too late to set the registry: distro is uninstalling")
							return
						}

						if state == gowsl.NonRegistered || state == gowsl.Installing {
							t.Logf("Setup: Waiting for distro to be registered to set default user via registry")
							time.Sleep(10 * time.Second)
							continue
						}
						// if running or stopped
						id := wslCommand(ctx, "id", "-u", tc.withRegistryUser)
						out, err := id.CombinedOutput()
						if err != nil {
							t.Logf("Setup: Failed to get uid for %s: %v", tc.withRegistryUser, err)
							time.Sleep(300 * time.Millisecond)
							continue
						}
						uid, err := strconv.Atoi(strings.TrimSpace(string(out)))
						if err != nil {
							t.Logf("Setup: Failed to parse %s: %v", out, err)
							time.Sleep(300 * time.Millisecond)
							continue
						}
						// We use cloud-init to create the user with cloud-config data we control, so we expect it to be in the range of normal users.
						if uid < 1000 || uid > 60000 {
							registrySet <- fmt.Errorf("Setup: unexpected uid: %d", uid)
							return
						}
						registrySet <- d.DefaultUID(uint32(uid))
						return
					}
				}()
			}

			out, err := launcherCommand(ctx, "install", args...).CombinedOutput() // Using the "install" command to avoid the shell after installation.
			require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", out, err)

			// Seems out of order but setting the registry is concurrent with the installation, hopefully it will be set before the
			// launcher checks for the default user. Either way the user assertion in the end of this test case will work as exoected.
			require.NoError(t, <-registrySet, "Setup: Failed to set default user via GoWSL/registry")

			testSystemdEnabled(t)
			testInteropIsEnabled(t)
			if len(tc.wantFile) > 0 {
				testFileExists(t, tc.wantFile)
			}
			testDefaultUser(t, tc.wantUser)
		})
	}
}
