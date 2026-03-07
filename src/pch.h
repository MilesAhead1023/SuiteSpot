#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include <Windows.h>
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <filesystem>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_searchablecombo.h"
#include "imgui_rangeslider.h"
#include <nlohmann/json.hpp>

#include "logging.h"
