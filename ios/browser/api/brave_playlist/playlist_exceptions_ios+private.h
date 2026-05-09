// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_PRIVATE_H_
#define BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_PRIVATE_H_

#include "brave/components/brave_playlist/browser/playlist_exceptions.h"
#include "brave/ios/browser/api/brave_playlist/playlist_exceptions_ios.h"

@interface BravePlaylistExceptionsIOS (Private)
- (instancetype)initWithPlaylistExceptions:
    (brave_playlist::PlaylistExceptions*)playlistExceptions;
@end

#endif  // BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_PRIVATE_H_
