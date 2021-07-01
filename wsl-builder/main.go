package main

import (
	"errors"
	"log"

	"github.com/spf13/cobra"
)

/*
   release-template creates a new release content and assets if we have a new release or version

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
Ubuntu-Preview              Ubuntu (Preview)            2110.0             ubuntupreview             Preview
Ubuntu-20.04-LTS            Ubuntu 20.04.2 LTS          2004.2             ubuntu2004                20.04.2 LTS
Ubuntu-18.04-LTS            Ubuntu 18.04.5 LTS          1804.5             ubuntu1804                18.04.5 LTS

*/

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

	updateCmd := &cobra.Command{
		Use:   "update CSV_FILE",
		Short: "Update all releases in WSL distribution info and assets from template",
		Long: `This tool update all releases from an incoming CSV file and generate
		       every icons and xml files needed to build a WSL application`,
		RunE: func(cmd *cobra.Command, args []string) error {
			if len(args) != 1 {
				return errors.New("this command accepts exactly one CSV file")
			}

			return updateReleases(args[0])
		},
	}

	rootCmd.AddCommand(updateCmd)

	err := rootCmd.Execute()
	if err != nil {
		log.Fatal(err)
	}
}
