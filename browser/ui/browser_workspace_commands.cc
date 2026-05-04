/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/browser_workspace_commands.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "brave/browser/workspace/brave_workspace_service.h"
#include "brave/browser/workspace/brave_workspace_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/core/session_types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace brave {

namespace {

// Appends session commands for a single browser window to |commands| and
// updates |active_window_id| to this window if it is active (focused), or if
// no active window has been recorded yet. Returns the tab count serialized.
int AppendBrowserSessionCommands(
    TabStripModel* tsm,
    BrowserWindow* window,
    std::vector<std::unique_ptr<sessions::SessionCommand>>& commands,
    SessionID& active_window_id) {
  if (tsm->count() == 0) {
    return 0;
  }

  SessionID window_id = SessionID::NewUnique();
  commands.push_back(sessions::CreateSetWindowTypeCommand(
      window_id, sessions::SessionWindow::TYPE_NORMAL));
  commands.push_back(sessions::CreateSetWindowBoundsCommand(
      window_id, window->GetRestoredBounds(), window->GetRestoredState()));

  // Prefer the focused window; fall back to the first window if none is active.
  if (window->IsActive() || active_window_id == SessionID::InvalidValue()) {
    active_window_id = window_id;
  }

  // Emit group metadata for every tab group in this window.
  if (tsm->group_model()) {
    for (const tab_groups::TabGroupId& group_id :
         tsm->group_model()->ListTabGroups()) {
      auto* group = tsm->group_model()->GetTabGroup(group_id);
      if (!group || !group->visual_data()) {
        continue;
      }
      commands.push_back(sessions::CreateTabGroupMetadataUpdateCommand(
          group_id, group->visual_data()));
    }
  }

  int tab_count = 0;
  for (int i = 0; i < tsm->count(); ++i) {
    content::WebContents* contents = tsm->GetWebContentsAt(i);
    if (!contents) {
      continue;
    }

    SessionID tab_id = SessionID::NewUnique();

    commands.push_back(sessions::CreateSetTabWindowCommand(window_id, tab_id));
    commands.push_back(sessions::CreateSetTabIndexInWindowCommand(tab_id, i));

    if (tsm->IsTabPinned(i)) {
      commands.push_back(sessions::CreatePinnedStateCommand(tab_id, true));
    }

    std::optional<tab_groups::TabGroupId> group_id = tsm->GetTabGroupForTab(i);
    if (group_id.has_value()) {
      commands.push_back(sessions::CreateTabGroupCommand(tab_id, group_id));
    }

    // Serialize the full navigation history for this tab.
    auto& controller = contents->GetController();
    int nav_count = controller.GetEntryCount();
    int current_entry = controller.GetCurrentEntryIndex();
    for (int j = 0; j < nav_count; ++j) {
      content::NavigationEntry* entry = controller.GetEntryAtIndex(j);
      if (!entry) {
        continue;
      }
      auto serialized =
          sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
              j, entry);
      commands.push_back(
          sessions::CreateUpdateTabNavigationCommand(tab_id, serialized));
    }

    commands.push_back(sessions::CreateSetSelectedNavigationIndexCommand(
        tab_id, current_entry));
    tab_count++;
  }

  commands.push_back(sessions::CreateSetSelectedTabInWindowCommand(
      window_id, tsm->active_index()));
  return tab_count;
}

void DoRestoreWorkspace(
    base::WeakPtr<Profile> profile,
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands) {
  if (!profile || commands.empty()) {
    LOG(ERROR) << "Could not load workspace";
    return;
  }

  // RestoreSessionFromCommands constructs SessionTab/SessionWindow objects
  // whose constructors call SessionID::NewUnique(), which is sequence-checked
  // to the UI thread.  It must therefore be called here (UI thread), not in
  // the background I/O task.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows;
  SessionID active_window_id = SessionID::InvalidValue();
  std::string platform_session_id;
  std::set<SessionID> discarded_window_ids;
  sessions::RestoreSessionFromCommands(commands, &windows, &active_window_id,
                                       &platform_session_id,
                                       &discarded_window_ids);
  if (windows.empty()) {
    return;
  }

  // We restore tabs ourselves rather than using RestoreForeignSessionWindows
  // because that API creates an empty new_group_ids map and never calls
  // RestoreTabGroupMetadata, so tab group names/colors are silently dropped.
  // By passing the original TabGroupId directly to AddRestoredTab, we avoid
  // any ID remapping and can apply visual data with the same IDs afterwards.
  for (const auto& window : windows) {
    if (window->tabs.empty()) {
      continue;
    }

    if (Browser::GetCreationStatusForProfile(profile.get()) !=
        Browser::CreationStatus::kOk) {
      continue;
    }
    Browser::CreateParams params(Browser::TYPE_NORMAL, profile.get(), false);
    params.initial_bounds = window->bounds;
    params.initial_show_state = window->show_state;
    params.initial_workspace = window->workspace;
    params.initial_visible_on_all_workspaces_state =
        window->visible_on_all_workspaces;
    params.should_trigger_session_restore = false;
    Browser* browser = Browser::Create(params);
    if (!browser) {
      continue;
    }

    auto* tsm = browser->tab_strip_model();
    for (int i = 0; i < static_cast<int>(window->tabs.size()); ++i) {
      const auto& tab = window->tabs[i];
      if (tab->navigations.empty()) {
        continue;
      }
      chrome::AddRestoredTab(
          browser, tab->navigations,
          /*tab_index=*/i, tab->normalized_navigation_index(),
          tab->extension_app_id,
          /*group=*/tab->group,
          /*select=*/false, tab->pinned,
          /*last_active_time_ticks=*/base::TimeTicks(), tab->last_active_time,
          /*storage_namespace=*/nullptr, tab->user_agent_override,
          tab->extra_data,
          /*from_session_restore=*/true,
          /*is_active_browser=*/std::nullopt);
    }

    // Apply group names, colors, and collapsed state.  Since we passed the
    // original TabGroupIds to AddRestoredTab, no ID remapping is needed.
    for (const auto& session_group : window->tab_groups) {
      if (!tsm->group_model() ||
          !tsm->group_model()->ContainsTabGroup(session_group->id)) {
        continue;
      }
      tsm->ChangeTabGroupVisuals(session_group->id, session_group->visual_data);
    }

    int active = std::clamp(window->selected_tab_index, 0,
                            std::max(0, tsm->count() - 1));
    if (active < tsm->count()) {
      tsm->ActivateTabAt(active);
    }
    browser->window()->Show();
  }
}

}  // namespace

