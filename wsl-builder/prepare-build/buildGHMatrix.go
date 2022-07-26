package main

import (
	"encoding/json"
	"fmt"
	"path/filepath"
	"strings"

	"github.com/ubuntu/wsl/wsl-builder/common"
)

type matrixElem struct {
	AppID            string
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
			// Currently only Kinetic (22.10) and later are published to "https://cloud-images.ubuntu.com/wsl/"
			codeNameSubUri := r.CodeName
			imageBaseName := fmt.Sprintf("%s-server-cloudimg", r.CodeName)
			if strings.Compare(r.BuildVersion, "2210") >= 0 {
				codeNameSubUri = filepath.Join("wsl", r.CodeName)
				// The image base name scheme also changed.
				imageBaseName = fmt.Sprintf("ubuntu-%s-wsl", r.CodeName)
			}

			t += fmt.Sprintf("https://cloud-images.ubuntu.com/%s/current/%s-%s-wsl.rootfs.tar.gz::%s", codeNameSubUri, imageBaseName, arch, arch)
			rootfses += t
		}

		allBuilds = append(allBuilds, matrixElem{
			AppID:            r.AppID,
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
