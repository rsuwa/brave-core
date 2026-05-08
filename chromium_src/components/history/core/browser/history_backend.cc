/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <components/history/core/browser/history_backend.cc>

namespace history {

namespace {

constexpr int kKeepHistoryForever = -1;

}  // namespace

HistoryCountResult HistoryBackend::GetKnownToSyncCount() {
  int count = 0;
  return {db_ && db_->GetKnownToSyncCount(&count), count};
}

void HistoryBackend::UpdateExpireDaysThreshold(int days) {
  expirer_.StartExpiringOldStuff(days == kKeepHistoryForever
                                     ? base::TimeDelta::Max()
                                     : base::Days(days));
}

}  // namespace history
