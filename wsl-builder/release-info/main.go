package main

import (
	"errors"
	"os"

	"github.com/0xrawsec/golang-utils/log"
	"github.com/spf13/cobra"
)

func main() {
	var f columns
	rootCmd := &cobra.Command{
		Use:   "release-info CSV_FILE [DISTRO...] [--app-id] [--full-name] [--code-name] [--launcher] [--rootfs Arch] [--short]",
		Short: "Print informantion regarding a release",
		Long: `This tool shows information about any WSL release. Write no distro to see all of them.
Fields are always in the same order, regardless of the order the flags are passed in.`,
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) == 0 {
				return errors.New("this command needs at least one argument (CSV_FILE)")
			}
			cmd.SilenceUsage = true
			return writeReleaseInfo(args[0], args[1:], f) // Returning an error causes Cobra to print its help
		},
	}

	f.appId = rootCmd.Flags().Bool("app-id", false, "Display the App ID as shown in the store.")
	f.fullName = rootCmd.Flags().Bool("full-name", false, "Display the App FullName as shown in your machine.")
	f.launcher = rootCmd.Flags().Bool("launcher", false, "Display the launcher executable name.")
	f.codeName = rootCmd.Flags().Bool("code-name", false, "Display the code name, i.e. the distro's adjective.")
	f.rootfsArch = rootCmd.Flags().String("rootfs", "", "Display the URL where the rootfs can be downloaded.")
	f.short = rootCmd.Flags().BoolP("short", "s", false, "Don't display the table header.")

	err := rootCmd.Execute()
	if err != nil {
		log.Error(err)
		os.Exit(1)
	}
}
