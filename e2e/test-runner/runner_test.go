package test_runner

import (
	"bytes"
	"context"
	"gopkg.in/ini.v1"
	"os/exec"
	"strings"
	"testing"
	"time"
)

func expectedUpgradePolicy(distro string) string {
	if distro == "Ubuntu" {
		return "lts"
	}
	if strings.HasPrefix(distro, "Ubuntu") && strings.HasSuffix(distro, "LTS") {
		return "never"
	}
	// Preview and Dev
	return "normal"
}

// After completing the setup, the OOBE must have created a new user and the distro launcher must have set it as the default.
func assertUserNotRoot(tester *Tester) {
	outputStr := tester.AssertWslCommand("whoami")
	notRoot := strings.Fields(outputStr)[0]
	if notRoot == "root" {
		tester.Logf("%s", outputStr)
		tester.Fatal("Default user should not be root.")
	}
}

// Subiquity either installs or marks for installation the relevant language packs for the chosen language.
func assertLanguagePacksMarked(tester *Tester) {
	outputStr := tester.AssertWslCommand("apt-mark", "showinstall", "language-pack\\*")
	packs := strings.Fields(outputStr)
	tester.Logf("%s", outputStr) // I'd like to see what packs were installed in the end, just in case.
	if len(packs) == 0 {
		tester.Fatal("At least one language pack should have been installed or marked for installation, but apt-mark command output is empty.")
	}
}

// Proper release. Ensures the right tarball was used as rootfs.
func assertCorrectReleaseRootfs(tester *Tester) {
	expectedRelease := func() string {
		if *distroName == "Ubuntu-Preview" {
			return "22.10"
		}
		if *distroName == "Ubuntu" {
			return "22.04"
		}
		if *distroName == "Ubuntu22.04LTS" {
			return "22.04"
		}
		if *distroName == "Ubuntu20.04LTS" {
			return "20.04"
		}
		if *distroName == "Ubuntu18.04LTS" {
			return "18.04"
		}
		return "22.10" // Development version
	}()
	if len(expectedRelease) == 0 {
		tester.Logf("Unknown Ubuntu release corresponding to distro name '%s'", *distroName)
		tester.Fatal("Unexpected value provided via --distro-name")
	}
	outputStr := tester.AssertWslCommand("lsb_release", "-r")
	cfg, err := ini.Load([]byte(outputStr))
	if err != nil {
		tester.Logf("Output of lsb_release -r:\n%s", outputStr)
		tester.Fatal("Failed to parse ini file")
	}

	release := cfg.Section(ini.DefaultSection).Key("Release")
	if release.String() != expectedRelease {
		tester.Logf("Output of lsb_release -r:\n%s", outputStr)
		tester.Logf("Parsed release:   %s", release.String())
		tester.Logf("Expected release: %s", expectedRelease)
		tester.Fatal("Unexpected release string")
	}
}

// Ensures systemd was enabled.
func assertSystemdEnabled(tester *Tester) {
	tester.AssertWslCommand("busctl", "list", "--no-pager")

	getSystemdStatus := func() string {
		outputStr := tester.AssertWslCommand("bash", "-ec", "systemctl is-system-running --wait || exit 0")
		return strings.TrimSpace(outputStr)
	}

	status := getSystemdStatus()
	if status != "degraded" && status != "running" {
		tester.Logf("%s", status)
		tester.Fatal("Systemd failed to start")
	}
}

// Sysusers service fix
func assertSysusersServiceWorks(tester *Tester) {
	tester.AssertWslCommand("systemctl", "status", "systemd-sysusers.service")
}

// Upgrade policy matches launcher
func assertCorrectUpgradePolicy(tester *Tester) {
	outputStr := tester.AssertWslCommand("cat", "/etc/update-manager/release-upgrades")
	cfg, err := ini.Load([]byte(outputStr))
	if err != nil {
		tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
		tester.Fatal("Failed to parse ini file")
	}

	section, err := cfg.GetSection("DEFAULT")
	if err != nil {
		tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
		tester.Fatal("Failed to find section DEFAULT in /etc/update-manager/release-upgrades")
	}
	value, err := section.GetKey("Prompt")
	if err != nil {
		tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
		tester.Fatal("Failed to find key 'Prompt' in DEFAULT section of /etc/update-manager/release-upgrades")
	}

	expectedPolicy := expectedUpgradePolicy(*distroName)
	if value.String() != expectedUpgradePolicy(expectedPolicy) {
		tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
		tester.Logf("Parsed policy: %s", value.String())
		tester.Logf("Expected policy: %s", expectedUpgradePolicy(expectedPolicy))
		tester.Fatal("Wrong upgrade policy")
	}
}

