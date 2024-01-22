package launchertester

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestHelpFlagNoInstall(t *testing.T) {
	wslSetup(t)

	ctx, cancel := context.WithTimeout(context.Background(), commandTimeout)
	defer cancel()

	out, err := launcherCommand(ctx, "help").CombinedOutput()
	require.NoErrorf(t, err, "Unexpected error using help message: %s", out)

	require.Equal(t, "DistroNotFound", distroState(t), "Using command help should not install the distro")
}
