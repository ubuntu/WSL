package main

import (
	"encoding/csv"
	"fmt"
	"os"
	"strings"
)

type wslReleaseInfo struct {
	WslID        string
	FullName     string
	BuildVersion string
	LauncherName string
	IconVersion  string
}

// ReleaseInfos returns all releases we care about from a csvPath.
func ReleaseInfos(csvPath string) (releasesInfo []wslReleaseInfo, err error) {
	releases, err := readCSV(csvPath)
	if err != nil {
		return nil, err
	}

	wslReleases := activeReleases(releases)
	releasesInfo = buildReleasesInfo(wslReleases)

	return releasesInfo, nil
}

// activeReleases extracts WSL supported releases from the releases content
// and returns a map of WslID to version string.
func activeReleases(releases [][]string) (wslReleases map[string]string) {
	wslReleases = make(map[string]string)
	var latestLTS, latestLTSReleasedDate string

	for _, release := range releases {
		// There is always a development release, LTS or not
		if release[4] == "Active Development" {
			wslReleases["Ubuntu-Preview"] = fmt.Sprintf("%s.0", release[0])
		}

		// We have one application per LTSes, starting with 18.04 LTSes and onwards.
		if release[7] == "False" || release[0] < "18.04" {
			continue
		}
		wslID := fmt.Sprintf("Ubuntu-%s-LTS", release[0])
		wslReleases[wslID] = fmt.Sprintf("%s.%s", release[0], release[1])

		// Pin latest supported LTS as the "Ubuntu" application
		if release[9] > latestLTSReleasedDate && release[4] == "Supported" {
			latestLTSReleasedDate = release[9]
			latestLTS = wslID
		}
	}
	wslReleases["Ubuntu"] = wslReleases[latestLTS]

	return wslReleases
}

// buildReleasesInfo returns a slice of release info object, ready to be used
// in templates.
func buildReleasesInfo(releases map[string]string) (releasesInfo []wslReleaseInfo) {
	// Ubuntu-Preview              Ubuntu (Preview)            2110.0             ubuntupreview             Preview
	// Ubuntu-20.04-LTS            Ubuntu 20.04.2 LTS          2004.2             ubuntu2004                20.04.2 LTS
	for wslID, version := range releases {
		var fullName, iconVersion string
		switch wslID {
		case "Ubuntu":
			fullName = "Ubuntu"
		case "Ubuntu-Preview":
			fullName = "Ubuntu (Preview)"
			iconVersion = "Preview"
		default:
			fullName = fmt.Sprintf("Ubuntu %s LTS", version)
			iconVersion = fmt.Sprintf("%s LTS", version)
		}

		releasesInfo = append(releasesInfo, wslReleaseInfo{
			WslID:    wslID,
			FullName: fullName,
			// buildVersion is the version with first . stripped out
			BuildVersion: strings.Replace(version, ".", "", 1),
			LauncherName: strings.TrimSuffix(strings.ReplaceAll(strings.ReplaceAll(strings.ToLower(wslID), "-", ""), ".", ""), "lts"),
			IconVersion:  iconVersion,
		})
	}

	return releasesInfo
}

// readCSV deserialized the CSV filed into a [][]string
func readCSV(name string) (releases [][]string, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't read CSV file: %v", err)
		}
	}()

	// Read CSV file
	f, err := os.Open(name)
	if err != nil {
		return nil, err
	}
	r := csv.NewReader(f)
	r.Comma = '\t'
	return r.ReadAll()
}
