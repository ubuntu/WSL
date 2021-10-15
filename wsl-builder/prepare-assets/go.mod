module github.com/ubuntu/WSL-DistroLauncher/wsl-builder/prepare-assets

go 1.16

require (
	github.com/spf13/cobra v1.1.3
	github.com/termie/go-shutil v0.0.0-20140729215957-bcacb06fecae
	github.com/ubuntu/WSL-DsitroLauncher/wsl-builder/common v0.0.0-00010101000000-000000000000
	gopkg.in/gographics/imagick.v2 v2.6.0
)

// We always want to use the latest local version.
replace github.com/ubuntu/WSL-DsitroLauncher/wsl-builder/common => ../common
