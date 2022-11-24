package test_runner

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"
)

// TestBasicSetupGUI runs a battery of assertions after installing with the graphical OOBE.
func TestBasicSetupGUI(t *testing.T) {
	WSLSetup(t)

	outp, err := LauncherCommand(context.Background(), "install", "--ui=gui").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", outp, err)
	require.Greaterf(t, len(outp), 0, "Failed to install the distro: No output produced.")

	testCases := map[string]func(t *testing.T){
		"UserNotRoot":             tcUserNotRoot,
		"LanguagePacksMarked":     tcLanguagePacksMarked,
		"CorrectReleaseRootfs":    tcCorrectReleaseRootfs,
		"SystemdEnabled":          tcSystemdEnabled,
		"SysusersServiceWorks":    tcSysusersServiceWorks,
		"CorrectUpgradePolicy":    tcCorrectUpgradePolicy,
		"UpgradePolicyIdempotent": tcUpgradePolicyIdempotent,
	}

	for name, tc := range testCases {
		t.Run(name, tc)
	}
}

// TestBasicSetupGUI runs a battery of assertions after installing with WSL's default OOBE.
func TestBasicSetupFallbackUI(t *testing.T) {
	WSLSetup(t)

	outp, err := LauncherCommand(context.Background(), "install", "--root", "--ui=none").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected error installing: %s\n%v", outp, err)
	require.Greaterf(t, len(outp), 0, "Failed to install the distro: No output produced.")

	testCases := map[string]func(t *testing.T){
		"LanguagePacksMarked":     tcLanguagePacksMarked,
		"CorrectReleaseRootfs":    tcCorrectReleaseRootfs,
		"SystemdEnabled":          tcSystemdEnabled,
		"SysusersServiceWorks":    tcSysusersServiceWorks,
		"CorrectUpgradePolicy":    tcCorrectUpgradePolicy,
		"UpgradePolicyIdempotent": tcUpgradePolicyIdempotent,
	}

	for name, tc := range testCases {
		t.Run(name, tc)
	}
}
