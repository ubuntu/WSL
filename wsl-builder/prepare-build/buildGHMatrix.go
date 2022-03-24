package main

import (
	"encoding/json"
	"fmt"

	"github.com/ubuntu/wsl/wsl-builder/common"
)

type matrixElem struct {
	WslID            string
	Rootfses         string
	RootfsesChecksum string
	Upload           string
}

// buildGHMatrix computes the list of distro that needs to start a build request.
func buildGHMatrix(csvPath, metaPath string) error {
	releasesInfo, err := common.ReleasesInfo(csvPath)
	if err != nil {
		return err
	}

	var allBuilds []matrixElem
	// List which releases we should try building
	for _, r := range releasesInfo {
		if !r.ShouldBuild {
			continue
		}

		var rootfses string
		for i, arch := range []string{"amd64", "arm64"} {
			t := ","
			if i == 0 {
				t = ""
			}
			t += fmt.Sprintf("https://cloud-images.ubuntu.com/%s/current/%s-server-cloudimg-%s-wsl.rootfs.tar.gz::%s", r.CodeName, r.CodeName, arch, arch)
			rootfses += t
		}

		allBuilds = append(allBuilds, matrixElem{
			WslID:            r.WslID,
			Rootfses:         rootfses,
			RootfsesChecksum: "yes",
			Upload:           "yes",
		})
	}

	d, err := json.Marshal(allBuilds)
	if err != nil {
		return err
	}
	fmt.Println(string(d))
	return nil
}
