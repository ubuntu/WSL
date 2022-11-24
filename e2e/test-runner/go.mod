module github.com/ubuntu/wsl/e2e/test-runner

go 1.18

require github.com/ubuntu/wsl/e2e/constants v0.0.0

require (
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	github.com/stretchr/testify v1.8.1 // indirect
	gopkg.in/ini.v1 v1.67.0 // indirect
	gopkg.in/yaml.v3 v3.0.1 // indirect
)

replace github.com/ubuntu/wsl/e2e/constants => ../constants
