// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveShared
import Foundation
import Web

extension TabDataValues {
  private struct WidgetSearchTabHelperKey: TabDataKey {
    static var defaultValue: WidgetSearchTabHelper? { nil }
  }

  var widgetSearchTabHelper: WidgetSearchTabHelper? {
    get { self[WidgetSearchTabHelperKey.self] }
    set { self[WidgetSearchTabHelperKey.self] = newValue }
  }
}

/// Marks a tab as having a pending widget-initiated search whose next Brave Search request
/// should use `source=ios-widget`.
///
/// Install the helper when a widget search is initiated; it tears itself down once the search
/// is consumed via ``TabState/searchURLWithWidgetAttribution(from:query:locale:isBraveSearchPromotion:)``
/// or once the browser navigates off to a different tab page.
final class WidgetSearchTabHelper: TabObserver {
  private weak var tab: (any TabState)?

  init(tab: some TabState) {
    self.tab = tab
    tab.addObserver(self)
  }

  deinit {
    tab?.removeObserver(self)
  }

  // MARK: - TabObserver

  func tabDidCommitNavigation(_ tab: some TabState) {
    guard self.tab === tab else { return }
    // Don't tear down on the NTP commit that precedes the actual widget search navigation.
    guard let url = tab.lastCommittedURL, !url.isNewTabURL else { return }
    finalize()
  }

  func tabWillBeDestroyed(_ tab: some TabState) {
    // The tab is going away; clearing widgetSearchTabHelper on it would be moot.
    tab.removeObserver(self)
  }

  // MARK: - Teardown

  /// Ends the pending widget search flow and removes this helper from the tab.
  ///
  /// After this call the helper is no longer reachable via `TabDataValues` and will be deallocated once the current call stack unwinds.
  func finalize() {
    tab?.widgetSearchTabHelper = nil
  }
}

extension TabState {
  /// Builds a search URL from `engine`, applying widget attribution when ``widgetSearchTabHelper``
  /// is present, and finalizing that helper as part of the same call
  ///
  /// Finalization happens before URL construction so that any re-entrant URL
  /// build during this call (e.g. a second search submitted before navigation commits) cannot see
  /// the helper as  still pending and apply widget attribution a second time.
  func searchURLWithWidgetAttribution(
    from engine: OpenSearchEngine,
    query: String,
    locale: Locale = .current,
    isBraveSearchPromotion: Bool = false
  ) -> URL? {
    let helper = data.widgetSearchTabHelper
    helper?.finalize()
    return engine.searchURLForQuery(
      query,
      locale: locale,
      isBraveSearchPromotion: isBraveSearchPromotion,
      isWidgetSearchAttribution: helper != nil
    )
  }
}
