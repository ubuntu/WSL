module github.com/ubuntu/wsl/e2e/test-runner

go 1.18

require github.com/ubuntu/wsl/e2e/constants v0.0.0

require gopkg.in/ini.v1 v1.67.0 // indirect

replace github.com/ubuntu/wsl/e2e/constants => ../constants
