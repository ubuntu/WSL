package main

import (
	"bufio"
	"bytes"
	"crypto/sha256"
	"fmt"
	"io"
	"io/fs"
	"log"
	"math"
	"net"
	"net/http"
	"net/url"
	"os"
	"path"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	shutil "github.com/termie/go-shutil"
	"github.com/ubuntu/wsl/wsl-builder/common"
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

// prepareBuild finds the correct paths of the VS projects, prepare build assets and get rootfs images.
func prepareBuild(buildIDPath, appID, rootfses string, noChecksum bool, buildID int) error {
	metaPath, err := common.GetPath("meta")
	if err != nil {
		return err
	}
	rootPath := filepath.Dir(metaPath)

	var buildNumber string
	if buildID < 0 {
		if buildNumber, err = extractAndStoreNextBuildNumber(buildIDPath); err != nil {
			return err
		}
	} else {
		// force a manual build number
		buildNumber = fmt.Sprintf("%d", buildID)
	}

	archs, err := getRootfses(rootPath, rootfses, noChecksum)
	if err != nil {
		return err
	}

	if err := prepareAssets(rootPath, appID, buildNumber, archs); err != nil {
		return err
	}

	return nil
}

// extractAndStoreNextBuildNumber returns the next minor build number to use.
func extractAndStoreNextBuildNumber(buildIDPath string) (buildNumber string, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't update build number: %v", err)
		}
	}()

	buildNumber = "0"
	if d, err := os.ReadFile(buildIDPath); err == nil {
		num, err := strconv.Atoi(string(d))
		if err != nil {
			return "", fmt.Errorf("invalid previous build number: %v", err)
		}
		buildNumber = fmt.Sprintf("%d", num+1)
	}

	if err := os.MkdirAll(filepath.Dir(buildIDPath), 0755); err != nil {
		return "", err
	}
	if err := os.WriteFile(buildIDPath, []byte(buildNumber), 0644); err != nil {
		return "", err
	}

	return buildNumber, nil
}

// getRootfs downloads one rootfs file in tar.gz format and place
// it where the distro launcher build system expects. If `uri` points to
// a local regular file, it is copied from disk instead of downloaded.
func getRootfs(uri, rootPath, winArch string, noChecksum bool) error {
	if err := os.MkdirAll(winArch, 0755); err != nil {
		return err
	}

	if isLocalFile(uri) {
		if !noChecksum {
			log.Printf("Checksum not supported for local URI")
		}
		return copyLocalFile(uri, filepath.Join(rootPath, winArch, "install.tar.gz"))
	}

	if err := downloadFile(uri, filepath.Join(rootPath, winArch, "install.tar.gz")); err != nil {
		return err
	}

	if noChecksum {
		return nil
	}

	u, err := url.Parse(uri)
	if err != nil {
		return err
	}
	u.Path = filepath.Join(path.Dir(u.Path), "SHA256SUMS")
	checksumURL := strings.ReplaceAll(u.String(), "%5C", "/")
	checksumDest := filepath.Join(rootPath, winArch, "SHA256SUMS")
	if err := downloadFile(checksumURL, checksumDest); err != nil {
		return err
	}
	if err := checksumMatches(filepath.Join(rootPath, winArch, "install.tar.gz"), filepath.Base(uri), checksumDest); err != nil {
		return err
	}
	return nil
}

