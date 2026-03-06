#pragma once
#include "IMGUI/imgui.h"
#include "StatusMessageUI.h"
#include <string>
#include <vector>

class SuiteSpot;

class LoadoutUI {
public:
    explicit LoadoutUI(SuiteSpot* plugin);
    void RenderLoadoutControls();

private:
    SuiteSpot* plugin_;

    UI::StatusMessage loadoutStatus;
    std::string currentLoadoutName;
    std::vector<std::string> loadoutNames;
    int selectedLoadoutIndex = 0;
    bool loadoutsInitialized = false;
};
