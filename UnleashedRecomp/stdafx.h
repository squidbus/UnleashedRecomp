#pragma once

#define NOMINMAX

#include <windows.h>
#include <ShlObj_core.h>
#include <algorithm>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <format>
#include <vector>
#include <string>
#include <cassert>
#include <xbox.h>
#include <xxhash.h>
#include <ankerl/unordered_dense.h>
#include <ddspp.h>
#include <ppc/ppc_recomp_shared.h>
#include <toml++/toml.hpp>
#include <zstd.h>

#include "framework.h"
#include "mutex.h"
