package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"fmt"
	"io"
	"io/fs"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	shutil "github.com/termie/go-shutil"
	"golang.org/x/sync/errgroup"
)

var (
	linuxToWindowsArch = map[string]string{
		"amd64": "x64",
		"arm64": "ARM64",
		"x64":   "x64",
		"ARM64": "ARM64",
	}
)

func prepareBuild(artifactsPath, wslID, rootfses string, noChecksum bool) error {
	rootPath, err := getPathWith("DistroLauncher-Appx")
	if err != nil {
		return err
	}
	rootPath = filepath.Dir(rootPath)

	buildNumber, err := extractAndStoreNextBuildNumber(artifactsPath)
	if err != nil {
		return err
	}

	archs, err := downloadRootfses(rootPath, rootfses, noChecksum)
	if err != nil {
		return err
	}

	if err := prepareAssets(rootPath, wslID, buildNumber, archs); err != nil {
		return err
	}

	return nil
}

// extractAndStoreNextBuildNumber returns the next minor build number to use
func extractAndStoreNextBuildNumber(artifactsPath string) (buildNumber string, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't update build number: %v", err)
		}
	}()

	buildNumber = "0"
	if d, err := os.ReadFile(filepath.Join(artifactsPath, "buildID")); err == nil {
		num, err := strconv.Atoi(string(d))
		if err != nil {
			return "", fmt.Errorf("invalid previous build number: %v", err)
		}
		buildNumber = fmt.Sprintf("%d", num+1)
	}
	artifactsPath = fmt.Sprintf("%s-new", filepath.Clean(artifactsPath))
	if err := os.MkdirAll(artifactsPath, 0755); err != nil {
		return "", err
	}
	if err := os.WriteFile(filepath.Join(artifactsPath, "buildID"), []byte(buildNumber), 0644); err != nil {
		return "", err
	}

	return buildNumber, nil
}

// downloadRootfses and returns a list of windows archs we will build on
func downloadRootfses(rootPath string, rootfses string, noChecksum bool) ([]string, error) {
	requestedArches := make(map[string]struct{})

	var g errgroup.Group
	for _, rootfs := range strings.Split(rootfses, ",") {
		e := strings.Split(rootfs, "::")
		url := e[0]

		// Register arch
		var arch string
		switch len(e) {
		case 1:
			// Autodetect arch from url
			for a := range linuxToWindowsArch {
				if !strings.Contains(url, a) {
					continue
				}
				arch = a
				break
			}
		case 2:
			arch = e[1]
		default:
			return nil, fmt.Errorf("invalid url/rootfs form. Only one :: separator to arch is allowded. Got: %q", rootfs)
		}
		winArch, ok := linuxToWindowsArch[arch]
		if !ok {
			return nil, fmt.Errorf("arch %q not supported in WSL (no Windows equivalent)", arch)
		}
		requestedArches[winArch] = struct{}{}

		// Download rootfs and checksum it
		g.Go(func() error {
			if err := os.MkdirAll(winArch, 0755); err != nil {
				return err
			}
			if err := downloadFile(url, filepath.Join(rootPath, winArch, "install.tar.gz")); err != nil {
				return err
			}

			if noChecksum {
				return nil
			}

			checksumURL := filepath.Join(filepath.Dir(winArch), "SHA256SUMS")
			if err := downloadFile(checksumURL, filepath.Join(rootPath, winArch, "SHA256SUMS")); err != nil {
				return err
			}
			if err := checksumMatches(filepath.Join(rootPath, winArch, "install.tar.gz"), filepath.Base(url), checksumURL); err != nil {
				return err
			}
			return nil
		})
	}

	var arches []string
	for a := range requestedArches {
		arches = append(arches, a)
	}

	return arches, g.Wait()
}

// downloadFile into dest
func downloadFile(url string, dest string) (err error) {
	log.Printf("downloading file %s", url)
	defer func() {
		if err != nil {
			err = fmt.Errorf("could not download %q: %v", url, err)
		}
	}()

	// Get the data
	var netClient = &http.Client{
		Timeout: 10 * time.Minute,
	}
	resp, err := netClient.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		return fmt.Errorf("http request failed with code %d", resp.StatusCode)
	}

	out, err := os.Create(dest)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, resp.Body)
	return err
}

// checksumMatches checks that file to match
func checksumMatches(path, origName, checksumPath string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("error checking checksum for: %q", path)
		}
	}()

	// Checksum of the rootfs
	f, err := os.Open(path)
	if err != nil {
		return err
	}
	defer f.Close()

	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return err
	}
	gotChecksum := fmt.Sprintf("%x", h.Sum(nil))

	// Load checksum file
	checksumF, err := os.Open(checksumPath)
	if err != nil {
		return err
	}
	defer checksumF.Close()
	scanner := bufio.NewScanner(checksumF)
	scanner.Split(bufio.ScanLines)
	var text []string

	for scanner.Scan() {
		text = append(text, scanner.Text())
	}

	var found bool
	for _, l := range text {
		e := strings.Split(l, " ")
		if len(e) != 2 {
			continue
		}

		e[1] = strings.TrimSuffix(e[1], "*")
		if e[1] != origName {
			continue
		}

		found = true
		if e[0] != gotChecksum {
			return fmt.Errorf("checksum don’t match: expected %q but got %q", e[0], gotChecksum)
		}
	}
	if !found {
		return fmt.Errorf("couldn't find %q in checksum file", origName)
	}
	return nil
}

// prepareAssets copies metadata and assets files, appending dynamic elements
func prepareAssets(rootPath, wslID, buildNumber string, arches []string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("could not prepare assets and metadata: %v", err)
		}
	}()

	// Copy content from meta/
	rootDir := filepath.Join(rootPath, "meta", wslID)
	if err := filepath.WalkDir(rootDir, func(path string, de fs.DirEntry, err error) error {
		if de.IsDir() {
			return nil
		}

		relPath := strings.TrimPrefix(path, fmt.Sprintf("%s%c", rootDir, os.PathSeparator))

		if err := shutil.CopyFile(path, relPath, false); err != nil {
			return fmt.Errorf("copy %q to %q failed: %v", path, relPath, err)
		}

		return nil
	}); err != nil {
		return err
	}

	// Prepare appxmanifest
	appxManifest := filepath.Join("DistroLauncher-Appx", "MyDistro.appxmanifest")
	for _, arch := range arches {
		d, err := os.ReadFile(appxManifest)
		if err != nil {
			return fmt.Errorf("failed to read %q: %v", appxManifest, err)
		}
		d = bytes.ReplaceAll(d, []byte("[[BUILD_ID]]"), []byte(buildNumber))
		d = bytes.ReplaceAll(d, []byte("[[WIN_ARCH]]"), []byte(strings.ToLower(arch)))

		destPath := filepath.Join(rootPath, arch, filepath.Base(appxManifest))
		if err := os.WriteFile(destPath, d, 0644); err != nil {
			return fmt.Errorf("failed to write %q: %v", destPath, err)
		}
	}

	// Print for github env variable
	archString := strings.Join(arches, "|")
	fmt.Println(archString)

	return nil
}
