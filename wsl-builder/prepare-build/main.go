package main

import (
	"errors"
	"log"

	"github.com/spf13/cobra"
	"github.com/ubuntu/WSL-DsitroLauncher/wsl-builder/common"
)

func main() {
	rootCmd := &cobra.Command{
		Use:   "prepare COMMAND",
		Short: "Prepares the build tree before running msbuild",
		Long: `This tool downloads the assets and prepares the build tree before
		       running msbuild to build WSL application.`,
		RunE: func(cmd *cobra.Command, args []string) error {
			return nil
		},
	}

	buildGHMatrixCmd := &cobra.Command{
		Use:   "build-github-matrix CSV_FILE",
		Short: "Return a json list of all build combinations we support",
		Long: `This builds and returns the list of all desired build combinations
		       we should trigger on Github Actions, following the release schedule`,
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) != 1 {
				return errors.New("this command accepts exactly one CSV file")
			}

			metaPath, err := common.GetPathWith("meta")
			if err != nil {
				return err
			}
			return buildGHMatrix(args[0], metaPath)
		},
	}
	rootCmd.AddCommand(buildGHMatrixCmd)

	var noChecksum *bool
	var buildID *int
	prepareBuildCmd := &cobra.Command{
		Use:   "prepare BUILDID_PATH WSL_ID ROOTFSES",
		Short: "Prepares the build source before calling msbuild",
		Long: `This downloads the root file systems and prepares the build before
		       building the appx bundle`,
		Args: cobra.ExactArgs(3),
		RunE: func(cmd *cobra.Command, args []string) error {
			return prepareBuild(args[0], args[1], args[2], *noChecksum, *buildID)
		},
	}
	rootCmd.AddCommand(prepareBuildCmd)
	noChecksum = prepareBuildCmd.Flags().Bool("no-checksum", false, "Disable checksum verification on rootfses")
	buildID = prepareBuildCmd.Flags().Int("build-id", -1, "Force a build ID")

	err := rootCmd.Execute()
	if err != nil {
		log.Fatal(err)
	}
}
