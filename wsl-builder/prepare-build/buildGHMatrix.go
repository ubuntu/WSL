package main

import (
	"encoding/json"
	"fmt"
)

type matrixElem struct {
	WslID    string
	Rootfses string
	Upload   string
}

// buildGHMatrix computes the list of distro that needs to start a build request.
func buildGHMatrix(csvPath string) error {
	releasesInfo, err := ReleasesInfo(csvPath)
	if err != nil {
		return err
	}

	var allBuilds []matrixElem
	// List which releases we should try building
	for _, r := range releasesInfo {
		if !r.shouldBuild {
			continue
		}

		var rootfses string
		for i, arch := range []string{"amd64", "arm64"} {
			t := ","
			if i == 0 {
				t = ""
			}
			t += fmt.Sprintf("https://cloud-images.ubuntu.com/%s/current/%s-server-cloudimg-%s-wsl.rootfs.tar.gz::%s", r.codeName, r.codeName, arch, arch)
			rootfses += t
		}

		allBuilds = append(allBuilds, matrixElem{
			WslID:    r.WslID,
			Rootfses: rootfses,
			Upload:   "yes",
		})
	}

	d, err := json.Marshal(allBuilds)
	if err != nil {
		return err
	}
	fmt.Println(string(d))
	return nil
}
