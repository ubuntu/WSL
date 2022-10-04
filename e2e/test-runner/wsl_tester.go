package test_runner

import (
	"flag"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// Defaults choosen based on the default state of the repository on git-clone.
const defaultLauncherName = "ubuntudev.launchername.dev.exe"
const defaultDistroName = "UbuntuDev.WslId.Dev"

const serverLogPath = "/var/log/installer/systemsetup-server-debug.log"
const clientLogPath = "ubuntu-desktop-installer/packages/ubuntu_wsl_setup/build/windows/runner/Debug/.ubuntu_wsl_setup.exe/ubuntu_wsl_setup.exe.log"

var launcherName = flag.String("launcher-name", defaultLauncherName, "WSL distro launcher under test.")
var distroName = flag.String("distro-name", defaultDistroName, "WSL distro instance registered for testing.")

// A testing wrapper with functions simplifying composing and running commands through os.Exec and checking their outputs.
type Tester struct {
	*testing.T
}

func checkTestEnvMinRequirements(t *testing.T) {
	rootDir := os.Getenv("GITHUB_WORKSPACE")
	if len(rootDir) == 0 {
		t.Fatalf("GITHUB_WORKSPACE not set. It should point to the launcher repo root directory.")
	}
}

// Returns an instance of Tester set for unregistering the distro instance on clean up.
func WslTester(t *testing.T) Tester {
	checkTestEnvMinRequirements(t)
	tester := Tester{t}
	tester.Cleanup(func() {
		// print debug logs
		if tester.Failed() {
			tester.Log("\n\n=== Server Debug Log ====")
			logContents := tester.AssertWslCommand("cat", serverLogPath)
			tester.Logf("%s", logContents)
			tester.Log("\n\n=== Client Debug Log ====")
			rootDir := os.Getenv("GITHUB_WORKSPACE")
			path := filepath.Join(rootDir, clientLogPath)
			clientLogContents, err := ioutil.ReadFile(path)
			if err != nil {
				tester.Fatalf("%s", err)
			}
			tester.Logf("%s", clientLogContents)
		}
		// unregister the instance.
		tester.AssertOsCommand("wsl.exe", "--unregister", *distroName)
	})
	return tester
}

// Runs 'wsl.exe -d distroName' command line supplied as arguments to the wsl.exe command.
// Asserts that the process succeeds and returns its combined output (stdout + stderr).
func (t *Tester) AssertWslCommand(args ...string) string {
	fullArgs := append([]string{"-d", *distroName}, args...)
	return t.AssertOsCommand("wsl.exe", fullArgs...)
}

// Runs the command line supplied as arguments to the configured distro launcher on powershell.
// Asserts that the process succeeds and returns its combined output (stdout + stderr).
func (t *Tester) AssertLauncherCommand(fullCommand string) string {
	shellCommand := []string{"-noninteractive", "-nologo", "-noprofile", "-command", *launcherName + " " + fullCommand}
	return t.AssertOsCommand("powershell.exe", shellCommand...)
}

// Runs the specified command and arguments by means of os.exec (no shell involved.)
// Asserts that the process succeeds and returns its combined output (stdout + stderr).
func (t *Tester) AssertOsCommand(name string, args ...string) string {
	cmd := exec.Command(name, args...)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("Failed to run the command: %s. Error %s", name, err)
	}
	return string(output[:])
}
