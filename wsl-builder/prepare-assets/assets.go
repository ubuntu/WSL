package main

import (
	"bytes"
	"fmt"
	"image"
	_ "image/png"
	"io/fs"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"text/template"

	"gopkg.in/gographics/imagick.v2/imagick"
)

// updateAssets orchestrates the distro launcher metadata and assets generation from a csv file path.
func updateAssets(csvPath string) error {
	imagick.Initialize()
	defer imagick.Terminate()

	rootPath, err := getPathWith("DistroLauncher-Appx")
	if err != nil {
		return err
	}
	rootPath = filepath.Dir(rootPath)
	metaPath, err := getPathWith("meta")
	if err != nil {
		return err
	}

	releasesInfo, err := ReleasesInfo(csvPath)
	if err != nil {
		return err
	}

	// Update each release
	for _, r := range releasesInfo {
		// Reset directory
		wslPath := filepath.Join(metaPath, r.WslID)
		if err := os.RemoveAll(wslPath); err != nil {
			return err
		}
		if err := os.MkdirAll(wslPath, 0755); err != nil {
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

// generateMetaFilesForRelease updates all metadata files from template for a given release.
func generateMetaFilesForRelease(r wslReleaseInfo, wslPath, rootPath string) (err error) {
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
		if !strings.HasPrefix(relPath, "DistroLauncher/") && !strings.HasPrefix(relPath, "DistroLauncher-Appx/") {
			return nil
		}
		if d.IsDir() {
			return nil
		}

		// ensure we have template file to substitude data in
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		templateData := string(data)
		if !strings.Contains(templateData, "{{.") {
			return nil
		}

		destPath := filepath.Join(wslPath, relPath)
		if err := os.MkdirAll(filepath.Dir(destPath), 0755); err != nil {
			return err
		}

		t := template.Must(template.New("").Parse(templateData))

		f, err := os.Create(destPath)
		if err != nil {
			return nil
		}
		defer f.Close()

		return t.Execute(f, r)
	})
	return err
}

// generateAssetsForRelease updates all assets files from template for a given release.
func generateAssetsForRelease(r wslReleaseInfo, wslPath, metaPath, rootPath string) (err error) {
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

		if err := mw.ResizeImage(uint(src.Width), uint(src.Height), imagick.FILTER_LANCZOS, 1); err != nil {
			return err
		}
		if err := mw.SetImageCompressionQuality(95); err != nil {
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
