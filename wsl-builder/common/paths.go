package common

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
)

// GeneratedDir is the generated output directory for a given released
const GeneratedDir = "generated"

// GetPath returns the first directory to <subdirectory>. It goes up from
// current working directory.
func GetPath(subdirectory string) (metaDir string, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("couldn't find a %s directory: %v", subdirectory, err)
		}
	}()

	current, err := os.Getwd()
	if err != nil {
		return "", err
	}

	for current != "/" {
		if _, err := os.Stat(filepath.Join(current, subdirectory)); err == nil {
			return filepath.Join(current, subdirectory), nil
		}
		current = filepath.Dir(current)
	}

	return "", errors.New("nothing found until /")
}
