// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveCore
import BraveShields
import BraveUI
import Data
import Shared
import SnapKit
import SwiftUI
import UIKit
import Web
import WebKit

class QuickViewController: UIViewController {
  private let url: URL
  private weak var parentTab: (any TabState)?
  private var currentTab: (any TabState)?
  private let privateBrowsingManager: PrivateBrowsingManager
  private let toolbarViewModel: QuickViewToolbarModel
  private var readerModeHandler: ReaderModeScriptHandler?
  private lazy var toolbarHostingController = UIHostingController(
    rootView: QuickViewToolbarView(viewModel: toolbarViewModel)
  )
  private let onOpenInNewTab: ((URLRequest) -> Void)?

  init(
    url: URL,
    for tab: some TabState,
    privateBrowsingManager: PrivateBrowsingManager,
    onOpenInNewTab: ((URLRequest) -> Void)?
  ) {
    self.url = url
    self.parentTab = tab
    self.toolbarViewModel = QuickViewToolbarModel(url: url)
    self.privateBrowsingManager = privateBrowsingManager
    self.onOpenInNewTab = onOpenInNewTab
    super.init(nibName: nil, bundle: nil)
    modalPresentationStyle = .fullScreen

    updateViewModel()
  }

  @available(*, unavailable)
  required init?(coder aDecoder: NSCoder) { fatalError() }

  override func viewDidLoad() {
    super.viewDidLoad()

    guard let parentTab else {
      return
    }

    var initialConfiguration: WKWebViewConfiguration?
    if !FeatureList.kUseProfileWebViewConfiguration.enabled {
      initialConfiguration =
        parentTab.isPrivate
        ? TabManager.privateConfiguration : TabManager.defaultConfiguration
    }
    let tab = TabStateFactory.create(
      with: .init(profile: parentTab.profile, initialConfiguration: initialConfiguration)
    )
    if !FeatureList.kUseProfileWebViewConfiguration.enabled {
      tab.browserData = TabBrowserData(tab: tab)
    }
    tab.addObserver(toolbarViewModel)
    tab.addObserver(self)
    tab.createWebView()
    tab.delegate = self
    let braveShieldsTabHelper: BraveShieldsTabHelper = .init(
      tab: tab,
      braveShieldsSettings: BraveShieldsSettingsServiceFactory.get(profile: tab.profile)
    )
    tab.braveShieldsHelper = braveShieldsTabHelper
    tab.addPolicyDecider(braveShieldsTabHelper)
    tab.webViewProxy?.scrollView?.layer.masksToBounds = true
    tab.isVisible = true
    self.currentTab = tab

    guard let currentTab = currentTab else {
      return
    }

    // Add rule lists for this page
    Task(priority: .userInitiated) {
      let isBraveShieldsEnabled = braveShieldsTabHelper.isBraveShieldsEnabled(for: url)
      let shieldLevel = braveShieldsTabHelper.shieldLevel(for: url, considerAllShieldsOption: true)
      let ruleLists = await AdBlockGroupsManager.shared.ruleLists(
        isBraveShieldsEnabled: isBraveShieldsEnabled,
        shieldLevel: shieldLevel
      )
      for ruleList in ruleLists {
        currentTab.configuration?.userContentController.add(ruleList)
      }
    }

    setupUI()

    currentTab.loadRequest(URLRequest(url: url))
  }

  private func updateViewModel() {
    toolbarViewModel.onActionButton = { [weak self] button in
      switch button {
      case .close:
        self?.dismiss(animated: true)
      case .back:
        guard let currentTab = self?.currentTab else { return }
        currentTab.goBack()
      case .forward:
        guard let currentTab = self?.currentTab else { return }
        currentTab.goForward()
      case .readerMode:
        self?.toggleReaderMode()
      case .shield, .refresh, .playlist,
        .translate, .share, .openTab:
        break
      }
    }
  }

  private func setupUI() {
    guard let currentTab = currentTab else {
      return
    }

    view.backgroundColor = privateBrowsingManager.browserColors.chromeBackground
    view.addSubview(currentTab.view)

    toolbarHostingController.view.backgroundColor = .clear
    addChild(toolbarHostingController)
    view.addSubview(toolbarHostingController.view)
    toolbarHostingController.didMove(toParent: self)

    currentTab.view.snp.makeConstraints {
      $0.top.equalTo(view.safeAreaLayoutGuide.snp.top)
      $0.leading.trailing.equalTo(view)
      $0.bottom.equalTo(toolbarHostingController.view.snp.top)
    }
    toolbarHostingController.view.snp.makeConstraints {
      $0.leading.trailing.equalTo(view)
      $0.bottom.equalTo(view.safeAreaLayoutGuide.snp.bottom)
    }
  }

