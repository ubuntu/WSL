package main

import (
	"bytes"
	"errors"
	"fmt"
	"image"
	_ "image/png"
	"io/fs"
	"io/ioutil"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"text/template"

	shutil "github.com/termie/go-shutil"
	"github.com/ubuntu/WSL/wsl-builder/common"
	"gopkg.in/gographics/imagick.v2/imagick"
)

// updateAssets orchestrates the distro launcher metadata and assets generation from a csv file path.
func updateAssets(csvPath string) error {
	imagick.Initialize()
	defer imagick.Terminate()

	metaPath, err := common.GetPath("meta")
	if err != nil {
		return err
	}
	rootPath := filepath.Dir(metaPath)

	releasesInfo, err := common.ReleasesInfo(csvPath)
	if err != nil {
		return err
	}

	if err != nil {
		return err
	}

	// Update each release
	for _, r := range releasesInfo {
		wslPath := filepath.Join(metaPath, r.WslID)
		if err := resetMetaDirectoryForRelease(wslPath); err != nil {
			return err
		}

		if err := generateMetaFilesForRelease(r, wslPath, rootPath); err != nil {
			return err
		}

		if err := generateAssetsForRelease(r, wslPath, metaPath, rootPath); err != nil {
			return err
		}
	}

	return nil
}

// resetMetaDirectoryForRelease recreate a vanilla meta directory, but keep any store screenshots
func resetMetaDirectoryForRelease(wslPath string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't generate reset meta directory %q: %v", wslPath, err)
		}
	}()

	oldWslPath := wslPath + ".old"
	if err := os.Rename(wslPath, oldWslPath); !errors.Is(err, fs.ErrNotExist) && err != nil {
		return err
	}

	if err := os.MkdirAll(wslPath, 0755); err != nil {
		return err
	}

	err = filepath.WalkDir(filepath.Join(oldWslPath, "store"), func(path string, d fs.DirEntry, err error) error {
		// "store" directory does not exists in oldWslPath
		if errors.Is(err, fs.ErrNotExist) {
			return nil
		}
		if err != nil {
			return err
		}

		if !strings.HasSuffix(path, ".png") || d.IsDir() {
			return nil
		}

		relPath := strings.TrimPrefix(path, oldWslPath+"/")

		destPath := filepath.Join(wslPath, relPath)
		if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
			return err
		}

		return os.Rename(path, destPath)
	})

	if err := os.RemoveAll(oldWslPath); err != nil {
		return err
	}

	return nil
}

type captionScreenshot struct {
	DisplayName string
	Path        string
}

type releaseInfoWithStoreScreenshots struct {
	common.WslReleaseInfo
	StoreScreenShots []captionScreenshot
}

// generateMetaFilesForRelease updates all metadata files from template for a given release.
func generateMetaFilesForRelease(r common.WslReleaseInfo, wslPath, rootPath string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't generate metadata file for %s: %v", r.WslID, err)
		}
	}()

	// Fetch all templated files and recreate them (and intermediate directories)
	err = filepath.WalkDir(rootPath, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		relPath := strings.TrimPrefix(path, rootPath+"/")
		if !strings.HasPrefix(relPath, "DistroLauncher/") && !strings.HasPrefix(relPath, "DistroLauncher-Appx/") && !strings.HasPrefix(relPath, "meta/store") {
			return nil
		}
		if d.IsDir() {
			return nil
		}
		// if the destination is the meta directory, take that as root
		relPath = strings.TrimPrefix(relPath, "meta/")

		destPath := filepath.Join(wslPath, relPath)
		if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
			return err
		}

		// if it’s a store screenshot .png file, just copy it.
		if filepath.Ext(path) == ".png" && strings.HasPrefix(relPath, "store") {
			return shutil.CopyFile(path, destPath, false)
		}

		// ensure we have template file to substitude data in
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		templateData := string(data)
		if filepath.Ext(path) == ".png" || !strings.Contains(templateData, "{{.") {
			return nil
		}

		t := template.Must(template.New("").Parse(templateData))

		f, err := os.Create(destPath)
		if err != nil {
			return nil
		}
		defer f.Close()

		// Extend our object with screenshots from store for store product description
		var templateMeta interface{} = r
		if filepath.Base(path) == "ProductDescription.xml" {
			rWithScrenshots := releaseInfoWithStoreScreenshots{WslReleaseInfo: r}

			// Collect corresponding original common screenshots for that release that we will put at the end of the description
			commonScreenshots, err := filterAndSortImagesInDir(filepath.Dir(path), nil)
			if err != nil {
				return err
			}

			// And now collect screenshots already copied in the dest store directory
			screenshots, err := filterAndSortImagesInDir(filepath.Dir(destPath), commonScreenshots)
			if err != nil {
				return err
			}
			// Append base sorted screenshots
			screenshots = append(screenshots, commonScreenshots...)

			for _, s := range screenshots {
				rWithScrenshots.StoreScreenShots = append(rWithScrenshots.StoreScreenShots, captionScreenshot{
					Path:        s,
					DisplayName: strings.TrimSuffix(strings.ReplaceAll(s, "_", " "), ".png"),
				})
			}

			templateMeta = rWithScrenshots
		}

		return t.Execute(f, templateMeta)
	})
	return err
}

