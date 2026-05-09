/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_playlist/browser/playlist_exceptions.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace brave_playlist {

class PlaylistExceptionsUnitTest : public testing::Test {
 public:
  PlaylistExceptionsUnitTest() = default;

 protected:
  void SetUp() override { PlaylistExceptions::GetInstance()->ResetForTesting(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(PlaylistExceptionsUnitTest, NotReadyIsPermissive) {
  PlaylistExceptions* ex = PlaylistExceptions::GetInstance();
  ASSERT_FALSE(ex->is_ready_);
  EXPECT_TRUE(ex->CanResolvePageSrcLater(GURL("https://youtube.com/")));
}

TEST_F(PlaylistExceptionsUnitTest, RulesBlockListedPaths) {
  PlaylistExceptions* ex = PlaylistExceptions::GetInstance();

  constexpr char kJson[] = R"({
    "version": 1,
    "rules": [{
      "registrable_domain": "youtube.com",
      "deny_root_path": true,
      "path_prefixes": ["/results", "/feed", "/@"]
    }]
  })";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(
      temp_dir.GetPath().AppendASCII("playlist_exceptions.json"), kJson));

  ex->OnComponentReady(temp_dir.GetPath());
  ASSERT_TRUE(base::test::RunUntil([&]() { return ex->is_ready_; }));

  EXPECT_FALSE(ex->CanResolvePageSrcLater(GURL("https://youtube.com/")));
  EXPECT_FALSE(ex->CanResolvePageSrcLater(GURL("https://www.youtube.com")));
  EXPECT_FALSE(
      ex->CanResolvePageSrcLater(GURL("https://www.youtube.com/results?foo=1")));
  EXPECT_FALSE(
      ex->CanResolvePageSrcLater(GURL("https://www.youtube.com/@Example")));
  EXPECT_TRUE(
      ex->CanResolvePageSrcLater(GURL("https://www.youtube.com/watch?v=1")));

  std::vector<std::string> listed = ex->ListPlaylistExceptions();
  ASSERT_EQ(listed.size(), 4u);
}

}  // namespace brave_playlist