  private func toggleReaderMode() {
    guard let tab = currentTab else { return }
    let isActive: Bool
    if FeatureList.kUseProfileWebViewConfiguration.enabled {
      isActive = tab.readerMode?.state == .active
    } else {
      isActive = readerModeHandler?.state == .active
    }
    isActive ? disableReaderMode() : enableReaderMode()
  }

  private func enableReaderMode() {
    guard let tab = currentTab,
      let backForwardList = tab.backForwardList,
      let currentURL = backForwardList.currentItem?.url,
      !InternalURL.isValid(url: currentURL)
    else { return }

    let headers =
      (tab.responses?[currentURL] as? HTTPURLResponse)?.allHeaderFields as? [String: String]
    guard
      let readerModeURL = currentURL.encodeEmbeddedInternalURL(for: .readermode, headers: headers)
    else { return }

    let readerModeCache = ReaderModeScriptHandler.cache(for: tab)

    if FeatureList.kUseProfileWebViewConfiguration.enabled {
      if let readabilityResult = tab.readerMode?.readabilityResult {
        Task { @MainActor in
          try? await readerModeCache.put(currentURL, readabilityResult)
          tab.loadRequest(PrivilegedRequest(url: readerModeURL) as URLRequest)
        }
      }
    } else {
      tab.evaluateJavaScript(
        functionName: "\(readerModeNamespace).readerize",
        contentWorld: ReaderModeScriptHandler.scriptSandbox
      ) { [weak tab] (object, error) in
        guard let tab else { return }
        if let readabilityResult = ReadabilityResult(object: object as AnyObject?) {
          Task { @MainActor in
            try? await readerModeCache.put(currentURL, readabilityResult)
            tab.loadRequest(PrivilegedRequest(url: readerModeURL) as URLRequest)
          }
        }
      }
    }
  }

  private func disableReaderMode() {
    guard let tab = currentTab,
      let backForwardList = tab.backForwardList,
      let currentURL = backForwardList.currentItem?.url,
      let originalURL = currentURL.decodeEmbeddedInternalURL(for: .readermode)
    else { return }

    tab.loadRequest(URLRequest(url: originalURL))
  }

  private func checkReaderMode(for tab: some TabState) {
    guard let url = tab.visibleURL,
      !url.isNewTabURL,
      !InternalURL.isValid(url: url) || url.isInternalURL(for: .readermode),
      !url.isFileURL
    else { return }

    if FeatureList.kUseProfileWebViewConfiguration.enabled {
      if let readerMode = tab.readerMode {
        Task { @MainActor [weak self] in
          await readerMode.checkReadability()
          self?.toolbarViewModel.updateReaderModeState(readerMode.state)
        }
      }
    } else {
      tab.evaluateJavaScript(
        functionName: "\(readerModeNamespace).checkReadability",
        contentWorld: ReaderModeScriptHandler.scriptSandbox
      )
    }
  }
}

// MARK: - TabDelegate
extension QuickViewController: TabDelegate {
  func tab(
    _ tab: some TabState,
    createNewTabWithRequest request: URLRequest,
    isUserInitiated: Bool
  ) -> (any TabState)? {
    // window.open should open in a regular tab
    dismiss(animated: true) { [weak self] in
      self?.onOpenInNewTab?(request)
    }
    return nil
  }
}

// MARK: - TabObserver
extension QuickViewController: TabObserver {
  func tabDidCreateWebView(_ tab: some TabState) {
    if FeatureList.kUseProfileWebViewConfiguration.enabled {
      tab.readerMode = .init(tab: tab)
    } else {
      let handler = ReaderModeScriptHandler()
      readerModeHandler = handler
      tab.browserData?.addContentScript(
        handler,
        name: ReaderModeScriptHandler.scriptName,
        contentWorld: ReaderModeScriptHandler.scriptSandbox
      )
      readerModeHandler?.delegate = toolbarViewModel
    }
  }

  func tabDidFinishNavigation(_ tab: some TabState) {
    checkReaderMode(for: tab)
  }

  func tabDidTitleChange(_ tab: some TabState) {
    checkReaderMode(for: tab)
  }

  func tabDidUpdateURL(_ tab: some TabState) {
    checkReaderMode(for: tab)
  }

  func tabWillBeDestroyed(_ tab: some TabState) {
    tab.removeObserver(self)
  }

  func tab(_ tab: some TabState, frameDidBecomeAvailable frame: WebFrame) {
    guard frame.isMainFrame else { return }
    checkReaderMode(for: tab)
  }
}