// getRootfses returns a list of windows archs we will build on
// and place rootfses into the path expected by the WSL build process for each arch.
func getRootfses(rootPath, rootfses string, noChecksum bool) ([]string, error) {
	requestedArches := make(map[string]struct{})

	var g errgroup.Group
	for _, rootfs := range strings.Split(rootfses, ",") {
		e := strings.Split(rootfs, "::")
		rootfsURL := e[0]

		// Register arch
		var arch string
		switch len(e) {
		case 1:
			// Autodetect arch from url
			for a := range linuxToWindowsArch {
				if !strings.Contains(rootfsURL, a) {
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

		// Obtains rootfs and checksum it if `noChecksum==false`
		g.Go(func() error {
			return getRootfs(rootfsURL, rootPath, winArch, noChecksum)
		})
	}

	var arches []string
	for a := range requestedArches {
		arches = append(arches, a)
	}

	return arches, g.Wait()
}

// isLocalFile returns true if `url` points to a regular local file.
func isLocalFile(url string) bool {
	sourceFileStat, err := os.Stat(url)
	if err != nil {
		return false
	}

	if !sourceFileStat.Mode().IsRegular() {
		log.Printf("%s is not a regular file", url)
		return false
	}
	return true
}

// copyLocalFile copies a regular local file pointed by `url`
// into a new file created at `dest`.
func copyLocalFile(url, dest string) (err error) {
	log.Printf("copying file %s", url)
	source, err := os.Open(url)
	if err != nil {
		return err
	}
	defer source.Close()

	var size uint64
	fi, err := source.Stat()
	if err != nil {
		log.Printf("Warning: unknown size for %s: %v", url, err)
	} else {
		size = uint64(fi.Size())
	}

	return writeContentInto(source, size, dest)
}

// downloadFile downloads a file from the address pointed by `url` into `dest`.
func downloadFile(url, dest string) (err error) {
	log.Printf("downloading file %s", url)
	defer func() {
		if err != nil {
			err = fmt.Errorf("could not download %q: %v", url, err)
		}
	}()

	// Get the data
	var netTransport = &http.Transport{
		Dial: (&net.Dialer{
			Timeout: 5 * time.Second,
		}).Dial,
		TLSHandshakeTimeout: 5 * time.Second,
	}
	var netClient = &http.Client{
		Timeout:   30 * time.Minute,
		Transport: netTransport,
	}
	resp, err := netClient.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode >= 400 {
		return fmt.Errorf("http request failed with code %d", resp.StatusCode)
	}

	var size int
	if size, err = strconv.Atoi(resp.Header.Get("Content-Length")); err != nil {
		log.Printf("Warning: unknown size for %s: %v", url, err)
	}
	return writeContentInto(resp.Body, uint64(size), dest)
}

type writeCounter struct {
	f               *os.File
	name            string
	current         uint64
	previousPrinted uint64
	total           uint64
}

func (wc *writeCounter) Write(p []byte) (int, error) {
	n := len(p)
	wc.current += uint64(n)
	if wc.current < wc.previousPrinted+10*(1<<20) {
		return wc.f.Write(p)
	}
	log.Printf("%s: %.0f MB / %.0f MB\n", wc.name,
		math.Floor(float64(wc.current)/(1<<20)),
		math.Floor(float64(wc.total)/(1<<20)))
	wc.previousPrinted = wc.current
	return wc.f.Write(p)
}

func (wc *writeCounter) Close() error {
	return wc.f.Close()
}

// writeContentInto writes the content of the `source io.Reader`
// into a new file created at `dest`.
// total is the total size of the content to be downloaded.
func writeContentInto(source io.Reader, total uint64, dest string) (err error) {
	out, err := os.Create(dest)
	if err != nil {
		return err
	}
	wc := &writeCounter{
		f:     out,
		name:  filepath.Join(filepath.Base(filepath.Dir(dest)), filepath.Base(dest)),
		total: total,
	}
	defer wc.Close()

	_, err = io.Copy(wc, source)
	return err
}

// checksumMatches checks the file pointed by `path` against values
// provided in the `checksumPath` file.
func checksumMatches(path, origName, checksumPath string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("error checking checksum for: %q: %v", path, err)
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

		e[1] = strings.TrimPrefix(e[1], "*")
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

// prepareAssets copies metadata and assets files, appending dynamic elements.
func prepareAssets(rootPath, wslID, buildNumber string, arches []string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("could not prepare assets and metadata: %v", err)
		}
	}()

	// Copy content from meta/
	rootDir := filepath.Join(rootPath, "meta", wslID, common.GeneratedDir)
	if err := filepath.WalkDir(rootDir, func(path string, de fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if de.IsDir() {
			return nil
		}

		relPath := strings.TrimPrefix(path, fmt.Sprintf("%s%c", rootDir, os.PathSeparator))

		if err := os.MkdirAll(filepath.Dir(relPath), 0755); err != nil {
			return fmt.Errorf("creating parent dir for %q failed: %v", relPath, err)
		}

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
		destPath := appxManifest
		if arch != "x64" {
			destPath = filepath.Join(rootPath, arch, filepath.Base(appxManifest))
		}

		d, err := os.ReadFile(appxManifest)
		if err != nil {
			return fmt.Errorf("failed to read %q: %v", appxManifest, err)
		}
		d = bytes.ReplaceAll(d, []byte(".42."), []byte(buildNumber))
		d = bytes.ReplaceAll(d, []byte("x64"), []byte(strings.ToLower(arch)))
		if err := os.WriteFile(destPath, d, 0644); err != nil {
			return fmt.Errorf("failed to write %q: %v", destPath, err)
		}
	}

	// Print for github env variable
	archString := strings.Join(arches, "|")
	fmt.Println(archString)

	return nil
}
