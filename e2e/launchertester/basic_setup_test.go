// package launchertester runs end-to-end tests on Ubuntu's WSL launcher
package launchertester

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

// TestBasicSetupGUI runs a battery of assertions after installing with the graphical OOBE.
func TestBasicSetupGUI(t *testing.T) {
	wslSetup(t)

	ctx, cancel := context.WithTimeout(context.Background(), installTimeout)
	defer cancel()
	out, err := launcherCommand(ctx, "install", "--ui=gui").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected error installing: %s\nLogs:%s", out, subiquityLogs(t))
	// TODO: check with Carlos if this is necessary
	require.NotEmpty(t, out, "Failed to install the distro: No output produced. Logs:%s", subiquityLogs(t))

	testCases := map[string]func(t *testing.T){
		"UserNotRoot":             testUserNotRoot,
		"LanguagePacksMarked":     testLanguagePacksMarked,
		"CorrectReleaseRootfs":    testCorrectReleaseRootfs,
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

// TestBasicSetupGUI runs a battery of assertions after installing with WSL's default OOBE.
func TestBasicSetupFallbackUI(t *testing.T) {
	wslSetup(t)

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
	defer cancel()
	// TODO: try to inject user/password to stdin to avoid --root arg.
	out, err := launcherCommand(ctx, "install", "--root", "--ui=none").CombinedOutput() // Installing as root to avoid Stdin
	require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", out, err)
	// TODO: check with Carlos if this is necessary
	require.NotEmpty(t, out, "Failed to install the distro: No output produced.")

	testCases := map[string]func(t *testing.T){
		// Skipping testUserNotRoot because we installed as --root
		"LanguagePacksMarked":     testLanguagePacksMarked,
		"CorrectReleaseRootfs":    testCorrectReleaseRootfs,
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
