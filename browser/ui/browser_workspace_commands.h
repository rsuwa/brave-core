/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_BROWSER_WORKSPACE_COMMANDS_H_
#define BRAVE_BROWSER_UI_BROWSER_WORKSPACE_COMMANDS_H_

#include <string>

class Profile;

namespace brave {

void ShowSaveWorkspaceDialog(Profile* profile);
void ShowOpenWorkspaceDialog(Profile* profile);
void SaveWorkspace(Profile* profile, const std::string& name);
void RestoreWorkspace(Profile* profile, const std::string& name);

}  // namespace brave

#endif  // BRAVE_BROWSER_UI_BROWSER_WORKSPACE_COMMANDS_H_
