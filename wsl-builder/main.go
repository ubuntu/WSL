package main

import (
	"errors"
	"log"

	"github.com/spf13/cobra"
)

func main() {
	rootCmd := &cobra.Command{
		Use:   "wsl-builder COMMAND",
		Short: "Update releases in WSL distribution info and assets from template",
		Long: `This tool update releases from an incoming CSV file and generate
		       every icons and xml files needed to build a WSL application`,
		RunE: func(cmd *cobra.Command, args []string) error {
			return nil
		},
	}

	assetsCmd := &cobra.Command{
		Use:   "assets CSV_FILE",
		Short: "Update all releases in WSL distribution info and assets from template",
		Long: `This tool update all releases from an incoming CSV file and generate
		       every icons and xml files needed to build a WSL application`,
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) != 1 {
				return errors.New("this command accepts exactly one CSV file")
			}

			return updateAssets(args[0])
		},
	}
	rootCmd.AddCommand(assetsCmd)

	buildGHMatrixCmd := &cobra.Command{
		Use:   "build-github-matrix CSV_FILE",
		Short: "Return a json list of all build combinations we support",
		Long: `This builds and returns the list of all desired build combinations
		       we should trigger on Github Actions, following the release schedule`,
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) != 1 {
				return errors.New("this command accepts exactly one CSV file")
			}

			return buildGHMatrix(args[0])
		},
	}
	rootCmd.AddCommand(buildGHMatrixCmd)

	var noChecksum *bool
	prepareBuildCmd := &cobra.Command{
		Use:   "prepare-build ARTIFACTS_PATH WSL_ID ROOTFSES",
		Short: "Prepares the build source before calling msbuild",
		Long: `This downloads the root file systems and prepares the build before
		       building the appx bundle`,
		Args: cobra.ExactArgs(3),
		RunE: func(cmd *cobra.Command, args []string) error {
			return prepareBuild(args[0], args[1], args[2], *noChecksum)
		},
	}
	rootCmd.AddCommand(prepareBuildCmd)
	noChecksum = prepareBuildCmd.Flags().Bool("no-checksum", false, "Disable checksum verification on rootfses")

	err := rootCmd.Execute()
	if err != nil {
		log.Fatal(err)
	}
}