// collectAndSortImagesInDir returns a sorted list of all images in a directory
func filterAndSortImagesInDir(path string, excludes []string) ([]string, error) {
	files, err := os.ReadDir(path)
	if err != nil {
		return nil, err
	}

	// Filter and sort them
	var r []string
nextFile:
	for _, f := range files {
		if !strings.HasSuffix(f.Name(), ".png") {
			continue
		}
		// Exclude base list
		for _, e := range excludes {
			if f.Name() == e {
				continue nextFile
			}
		}

		r = append(r, f.Name())
	}
	sort.Strings(r)
	return r, nil
}

// generateAssetsForRelease updates all assets files from template for a given release.
func generateAssetsForRelease(r common.WslReleaseInfo, wslPath, metaPath, rootPath string) (err error) {
	// Iterates and generates over assets
	assetsRelDir := filepath.Join("DistroLauncher-Appx", "Assets")
	assetsPath := filepath.Join(rootPath, assetsRelDir)
	images, err := ioutil.ReadDir(assetsPath)
	if err != nil {
		return err
	}

	mwTemplates := make(map[string]*imagick.MagickWand)
	for _, f := range images {
		// 1. Load size of the ressource image
		srcF, err := os.Open(filepath.Join(assetsPath, f.Name()))
		if err != nil {
			return err
		}
		defer srcF.Close()

		src, _, err := image.DecodeConfig(srcF)
		if err != nil {
			return err
		}

		// 2. Compute template name
		templateName := filepath.Base(f.Name())
		templateName = strings.Split(templateName, ".")[0]
		if strings.Contains(f.Name(), "altform-unplated") {
			templateName = fmt.Sprintf("%s.altform-unplated", templateName)
		}
		templatePath := filepath.Join(metaPath, "images", fmt.Sprintf("%s.svg", templateName))

		// 3. Replace any version id if present
		templateData, err := os.ReadFile(templatePath)
		if err != nil {
			return err
		}
		t := template.Must(template.New("").Parse(string(templateData)))
		var templateBuf bytes.Buffer
		if err := t.Execute(&templateBuf, r); err != nil {
			return err
		}

		// 4. Rescale
		// Reuse existing templates
		mw, exists := mwTemplates[templateName]
		if !exists {
			pw := imagick.NewPixelWand()
			pw.SetColor("none")
			mw = imagick.NewMagickWand()
			defer mw.Destroy()
			if err := mw.SetBackgroundColor(pw); err != nil {
				return err
			}
			if err := mw.ReadImageBlob(templateBuf.Bytes()); err != nil {
				return err
			}
			mwTemplates[templateName] = mw
		}
		mw = mw.Clone()
		defer mw.Destroy()

		// There is a limitation of 200K to the image size uploaded to the store. All source images are 8 bits.
		if err := mw.SetImageDepth(8); err != nil {
			return err
		}
		if err := mw.ResizeImage(uint(src.Width), uint(src.Height), imagick.FILTER_LANCZOS, 1); err != nil {
			return err
		}
		if err := mw.StripImage(); err != nil {
			return err
		}

		assetsDest := filepath.Join(wslPath, assetsRelDir, f.Name())
		if err := os.MkdirAll(filepath.Dir(assetsDest), 0755); err != nil {
			return err
		}
		if err := mw.WriteImage(assetsDest); err != nil {
			return err
		}
	}

	// Icon files
	tmpDir, err := os.MkdirTemp("", "update-releases-*")
	if err != nil {
		return err
	}
	defer os.RemoveAll(tmpDir)
	src := filepath.Join(tmpDir, "icon.svg")
	srcF, err := os.Create(src)
	if err != nil {
		return err
	}
	defer srcF.Close()

	srcIconPath := filepath.Join(metaPath, "images", "icon.svg")
	templateData, err := os.ReadFile(srcIconPath)
	if err != nil {
		return err
	}
	t := template.Must(template.New("").Parse(string(templateData)))
	if err := t.Execute(srcF, r); err != nil {
		return err
	}
	srcF.Close()

	dest := filepath.Join(wslPath, "DistroLauncher", "images", "icon.ico")
	if err := os.MkdirAll(filepath.Dir(dest), 0755); err != nil {
		return err
	}

	_, err = imagick.ConvertImageCommand([]string{
		"convert", "-strip", "-background", "none", src, "-resize", "256x256", "-define",
		"icon:auto-resize=16,32,48,256",
		dest,
	})
	return err
}
