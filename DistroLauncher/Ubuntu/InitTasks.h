#pragma once
namespace Ubuntu
{
	// Returns true if system initialization tasks are complete.
	// If [checkDefaultUser] is true, we consider creating the default user part of such tasks.
	bool CheckInitTasks(WslApiLoader& api, bool checkDefaultUser);
};

