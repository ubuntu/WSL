package test_runner

import (
	"flag"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"github.com/ubuntu/wsl/e2e/constants"
)

const serverLogPath = "/var/log/installer/systemsetup-server-debug.log"
const clientLogPath = "ubuntu-desktop-installer/packages/ubuntu_wsl_setup/build/windows/runner/Debug/.ubuntu_wsl_setup.exe/ubuntu_wsl_setup.exe.log"

var launcherName = flag.String("launcher-name", constants.DefaultLauncherName, "WSL distro launcher under test.")
var distroName = flag.String("distro-name", constants.DefaultDistroName, "WSL distro instance registered for testing.")

// A testing wrapper with functions simplifying composing and running commands through os.Exec and checking their outputs.
type Tester struct {
	*testing.T
}

func checkTestEnvMinRequirements(t *testing.T) {
	rootDir := os.Getenv(constants.LauncherRepoEnvVar)
	if len(rootDir) == 0 {
		t.Fatalf("%s not set. It should point to the launcher repo root directory.", constants.LauncherRepoEnvVar)
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
			output, err := exec.Command("wsl.exe", "-d", *distroName, "cat", serverLogPath).CombinedOutput()
			if err == nil {
				tester.Logf("%s", output)
			} else {
				tester.Logf("Failed to retrieve server debug log: %s: %s", err, output)
			}
			tester.Log("\n\n=== Client Debug Log ====")
			rootDir := os.Getenv(constants.LauncherRepoEnvVar)
			path := filepath.Join(rootDir, clientLogPath)
			clientLogContents, err := ioutil.ReadFile(path)
			if err != nil {
				tester.Fatalf("Failed to retrieve client debug log: %s", err)
			} else {
				tester.Logf("%s", clientLogContents)
			}
		}
		// attempts to unregister the instance.
		cmd := exec.Command("wsl.exe", "--shutdown")
		_ = cmd.Run()
		cmd = exec.Command("wsl.exe", "--unregister", *distroName)
		_ = cmd.Run()
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
		argsStr := strings.Join(args, " ")
		t.Fatalf("Failed to run command:\n > %s %s\nError: %s", name, argsStr, err)
	}
	return string(output[:])
}
