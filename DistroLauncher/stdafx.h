//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <string>
#include <sstream>
#include <memory>
#include <assert.h>
#include <locale>
#include <codecvt>
#include <string_view>
#include <vector>
#include <wslapi.h>
#include <fstream>
#include <array>
#define SECURITY_WIN32
#include <sspi.h>
#include <secext.h>
#include <any>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <optional>
#include <variant>
#include <type_traits>
#include "expected.hpp"
#include "not_null.h"
#include "WslApiLoader.h"
#include "Helpers.h"
#include "DistributionInfo.h"
#include "OobeDefs.h"
#include "ExitStatus.h"
#include "OOBE.h"
#include "ProcessRunner.h"
#include "WSLInfo.h"
#include "Win32Utils.h"
#include "ApplicationStrategy.h"
#include "Application.h"
#include "UpgraderPolicy.h"
#include "RootUser.h"
// Message strings compiled from .MC file.
#include "messages.h"
#include "extended_messages.h"