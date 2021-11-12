/*
Package common groups utility routines and types commonly used in
'prepare-assets' and 'prepare-build' module main packages as part
of the WSL image and launcher build process.
*/
package common

import (
	"encoding/csv"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

// WslReleaseInfo models information about the Ubuntu releases to be used
// in the template files which are part of the WSL image and launcher build process.
type WslReleaseInfo struct {
	WslID          string   // uniquely identifies this distro in WSL context.
	FullName       string   // full name of the distro bing built.
	BuildVersion   string   // Ubuntu version we are building with the dot removed to be compatible with UWP version schema. 20.04 -> 2004
	LauncherName   string   // name of the executable WSL launcher.
	ShortVersion   string   // Ubuntu version without point release and LTS. For instance  20.04 -> 20.04. 20.04.5 -> 20.04
	ReleaseVersion string   // Version to show in some icon without point release. 20.04 -> 20.04 LTS. 20.04.5 -> 20.04 LTS
	ReservedNames  []string // list of reserved names in the store ("Ubuntu 20.04 LTS" for instance. One should match the UWP package name)

	CodeName    string // Ubuntu release code name (e.g. Focal Fossa, Impish Indri etc)
	ShouldBuild bool   //whether or not an image should be built for this release.
}

// ReleasesInfo returns all releases we care about from a csvPath.
func ReleasesInfo(csvPath string) (releasesInfo []WslReleaseInfo, err error) {
	releases, err := readCSV(csvPath)
	if err != nil {
		return nil, err
	}

	return buildWSLReleaseInfo(releases)
}

// buildWSLReleaseInfo extracts WSL supported releases from the releases content
// and returns a slice of WslReleaseInfo, ready to be used from templates.
func buildWSLReleaseInfo(releases [][]string) (wslReleases []WslReleaseInfo, err error) {
	var latestLTSReleasedDate string
	var ubuntuWSL WslReleaseInfo

	for _, release := range releases {
		minor, err := strconv.Atoi(release[1])
		if err != nil {
			return nil, fmt.Errorf("minor version is not an int: %v", err)
		}
		version := fmt.Sprintf("%s.%d", release[0], minor)
		buildVersion := strings.Replace(version, ".", "", 1)
		launcherName := fmt.Sprintf("ubuntu%s", strings.Replace(release[0], ".", "", 1))
		codeName := release[2]

		// There is always a development release, LTS or not
		if release[4] == "Active Development" || release[4] == "Pre-release Freeze" {
			wslID := "UbuntuPreview"
			fullName := "Ubuntu (Preview)"
			wslReleases = append(wslReleases, WslReleaseInfo{
				WslID:          wslID,
				FullName:       fullName,
				BuildVersion:   buildVersion,
				LauncherName:   "ubuntupreview",
				ShortVersion:   release[0],
				ReleaseVersion: "Preview",
				ReservedNames:  []string{fullName},

				CodeName:    codeName,
				ShouldBuild: true,
			})

		}

		// We have one application per LTSes, starting with 18.04 LTSes and onwards.
		if release[7] == "False" || release[0] < "18.04" {
			continue
		}

		// Follow WSL build release schedule.
		var shouldBuild bool
		releaseDate, err := time.Parse("2006-01-02", release[8])
		if err != nil {
			return nil, fmt.Errorf("wrong release date for %s: %v", codeName, err)
		}
		if withinAWeekOf(releaseDate) {
			shouldBuild = true
		} else if release[11] != "" {
			nextPointReleaseDate, err := time.Parse("2006-01-02", release[11])
			if err != nil {
				return nil, fmt.Errorf("wrong next point release date for %s: %v", codeName, err)
			}
			if withinAWeekOf(nextPointReleaseDate) {
				shouldBuild = true
				// We need to +1 the minor release as we are close to next point release
				minor++
				version = fmt.Sprintf("%s.%d", release[0], minor)
				buildVersion = strings.Replace(version, ".", "", 1)
			}
		}

		// we don’t want to display in FullName or ReleaseVersion the .0 suffix. BuildVersion stays as it.
		version = strings.TrimSuffix(version, ".0")

		wslID := fmt.Sprintf("Ubuntu%sLTS", release[0])
		// Reserve .pointReleases elements
		reservedNames := []string{fmt.Sprintf("Ubuntu %s LTS", release[0])}
		for i := 0; i < 10; i++ {
			reservedNames = append(reservedNames, fmt.Sprintf("Ubuntu %s.%d LTS", release[0], i))
		}

		// Add per-release application
		wsl := WslReleaseInfo{
			WslID:          wslID,
			FullName:       fmt.Sprintf("Ubuntu %s LTS", version),
			BuildVersion:   buildVersion,
			LauncherName:   launcherName,
			ShortVersion:   release[0],
			ReleaseVersion: fmt.Sprintf("%s LTS", release[0]),
			ReservedNames:  reservedNames,

			CodeName:    codeName,
			ShouldBuild: shouldBuild,
		}
		wslReleases = append(wslReleases, wsl)

		// Pin latest supported (ie non in Active Development) LTS as the "Ubuntu" application
		if release[9] > latestLTSReleasedDate && release[4] == "Supported" {
			latestLTSReleasedDate = release[9]
			ubuntuWSL = wsl
		}
	}

	// Select Ubuntu release		wslID := fmt.Sprintf("Ubuntu%sLTS", release[0])
	ubuntuWSL.WslID = "Ubuntu"
	ubuntuWSL.FullName = "Ubuntu"
	ubuntuWSL.LauncherName = "ubuntu"
	ubuntuWSL.ReleaseVersion = ""
	ubuntuWSL.ReservedNames = []string{ubuntuWSL.FullName}
	wslReleases = append(wslReleases, ubuntuWSL)

	return wslReleases, nil
}

/*
ver.	m.	code	full version	Status				Activ.	Sup.	LTS		Opened			Release			Milestone
21.10	0	impish	Ubuntu 21.10	Active Development	True	False	False	2021-04-23
21.04	0	hirsute	Ubuntu 21.04	Current Stable Release	True	True	False	2020-10-23	2021-04-22
20.10	0	groovy	Ubuntu 20.10	Supported	True	True	False	2020-04-24	2020-10-22
20.04	2	focal	Ubuntu 20.04.2 LTS	Supported	True	True	True	2019-02-18	2020-04-23	2021-02-11
18.04	5	bionic	Ubuntu 18.04.5 LTS	Supported	True	True	True	2017-10-04	2018-04-26	2020-08-06
16.04	6	xenial	Ubuntu 16.04.6 LTS	Supported	True	True	True	2015-10-19	2016-04-21	2019-02-28
14.04	6	trusty	Ubuntu 14.04.6 LTS	Supported	True	True	True	2012-10-01	2014-04-17	2019-03-07

WSL_ID                      FULLNAME                    BUILD_VERSION      LAUNCHER_NAME             ICON_VERSION                BUILD_ID (locally stored, incremental)
Ubuntu                      Ubuntu                      2004.2             ubuntu                    ""
UbuntuPreview               Ubuntu (Preview)            2110.0             ubuntupreview             Preview
Ubuntu20.04LTS              Ubuntu 20.04.2 LTS          2004.2             ubuntu2004                20.04.2 LTS
Ubuntu18.04LTS              Ubuntu 18.04.5 LTS          1804.5             ubuntu1804                18.04.5 LTS
*/

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

// withinAWeekOf returns if now is withing a week of date.
func withinAWeekOf(date time.Time) bool {
	now := time.Now()
	if now.After(date.Add(-time.Duration(time.Hour*24*7))) && now.Before(date.Add(time.Duration(time.Hour*24))) {
		return true
	}
	return false
}
