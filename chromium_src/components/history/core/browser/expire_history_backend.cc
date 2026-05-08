/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#define BRAVE_START_EXPIRING_OLD_STUFF \
  work_queue_ = {};                     \
  weak_factory_.InvalidateWeakPtrs();   \
  if (expiration_threshold_.is_max()) {  \
    return;                             \
  }

#include <components/history/core/browser/expire_history_backend.cc>

#undef BRAVE_START_EXPIRING_OLD_STUFF
