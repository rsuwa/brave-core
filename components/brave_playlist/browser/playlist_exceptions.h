/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_PLAYLIST_BROWSER_PLAYLIST_EXCEPTIONS_H_
#define BRAVE_COMPONENTS_BRAVE_PLAYLIST_BROWSER_PLAYLIST_EXCEPTIONS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

// Component payload `playlist_exceptions.json` schema (JSON object):
// {
//   "version": 1,
//   "rules": [
//     {
//       "registrable_domain": "youtube.com",
//       "deny_root_path": true,
//       "path_prefixes": ["/results", "/feed", "/@"]
//     }
//   ]
// }
// A URL is blocked (CanResolvePageSrcLater == false) when its 
// registrable domain matches a rule AND (deny_root_path matches "" or "/" path OR path
// starts with any listed prefix). 
// Note: Prefix "/" alone must not be used (use deny_root_path).

namespace brave_playlist {

class PlaylistResolveRule {
 public:
  PlaylistResolveRule();
  ~PlaylistResolveRule();

  PlaylistResolveRule(const PlaylistResolveRule&);
  PlaylistResolveRule& operator=(const PlaylistResolveRule&);
  PlaylistResolveRule(PlaylistResolveRule&&) noexcept;
  PlaylistResolveRule& operator=(PlaylistResolveRule&&) noexcept;

  std::string registrable_domain;
  std::vector<std::string> path_prefixes;
  bool deny_root_path = false;
};

class PlaylistExceptions {
 public:
  PlaylistExceptions(const PlaylistExceptions&) = delete;
  PlaylistExceptions& operator=(const PlaylistExceptions&) = delete;
  ~PlaylistExceptions();

  static PlaylistExceptions* GetInstance();

  void OnComponentReady(const base::FilePath& component_dir);

  // Returns true when `pageSrc` may be used for LivePlaylist-style reload /
  // re-resolution. When the component list is not loaded yet, returns true
  bool CanResolvePageSrcLater(const GURL& url) const;

  // Debugging / UI: human-readable "domain<TAB>condition" rows.
  std::vector<std::string> ListPlaylistExceptions() const;

  void ResetForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(PlaylistExceptionsUnitTest,
                           RulesBlockListedPaths);
  FRIEND_TEST_ALL_PREFIXES(PlaylistExceptionsUnitTest, NotReadyIsPermissive);

  PlaylistExceptions();

  void OnPlaylistExceptionsLoaded(const std::string& contents);

  base::FilePath component_path_;
  std::vector<PlaylistResolveRule> rules_;
  bool is_ready_ = false;
  base::WeakPtrFactory<PlaylistExceptions> weak_factory_{this};

  friend struct base::DefaultSingletonTraits<PlaylistExceptions>;
  friend class PlaylistExceptionsUnitTest;
};

}  // namespace brave_playlist

#endif  // BRAVE_COMPONENTS_BRAVE_PLAYLIST_BROWSER_PLAYLIST_EXCEPTIONS_H_
