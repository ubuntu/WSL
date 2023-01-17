package main

import (
	"encoding/json"
	"fmt"
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

		rootfsUrls := []string{}
		for _, arch := range []string{"amd64", "arm64"} {
			urlAndArch := fmt.Sprintf("%s::%s", r.RootfsURL(arch), arch)
			rootfsUrls = append(rootfsUrls, urlAndArch)
		}

		allBuilds = append(allBuilds, matrixElem{
			AppID:            r.AppID,
			Rootfses:         strings.Join(rootfsUrls, ","),
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
