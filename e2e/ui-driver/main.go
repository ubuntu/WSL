package main

import (
	"bufio"
	"flag"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type DartTest struct {
	file string
	args []string
}

func testFileFromCommandLine() DartTest {
	var isReconfig = flag.Bool("reconfigure", false, "Whether setup or reconfig")
	var distroName = flag.String("distro-name", "Ubuntu-Preview", "The WSL distro name")
	var prefill = flag.String("prefill", "", "Whether prefill info is set or not")
	// this will always be true for e2e testing.
	_ = flag.Bool("no-dry-run", true, "Whether live or dry-run")

	flag.Parse()

	log.Printf("UI Driver started with the command line: %v", os.Args)

	args := []string{}

	if len(*distroName) > 0 {
		args = []string{"--dart-define", "DISTRONAME=" + strings.Trim(*distroName, "\"")}
	}

	if len(*prefill) > 0 {
		return DartTest{"setup_with_prefill_test.dart", append(args, "--dart-define", "PREFILL="+strings.Trim(*prefill, "\""))}
	}

	if *isReconfig {
		return DartTest{"reconfigure_test.dart", args}
	}

	return DartTest{"setup_test.dart", args}
}

func main() {
	const e2eBaseDir = "integration_test/e2e/"
	rootDir := os.Getenv("GITHUB_WORKSPACE")
	testDriver := testFileFromCommandLine()
	args := append([]string{"test", filepath.Join(e2eBaseDir, testDriver.file)}, testDriver.args...)
	cmd := exec.Command("flutter", args...)
	cmd.Dir = filepath.Join(rootDir, "/ubuntu-desktop-installer/packages/ubuntu_wsl_setup/")
	stdin, err := cmd.StdinPipe()
	if err != nil {
		log.Println(err)
	}
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	scanner := bufio.NewScanner(os.Stdin)
	go func() {
		defer stdin.Close()
		for scanner.Scan() {
			io.Copy(stdin, os.Stdin)
		}
	}()

	log.Println("START")
	if err := cmd.Start(); err != nil {
		log.Println("An error occured: ", err)

	}

	err = cmd.Wait()
	log.Println("Command was: flutter ", args)
	log.Println("END")

	if err != nil {
		log.Fatalln(err)
	}
}
