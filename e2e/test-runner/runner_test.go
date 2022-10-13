package test_runner

import (
	"gopkg.in/ini.v1"
	"strings"
	"testing"
)

func expectedUpgradePolicy() string {
	if *distroName == "Ubuntu" {
		return "lts"
	}
	if strings.HasPrefix(*distroName, "Ubuntu") && strings.HasSuffix(*distroName, "LTS") {
		return "never"
	}
	// Preview and Dev
	return "normal"
}

func TestBasicSetup(t *testing.T) {
	// Wrapping testing.T to get the friendly additions to its API as declared in `wsl_tester.go`.
	tester := WslTester(t)

	// Invoke the launcher as if the user did: `<launcher>.exe install --ui=gui`
	outStr := tester.AssertLauncherCommand("install --root --ui=none") // without install the launcher will run bash.
	if len(outStr) == 0 {
		tester.Fatal("Failed to install the distro: No output produced.")
	}

	// After completing the setup, the OOBE must have created a new user and the distro launcher must have set it as the default.
	{
		outputStr := tester.AssertWslCommand("whoami")
		notRoot := strings.Fields(outputStr)[0]
		if notRoot == "root" {
			tester.Logf("%s", outputStr)
			tester.Fatal("Default user should not be root.")
		}
	}

	// Subiquity either installs or marks for installation the relevant language packs for the chosen language.
	{
		outputStr := tester.AssertWslCommand("apt-mark", "showinstall", "language-pack\\*")
		packs := strings.Fields(outputStr)
		tester.Logf("%s", outputStr) // I'd like to see what packs were installed in the end, just in case.
		if len(packs) == 0 {
			tester.Fatal("At least one language pack should have been installed or marked for installation, but apt-mark command output is empty.")
		}
	}

	// Proper release. Ensures the right tarball was used as rootfs
	{
		expectedRelease := func() string {
			if *distroName == "Ubuntu-Preview" || *distroName == "UbuntuDev.WslID.Dev" {
				return "Ubuntu Kinetic Kudu (development branch)"
			}
			if *distroName == "Ubuntu" {
				return "Ubuntu 22.04.1 LTS"
			}
			if *distroName == "Ubuntu22.04LTS" {
				return "Ubuntu 22.04.1 LTS"
			}
			if *distroName == "Ubuntu20.04LTS" {
				return "Ubuntu 20.04.5 LTS"
			}
			if *distroName == "Ubuntu18.04LTS" {
				return "Ubuntu 18.04.6 LTS"
			}
			return ""
		}()
		if len(expectedRelease) == 0 {
			tester.Logf("Unknown Ubuntu release corresponding to distro name '%s'", *distroName)
			tester.Fatal("Unexpected value provided via --distro-name")
		}
		outputStr := tester.AssertWslCommand("lsb_release", "-a")
		release := string("")
		prefix := "Description:"
		for _, line := range strings.Split(outputStr, "\n") {
			if strings.HasPrefix(line, prefix) {
				release = strings.TrimSpace(line[len(prefix):])
				break
			}
		}
		if len(release) == 0 {
			tester.Logf("Output from lsb_release -a:\n%s", outputStr)
			tester.Fatal("Could not parse release")
		}
		if release != expectedRelease {
			tester.Logf("Output from lsb_release -a:\n%s", outputStr)
			tester.Logf("Parsed release:   %s", release)
			tester.Logf("Expected release: %s", expectedRelease)
			tester.Fatal("Unexpected release string")
		}
	}

	// Systemd enabled. AssertWslCommand failure is allowed because degraded status exits with code 3
	{
		outputStr := tester.AssertWslCommand("bash", "-ec", "systemctl is-system-running || exit 0")
		lines := strings.Fields(outputStr)
		if len(lines) == 0 {
			tester.Fatal("systemctl is-system-running printed nothing")
		}
		if lines[0] != "degraded" && lines[0] != "running" {
			tester.Logf("%s", lines[0])
			tester.Fatal("Systemd should have been enabled")
		}
	}

	// Sysusers service fix
	{
		tester.AssertWslCommand("systemctl", "status", "systemd-sysusers.service")
	}

	// ---------------- Reboot with launcher -----------------
	tester.AssertOsCommand("wsl.exe", "-t", *distroName)
	tester.AssertLauncherCommand("run echo Hello")

	// Upgrade policy
	{
		outputStr := tester.AssertWslCommand("cat", "/etc/update-manager/release-upgrades")
		cfg, err := ini.Load([]byte(outputStr))
		if err != nil {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Fatal("Failed to parse ini file")
		}

		val := cfg.Section("DEFAULT").Key("Prompt").String()
		if val != expectedUpgradePolicy() {
			tester.Logf("Contents of /etc/update-manager/release-upgrades:\n%s", outputStr)
			tester.Logf("Parsed policy: %s", val)
			tester.Logf("Expected policy: %s", expectedUpgradePolicy())
			tester.Fatal("Wrong upgrade policy")
		}

		release_upgrades_date := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")

		// ---------------- Reboot with launcher -----------------
		tester.AssertOsCommand("wsl.exe", "-t", *distroName)
		tester.AssertLauncherCommand("run echo Hello")

		new_date := tester.AssertWslCommand("date", "-r", "/etc/update-manager/release-upgrades")
		if release_upgrades_date != new_date {
			tester.Logf("Launcher is modifying release upgrade file more than once")
		}
	}

}