void SaveWorkspace(Profile* profile, const std::string& name) {
  if (name.empty()) {
    return;
  }

  auto* service = BraveWorkspaceServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  // Collect session commands on the UI thread, then write to disk on a
  // background task (WriteWorkspaceToDisk does blocking file I/O).
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;

  SessionID active_window_id = SessionID::InvalidValue();
  int window_count = 0;
  int tab_count = 0;

  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* bwi) {
        if (bwi->GetProfile() != profile ||
            bwi->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
          return true;
        }
        window_count++;
        tab_count += AppendBrowserSessionCommands(
            bwi->GetBrowserForMigrationOnly()->tab_strip_model(),
            bwi->GetBrowserForMigrationOnly()->window(), commands,
            active_window_id);
        return true;
      });

  if (tab_count == 0) {
    return;
  }

  if (active_window_id != SessionID::InvalidValue()) {
    commands.push_back(
        sessions::CreateSetActiveWindowCommand(active_window_id));
  }

  base::FilePath workspace_dir = service->GetWorkspaceDirForName(name);

  // CommandStorageBackend must be constructed on the UI thread because its
  // constructor calls SingleThreadTaskRunner::GetCurrentDefault().  We create
  // a MayBlock SequencedTaskRunner here and pass it as the backend's owning
  // runner, then post the actual file I/O to that same runner.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  auto backend = base::MakeRefCounted<sessions::CommandStorageBackend>(
      task_runner, workspace_dir, BraveWorkspaceService::kWorkspaceSessionType,
      /*encryptor=*/std::nullopt);

  base::Time save_time = base::Time::Now();

  // |wrote_ok| is shared between |on_error| (sets false) and |on_success|
  // (reads).  Both run on the UI thread, with |on_error| guaranteed to run
  // first because AppendCommands enqueues it before WriteWorkspaceToDisk
  // enqueues |on_success|.
  auto wrote_ok = base::MakeRefCounted<base::RefCountedData<bool>>(true);

  auto on_success = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](base::WeakPtr<BraveWorkspaceService> service, std::string name,
             int window_count, int tab_count, base::Time modified_at,
             scoped_refptr<base::RefCountedData<bool>> wrote_ok) {
            if (service && wrote_ok->data) {
              service->SaveWorkspaceMetadata(name, window_count, tab_count,
                                             modified_at);
            }
          },
          service->GetWeakPtr(), name, window_count, tab_count, save_time,
          wrote_ok));

  auto on_error = base::BindOnce(
      [](scoped_refptr<base::RefCountedData<bool>> wrote_ok) {
        wrote_ok->data = false;
      },
      wrote_ok);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BraveWorkspaceService::WriteWorkspaceToDisk,
                                std::move(commands), std::move(workspace_dir),
                                std::move(backend), std::move(on_success),
                                std::move(on_error)));
}

void ShowSaveWorkspaceDialog(Profile* profile) {
  // TODO(https://github.com/brave/brave-browser/issues/55108)
  brave::SaveWorkspace(profile, "example-workspace");
}

void ShowOpenWorkspaceDialog(Profile* profile) {
  // TODO(https://github.com/brave/brave-browser/issues/55108)
  brave::RestoreWorkspace(profile, "example-workspace");
}

void RestoreWorkspace(Profile* profile, const std::string& name) {
  auto* service = BraveWorkspaceServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  base::FilePath path = service->GetWorkspaceDirForName(name);

  // Construct the backend on the UI thread (requires SingleThreadTaskRunner
  // context), then hand it to the background sequenced runner for file I/O.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  auto backend = base::MakeRefCounted<sessions::CommandStorageBackend>(
      task_runner, path, BraveWorkspaceService::kWorkspaceSessionType,
      /*encryptor=*/std::nullopt);

  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BraveWorkspaceService::ReadWorkspaceFromDisk,
                     std::move(path), std::move(backend)),
      base::BindOnce(&DoRestoreWorkspace, profile->GetWeakPtr()));
}

}  // namespace brave
