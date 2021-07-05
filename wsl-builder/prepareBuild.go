package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/termie/go-shutil"
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
	buildNumber, err := extractAndStoreNextBuildNumber(artifactsPath)
	if err != nil {
		return err
	}

	archs, err := downloadRootfses(rootfses, noChecksum)
	if err != nil {
		return err
	}

	if err := prepareAssets(wslID, buildNumber, archs); err != nil {
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
func downloadRootfses(rootfses string, noChecksum bool) ([]string, error) {
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
			for _, a := range linuxToWindowsArch {
				if !strings.Contains(url, a) {
					continue
				}
				arch = a
			}
		case 2:
			arch = e[1]
		default:
			return nil, fmt.Errorf("invalid url/rootfs form. Only one :: separator to arch is allowded. Got: %q", rootfs)
		}
		winArch, ok := linuxToWindowsArch[arch]
		if !ok {
			return nil, fmt.Errorf("arch %q not supported in WSL (no Windows equivalent)", winArch)
		}
		requestedArches[winArch] = struct{}{}

		// Download rootfs and checksum it
		g.Go(func() error {
			if err := os.MkdirAll(winArch, 0755); err != nil {
				return err
			}
			if err := downloadFile(url, filepath.Join(winArch, "install.tar.gz")); err != nil {
				return err
			}

			if noChecksum {
				return nil
			}

			checksumURL := filepath.Join(filepath.Dir(winArch), "SHA256SUMS")
			if err := downloadFile(checksumURL, filepath.Join(winArch, "SHA256SUMS")); err != nil {
				return err
			}
			if err := checksumMatches(filepath.Join(winArch, "install.tar.gz"), filepath.Base(url), checksumURL); err != nil {
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
func downloadFile(url string, dest string) error {
	// Get the data
	var netClient = &http.Client{
		Timeout: 2 * time.Minute,
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
func prepareAssets(wslID, buildNumber string, arches []string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("could not prepare assets and metadata: %v", err)
		}
	}()

	archString := strings.Join(arches, "|")

	rootDir := filepath.Join("meta", wslID)
	if err := filepath.WalkDir(rootDir, func(path string, de fs.DirEntry, err error) error {
		relPath := strings.TrimPrefix(path, rootDir)

		if err := shutil.CopyFile(path, relPath, false); err != nil {
			return err
		}

		if filepath.Ext(path) == "appxmanifest" {
			d, err := os.ReadFile(path)
			if err != nil {
				return err
			}
			d = bytes.ReplaceAll(d, []byte("[[BUILD_ID]]"), []byte(buildNumber))
			d = bytes.ReplaceAll(d, []byte("[[WIN_ARCH]]"), []byte(archString))

			if err := os.WriteFile(path, d, de.Type().Perm()); err != nil {
				return err
			}
		}

		return nil
	}); err != nil {
		return err
	}

	return nil
}
