#pragma once

void SelectLevelInConfig(int idx, bool remove = false);
void RenderLevelInConfig();

void SaveCurrentLevelInConfig();
void RemoveCurrentLevelFromConfig();
bool IsCurrentLevelInConfig();

void RenderCurrentLevel(bool currentReplay = false);

void RestartGame();
