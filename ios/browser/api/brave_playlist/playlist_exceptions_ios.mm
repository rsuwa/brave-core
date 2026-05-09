// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/brave_playlist/browser/playlist_exceptions.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "brave/ios/browser/api/brave_playlist/playlist_exceptions_ios+private.h"
#include "net/base/apple/url_conversions.h"
#include "url/gurl.h"

@interface BravePlaylistExceptionsIOS () {
  raw_ptr<brave_playlist::PlaylistExceptions> _playlistExceptions;  // NOT OWNED
}

@end

@implementation BravePlaylistExceptionsIOS

- (instancetype)initWithPlaylistExceptions:
    (brave_playlist::PlaylistExceptions*)playlistExceptions {
  if ((self = [super init])) {
    _playlistExceptions = playlistExceptions;
  }
  return self;
}

- (bool)canResolvePageSrcLater:(NSURL*)url {
  GURL gurl = net::GURLWithNSURL(url);
  if (!gurl.is_valid()) {
    return true;
  }
  return _playlistExceptions->CanResolvePageSrcLater(gurl);
}

- (NSArray<NSString*>*)listPlaylistExceptions {
  const std::vector<std::string> rows =
      _playlistExceptions->ListPlaylistExceptions();
  NSMutableArray<NSString*>* out =
      [NSMutableArray arrayWithCapacity:rows.size()];
  for (const std::string& row : rows) {
    [out addObject:base::SysUTF8ToNSString(row)];
  }
  return [out copy];
}

@end
