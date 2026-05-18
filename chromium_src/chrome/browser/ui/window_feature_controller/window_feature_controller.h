/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_
#define BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#define UsesImmersiveFullscreenMode virtual UsesImmersiveFullscreenMode
#define UsesImmersiveFullscreenTabbedMode \
  virtual UsesImmersiveFullscreenTabbedMode
#endif

#include <chrome/browser/ui/window_feature_controller/window_feature_controller.h>  // IWYU pragma: export

#if BUILDFLAG(IS_MAC)
#undef UsesImmersiveFullscreenTabbedMode
#undef UsesImmersiveFullscreenMode
#endif

#endif  // BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_WINDOW_FEATURE_CONTROLLER_WINDOW_FEATURE_CONTROLLER_H_
