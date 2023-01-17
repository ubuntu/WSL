package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"text/tabwriter"

	"github.com/ubuntu/wsl/wsl-builder/common"
)

type columns struct {
	rootfsArch                                         *string
	rootfs, appId, fullName, launcher, codeName, short *bool
}

func writeReleaseInfo(csvPath string, distros []string, cols columns) (exitCode int) {
	exitCode = 0

	releases, ok, err := selectReleases(csvPath, distros)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		return 1
	}
	if !ok {
		exitCode = 2
	}

	if ok := writeTable(releases, cols); !ok {
		exitCode = 2
	}

	return exitCode
}

// writeTable writes the information in a table into stdout.
// Issues are reported to stderr.
// Returns OK if there were no issues.
func writeTable(releases []common.WslReleaseInfo, cols columns) (ok bool) {
	w := tabwriter.NewWriter(os.Stdout, 1, 2, 2, ' ', 0)
	defer w.Flush()

	ok = true
	if e := writeHeader(w, cols); e != nil {
		ok = false
		fmt.Fprintf(os.Stderr, "Warning: could not write header: %v\n", e)
	}

	for _, r := range releases {
		if e := writeRow(w, r, cols); e != nil {
			ok = false
			fmt.Fprintf(os.Stderr, "Warning: could not write row for release %q: %v\n", r.AppID, e)
		}
	}

	return ok
}

func writeHeader(w io.Writer, f columns) error {
	if *f.short {
		return nil
	}

	text := []string{}
	text = append(text, "WslID")
	if *f.appId {
		text = append(text, "AppID")
	}
	if *f.fullName {
		text = append(text, "FullName")
	}
	if *f.launcher {
		text = append(text, "Launcher")
	}
	if *f.codeName {
		text = append(text, "CodeName")
	}
	if *f.rootfs {
		text = append(text, "Rootfs")
	}

	_, err := fmt.Fprintln(w, strings.Join(text, "\t"))
	return err
}

func writeRow(w io.Writer, r common.WslReleaseInfo, f columns) error {
	text := []string{}
	text = append(text, r.WslID)
	if *f.appId {
		text = append(text, r.AppID)
	}
	if *f.fullName {
		text = append(text, r.FullName)
	}
	if *f.codeName {
		text = append(text, r.CodeName)
	}
	if *f.launcher {
		text = append(text, r.LauncherName+".exe")
	}
	if *f.rootfs {
		text = append(text, common.RootfsUrl(r, *f.rootfsArch))
	}

	_, err := fmt.Fprintln(w, strings.Join(text, "\t"))
	return err
}

// selectReleases returns:
//  - releases: the intersection of releases in the CSV and in the list of targets.
//              If the list of targets is empty, all the releases in the CSV are returned.
//  - ok: whether all targets were found in the CSV
//  - err: Critical problems.
func selectReleases(csvPath string, targets []string) (releases []common.WslReleaseInfo, ok bool, err error) {
	releases, err = common.ReleasesInfo(csvPath)
	if err != nil {
		return releases, true, fmt.Errorf("could not read release info: %v", err)
	}

	end := len(releases)
	if len(targets) != 0 {
		end = partition(releases, func(r common.WslReleaseInfo) bool { return contains(targets, r.WslID) })
	}

	if end == 0 {
		return releases, false, errors.New("no releases were found")
	}

	if end < len(targets) {
		fmt.Fprintln(os.Stderr, "Warning: not all releases were found")
		return releases[:end], false, nil
	}

	return releases[:end], true, nil
}

// contains searches for a matching string and returns its index.
// If no string matches, -1 is returned.
func contains(arr []string, target string) bool {
	for _, a := range arr {
		if a == target {
			return true
		}
	}
	return false
}

// partition returns an index J and rearranges the slice such that all
// elements with index less than J fulfil the predicate.
func partition[T any](slice []T, predicate func(t T) bool) int {
	if len(slice) == 0 {
		return 0
	}
	var i int
	var j int
	if predicate(slice[0]) {
		j++
	}

	for i = 1; i < len(slice); i++ {
		if !predicate(slice[i]) {
			continue
		}
		if i == j {
			j++
			continue
		}
		slice[i], slice[j] = slice[j], slice[i]
		j++
	}
	return j
}