// Upgrade policy is not overriden every launch
func assertUpgradePolicyAppliedOnce(tester *Tester, creationTimestamp string) {
	tester.AssertOsCommand("wsl.exe", "-t", *distroName)
	tester.AssertLauncherCommand("run echo Hello")

	newTimestamp := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")
	if creationTimestamp != newTimestamp {
		tester.Logf("Launcher is modifying release upgrade file more than once")
	}
}

// Shuts down, then boots with the launcher
func assertRebootWithLauncher(tester *Tester) {
	tester.AssertOsCommand("wsl.exe", "-t", *distroName)
	tester.AssertLauncherCommand("run echo Hello")
}

func assertGetDistroStatus(t *Tester) string {
	// wsl -l -v outputs UTF-16 (See https://github.com/microsoft/WSL/issues/4607)
	// We use $env:WSL_UTF8=1 to prevent this (Available from 0.64.0 onwards https://github.com/microsoft/WSL/releases/tag/0.64.0)
	str := t.AssertOsCommand("powershell.exe", "-noninteractive", "-nologo", "-noprofile", "-command",
		"$env:WSL_UTF8=1 ; wsl -l -v")

	for _, line := range strings.Split(str, "\n")[1:] {
		columns := strings.Fields(line)
		// columns = {[*], distroName, status, WSL-version}
		if len(columns) < 3 {
			continue
		}
		idx := 0
		if columns[idx] == "*" {
			idx++
		}
		if columns[idx] == *distroName {
			return columns[idx+1]
		}
	}
	return "DistroNotFound"
}

func TestBasicSetup(t *testing.T) {
	// Wrapping testing.T to get the friendly additions to its API as declared in `wsl_tester.go`.
	tester := WslTester(t)

	// Invoke the launcher as if the user did: `<launcher>.exe install --ui=gui`
	outStr := tester.AssertLauncherCommand("install --ui=gui") // without install the launcher will run bash.
	if len(outStr) == 0 {
		tester.Fatal("Failed to install the distro: No output produced.")
	}

	assertUserNotRoot(&tester)
	assertLanguagePacksMarked(&tester)
	assertCorrectReleaseRootfs(&tester)
	assertSystemdEnabled(&tester)
	assertSysusersServiceWorks(&tester)

	assertRebootWithLauncher(&tester)
	assertCorrectUpgradePolicy(&tester)

	release_upgrades_date := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")
	assertRebootWithLauncher(&tester)
	assertUpgradePolicyAppliedOnce(&tester, release_upgrades_date)
}

// This tests the experience that most users have:
// opening WSL from the store or from the Start menu
func TestDefaultExperience(t *testing.T) {
	tester := WslTester(t)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	shellCommand := []string{"-noninteractive", "-nologo", "-noprofile", "-command",
		*launcherName, "install --root --ui=none"} // TODO: Change to ...*launcherName, "--hide-console")
	cmd := exec.CommandContext(ctx, "powershell.exe", shellCommand...)

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out

	cmd.Start()

	assertStatusTransition := func(fromStatus string, toStatus string) string {
		status := assertGetDistroStatus(&tester)
		for status == fromStatus {
			time.Sleep(1 * time.Second)
			status = assertGetDistroStatus(&tester)
		}
		if status != toStatus {
			tester.Logf("In transition from '%s' to '%s': Unexpected state '%s'", fromStatus, toStatus, status)
			tester.Logf("Output: %s", out.String())
			tester.Fatal("Unexpected Distro state transition")
		}
		return status
	}

	// Completing installation
	assertStatusTransition("DistroNotFound", "Installing")
	assertStatusTransition("Installing", "Running")
	assertStatusTransition("Running", "Stopped")

	// Ensuring proper installation
	if !strings.Contains(out.String(), "Installation successful!") { // TODO: Change this to parse MOTD
		tester.Logf("Status: %s", assertGetDistroStatus(&tester))
		tester.Logf("Output: %s", out.String())
		tester.Fatal("Distro was shut down without finishing install")
	}
}
