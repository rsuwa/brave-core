// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_H_
#define BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

OBJC_EXPORT
@interface BravePlaylistExceptionsIOS : NSObject

- (instancetype)init NS_UNAVAILABLE;

/// Returns whether `pageSrc` may be re-resolved later (e.g. via
/// LivePlaylistWebLoader). Mirrors `PlaylistExceptions::CanResolvePageSrcLater`.
- (bool)canResolvePageSrcLater:(NSURL*)url;

/// Rows from the loaded component list (`domain<TAB>condition`), for debugging.
- (NSArray<NSString*>*)listPlaylistExceptions;

@end

NS_ASSUME_NONNULL_END

#endif  // BRAVE_IOS_BROWSER_API_BRAVE_PLAYLIST_PLAYLIST_EXCEPTIONS_IOS_H_
