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
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
	"text/template"

	shutil "github.com/termie/go-shutil"
	"github.com/ubuntu/wsl/wsl-builder/common"
	"gopkg.in/gographics/imagick.v2/imagick"
)

// updateAssets orchestrates the distro launcher metadata and assets generation from a csv file path.
func updateAssets(csvPath string) error {
	// pngquant is a required dependency
	if _, err := exec.LookPath("pngquant"); err != nil {
		return err
	}

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

	// Collect all files to copy from generic dirs
	// We blacklist DistroLauncher-Appx/Assets/ and DistroLauncher/images/, as all those icons are generated from svg.
	files, err := listFilesForMeta(nil, rootPath,
		[]string{"DistroLauncher-Appx/Assets/", "DistroLauncher/images/"}, false,
		filepath.Join(rootPath, "DistroLauncher/"),
		filepath.Join(rootPath, "DistroLauncher-Appx/"))
	if err != nil {
		return err
	}

	// Extends with our general meta where we take all files
	if files, err = listFilesForMeta(files, filepath.Join(rootPath, "meta", "src"), nil, true,
		filepath.Join(rootPath, "meta", "src")); err != nil {
		return err
	}

	// Update each application
	for _, r := range releasesInfo {
		wslPath := filepath.Join(metaPath, r.WslID)
		generatedPath := filepath.Join(wslPath, common.GeneratedDir)

		// Cleanup previous generated directory
		if err := os.RemoveAll(generatedPath); err != nil {
			return err
		}

		// Reference files for this application
		refFiles := make(map[string]string)
		for k, v := range files {
			refFiles[k] = v
		}

		// Collect all files we can use overridding the main ones
		if refFiles, err = listFilesForMeta(refFiles, filepath.Join(wslPath, "src"), nil, true,
			filepath.Join(wslPath, "src")); err != nil {
			return err
		}

		// And now, generate the application meta from xml template
		if err := generateMetaForRelease(r, refFiles, rootPath, generatedPath); err != nil {
			return err
		}

		// Generate the application and launcher icons
		if err := generateImages(r, refFiles, rootPath, generatedPath); err != nil {
			return err
		}
	}

	return nil
}

// listFilesForMeta collects all templates files, icons and store content in every given path.
// The existing files map will be extended and overridden with new files.
// blacklist will ignore entire directories relative paths.
// if all is true, it doesn’t tests for templates, icons or images.
//
// The returned map is a relative path to refPath to copy to, with the corresponding source file to copy from.
func listFilesForMeta(existing map[string]string, refPath string, blacklist []string, all bool, paths ...string) (files map[string]string, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't list files for meta generation: %v", err)
		}
	}()

	files = make(map[string]string)
	if existing != nil {
		files = existing
	}

	// collect all files with templates in src, icons and general store
	for _, rootPath := range paths {
		if err := filepath.WalkDir(rootPath, func(path string, d fs.DirEntry, err error) error {
			// ignore if override does not exists
			if errors.Is(err, fs.ErrNotExist) {
				return nil
			}
			if err != nil {
				return err
			}
			// only list files
			if d.IsDir() {
				return nil
			}

			relPath := strings.TrimPrefix(path, refPath+"/")

			// blacklisted paths
			for _, b := range blacklist {
				if strings.HasPrefix(relPath, b) {
					return nil
				}
			}

			// Ensure we have template file to substitute data in or that it’s an image
			if !all {
				data, err := os.ReadFile(path)
				if err != nil {
					return err
				}
				if !strings.Contains(string(data), "{{.") && filepath.Ext(path) != ".png" && filepath.Ext(path) != ".svg" {
					return nil
				}
			}

			files[relPath] = path

			return nil
		}); err != nil {
			return nil, err
		}
	}

	return files, nil
}

