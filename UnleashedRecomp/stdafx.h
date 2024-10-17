#pragma once

#define NOMINMAX

#include <windows.h>
#include <mutex>
#include <vector>
#include <string>
#include <cassert>
#include <xbox.h>
#include <xxhash.h>
#include <ankerl/unordered_dense.h>
#include <ddspp.h>
#include <ppc/ppc_recomp_shared.h>
#include <toml++/toml.hpp>

#include "framework.h"
#include "mutex.h"
#include "config.h"