// generateMetaForRelease updates all teamplated non img files for a given release.
func generateMetaForRelease(r common.WslReleaseInfo, files map[string]string, rootPath, generatedPath string) (err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("can't generate metadata file for %s: %v", r.WslID, err)
		}
	}()

	type captionScreenshot struct {
		DisplayName string
		Path        string
	}

	type releaseInfoWithStoreScreenshots struct {
		common.WslReleaseInfo
		StoreScreenShots []captionScreenshot
	}

	for relPath, src := range files {
		// We handle svg separately
		if filepath.Ext(relPath) == ".svg" {
			continue
		}

		dest := filepath.Join(generatedPath, relPath)
		if err := os.MkdirAll(filepath.Dir(dest), 0755); err != nil {
			return err
		}

		if filepath.Ext(dest) == ".tmpl" {
			dest = strings.TrimSuffix(dest, ".tmpl")
		}

		data, err := os.ReadFile(src)
		if err != nil {
			return err
		}
		templateData := string(data)

		// Not a template or file we will replace later: direct copy
		if !strings.Contains(templateData, "{{.") || filepath.Base(dest) == "ProductDescription.xml" {
			if err := shutil.CopyFile(src, dest, false); err != nil {
				return err
			}
			continue
		}

		// Replace template content
		t := template.Must(template.New("").Parse(templateData))
		f, err := os.Create(dest)
		if err != nil {
			return nil
		}
		defer f.Close()
		if err := t.Execute(f, r); err != nil {
			return err
		}
	}

	// Replace ProductDescription.xml now that we have all images copied (raw and generated pngs)
	err = filepath.WalkDir(generatedPath, func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if filepath.Base(path) != "ProductDescription.xml" {
			return nil
		}

		rWithScreenshots := releaseInfoWithStoreScreenshots{WslReleaseInfo: r}
		// Collect and sort all original images within the same directory
		screenshots, err := listAndSortImagesIn(filepath.Dir(path))
		if err != nil {
			return err
		}
		for _, s := range screenshots {
			rWithScreenshots.StoreScreenShots = append(rWithScreenshots.StoreScreenShots, captionScreenshot{
				Path:        s,
				DisplayName: strings.TrimSuffix(strings.ReplaceAll(s, "_", " "), ".png"),
			})
		}

		// replace template content
		data, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		t := template.Must(template.New("").Parse(string(data)))

		f, err := os.Create(path)
		if err != nil {
			return nil
		}
		defer f.Close()
		return t.Execute(f, rWithScreenshots)
	})

	return err
}

// listAndSortImagesIn returns a sorted list of all png images in a directory
func listAndSortImagesIn(path string) ([]string, error) {
	files, err := os.ReadDir(path)
	if err != nil {
		return nil, err
	}

	// Filter and sort them
	var r []string
	for _, f := range files {
		if !strings.HasSuffix(f.Name(), ".png") {
			continue
		}
		r = append(r, f.Name())
	}
	sort.Strings(r)
	return r, nil
}

// generateImages creates .png and icons using templated svg.
func generateImages(r common.WslReleaseInfo, templates map[string]string, rootPath, generatedPath string) (err error) {
	// Iterates and generates over generated assets as a reference

	// A. Store and application images
	relDir := filepath.Join("DistroLauncher-Appx", "Assets")
	if err := os.MkdirAll(filepath.Join(generatedPath, relDir), 0755); err != nil {
		return err
	}
	assetsRefPath := filepath.Join(rootPath, relDir)
	images, err := ioutil.ReadDir(assetsRefPath)
	if err != nil {
		return err
	}

	mwTemplates := make(map[string]*imagick.MagickWand)
	for _, f := range images {
		// 1. Load size of the ressource image
		refF, err := os.Open(filepath.Join(assetsRefPath, f.Name()))
		if err != nil {
			return err
		}
		defer refF.Close()

		ref, _, err := image.DecodeConfig(refF)
		if err != nil {
			return err
		}

		// 2. Compute template name
		templateName := filepath.Base(f.Name())
		templateName = strings.Split(templateName, ".")[0]
		if strings.Contains(f.Name(), "altform-unplated") {
			templateName = fmt.Sprintf("%s.altform-unplated", templateName)
		}
		templateRelPath := filepath.Join(relDir, fmt.Sprintf("%s.svg", templateName))

		// 3. Replace any version id if present
		templateData, err := os.ReadFile(templates[templateRelPath])
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
		if err := mw.SetImageFormat(strings.TrimPrefix(filepath.Ext(f.Name()), ".")); err != nil {
			return err
		}
		if err := mw.ResizeImage(uint(ref.Width), uint(ref.Height), imagick.FILTER_LANCZOS, 1); err != nil {
			return err
		}
		if err := mw.StripImage(); err != nil {
			return err
		}

		// Crush the image size for png files
		var img = mw.GetImageBlob()
		if filepath.Ext(f.Name()) == ".png" {
			cmd := exec.Command("pngquant", "-")
			var out bytes.Buffer
			cmd.Stdin = bytes.NewBuffer(img)
			cmd.Stdout = &out
			err := cmd.Run()
			if err != nil {
				return err
			}
			img = out.Bytes()
		}

		assetsDest := filepath.Join(generatedPath, relDir, f.Name())
		if err := os.MkdirAll(filepath.Dir(assetsDest), 0755); err != nil {
			return err
		}
		f, err := os.Create(assetsDest)
		if err != nil {
			return err
		}
		defer f.Close()
		if _, err := f.Write(img); err != nil {
			return err
		}
		f.Close()
	}

	// B. Icon files
	iconRelPath := filepath.Join("DistroLauncher", "images", "icon.svg")
	if err := os.MkdirAll(filepath.Join(generatedPath, filepath.Dir(iconRelPath)), 0755); err != nil {
		return err
	}
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

	templateData, err := os.ReadFile(templates[iconRelPath])
	if err != nil {
		return err
	}
	t := template.Must(template.New("").Parse(string(templateData)))
	if err := t.Execute(srcF, r); err != nil {
		return err
	}
	srcF.Close()

	dest := filepath.Join(generatedPath, strings.ReplaceAll(iconRelPath, ".svg", ".ico"))
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
