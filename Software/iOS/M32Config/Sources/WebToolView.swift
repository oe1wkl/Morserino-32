//
//  WebToolView.swift
//  Hosts one of the bundled web pages and wires its transport to BLE:
//  m32_config_tool.html (the Config tab) or m32_trainer.html (the Trainer tab).
//
//  Neither page is modified for iOS — each stays the single source of truth
//  for both the browser and this app. All that changes is where its bytes go,
//  and a small bridge script does that: `Web/bridge.js` for the Config Tool,
//  `Web/trainer-bridge.js` for the Trainer. See those files for how little the
//  pages actually have to be told.
//

import SwiftUI
import WebKit

/// The pages this app shows. Each one drives the link under its own name, so
/// the two tabs cannot talk into the same session (see `M32BleTransport.driver`).
enum WebPage {
    case config
    case trainer

    var file: String {
        switch self {
        case .config: return "m32_config_tool.html"
        case .trainer: return "m32_trainer.html"
        }
    }

    var bridge: String {
        switch self {
        case .config: return "bridge.js"
        case .trainer: return "trainer-bridge.js"
        }
    }

    /// bridge.js replaces the Config Tool's own globals, so it runs after the
    /// page's scripts. The Trainer instead looks for `window.M32Host` while its
    /// scripts run, so that has to be in place before them.
    var injectionTime: WKUserScriptInjectionTime {
        switch self {
        case .config: return .atDocumentEnd
        case .trainer: return .atDocumentStart
        }
    }

    /// The tab's name, as the other tab tells the user who holds the link.
    var role: String {
        switch self {
        case .config: return "Config"
        case .trainer: return "Trainer"
        }
    }
}

struct WebToolView: UIViewRepresentable {

    let transport: M32BleTransport
    let handover: LinkHandover
    var page: WebPage = .config

    func makeCoordinator() -> Coordinator {
        Coordinator(transport: transport, handover: handover, role: page.role, pageFile: page.file)
    }

    func makeUIView(context: Context) -> WKWebView {
        let controller = WKUserContentController()
        controller.add(context.coordinator, name: Coordinator.messageName)

        if let bridge = BundleWeb.string(named: page.bridge) {
            // The classic initialiser injects into the PAGE content world, which
            // is what we need: the shim has to see and replace the tool's own
            // globals (`writer`, `doConnect`, `readBuffer`), or put M32Host where
            // the trainer's own scripts find it. An isolated world would run
            // without errors and do nothing at all.
            controller.addUserScript(WKUserScript(source: bridge,
                                                  injectionTime: page.injectionTime,
                                                  forMainFrameOnly: true))
        }

        let config = WKWebViewConfiguration()
        config.userContentController = controller
        config.setURLSchemeHandler(BundleSchemeHandler(), forURLScheme: BundleWeb.scheme)

        let webView = WKWebView(frame: .zero, configuration: config)
        webView.isOpaque = true
        webView.scrollView.contentInsetAdjustmentBehavior = .always
        // Without a UI delegate WKWebView answers every confirm() with Cancel
        // and swallows every alert(), with no error. The tool guards Recall,
        // Clear, Import, factory reset, file delete and more with confirm(), so
        // up to 1.2 those buttons silently did nothing in the app.
        webView.uiDelegate = context.coordinator
        // Links out of the page (the user manual, the word list's licence) open
        // in Safari: followed in here they would replace the page, with no way
        // back, and a target="_blank" link would do nothing at all.
        webView.navigationDelegate = context.coordinator
        context.coordinator.webView = webView

        webView.load(URLRequest(url: BundleWeb.url(for: page.file)))
        return webView
    }

    func updateUIView(_ webView: WKWebView, context: Context) { }

    // MARK: - Coordinator: the JS ⇄ CoreBluetooth bridge

    final class Coordinator: NSObject, WKScriptMessageHandler, WKUIDelegate, WKNavigationDelegate, LinkHandoverPage {

        static let messageName = "m32"

        private let transport: M32BleTransport
        private let handover: LinkHandover
        private let role: String
        private let pageFile: String
        private var tokens: [UUID] = []
        weak var webView: WKWebView?

        /// Whether this page holds the link. Bytes, logs and a link loss go
        /// only to the page that holds it: the other one is not connected and
        /// would take them for its own (the Config Tool's TTY would show the
        /// Trainer's traffic, and its link-lost notice would pop up for nothing).
        private var holdsLink: Bool { transport.driver == role }

        /// The bridge has installed itself in the loaded page (it says "ready").
        /// Before that, an attach() has nobody to talk to and waits here.
        private var pageReady = false
        private var attachWaiting = false

        init(transport: M32BleTransport, handover: LinkHandover, role: String, pageFile: String) {
            self.transport = transport
            self.handover = handover
            self.role = role
            self.pageFile = pageFile
            super.init()
            handover.register(self, role: role)

            tokens.append(transport.addReceiver { [weak self] data in
                guard let self, self.holdsLink else { return }
                self.deliverToPage(data)
            })
            tokens.append(transport.addLinkLostHandler { [weak self] in
                guard let self, self.holdsLink else { return }
                self.evaluate("window.__m32Native.linkLost()")
            })
            tokens.append(transport.addLogHandler { [weak self] message in
                guard let self, self.holdsLink else { return }
                self.evaluate("window.__m32Native.log(\(Self.jsString(message)))")
            })
        }

        deinit {
            tokens.forEach { transport.removeReceiver($0) }
        }

        // MARK: JS → native

        func userContentController(_ controller: WKUserContentController,
                                   didReceive message: WKScriptMessage) {
            guard let body = message.body as? [String: Any],
                  let id = body["id"] as? Int,
                  let command = body["cmd"] as? String else { return }

            switch command {
            case "ready":
                pageReady = true
                reply(id, ok: true, payload: "")
                if attachWaiting { attach() }

            case "released":
                reply(id, ok: true, payload: "")
                handover.released(by: role)

            case "handoverCancelled":
                reply(id, ok: true, payload: "")
                handover.cancelled(by: role)

            case "connect":
                guard transport.claim(role) else {
                    let holder = transport.driver ?? "other"
                    reply(id, ok: false, payload: handover.waitingFor == holder
                        ? "The \(holder) tab is finishing what it is doing. The connection comes here by itself."
                        : "The \(holder) tab is connected to the Morserino. "
                        + "Disconnect it there first: only one tab can use the connection at a time.")
                    return
                }
                // This page already holds a live link (it was reloaded, or its
                // web content process restarted): hand it the same link. The
                // page handshakes again, which an open session simply answers.
                if case .connected(let name) = transport.state {
                    reply(id, ok: true, payload: name)
                    return
                }
                Task { @MainActor in
                    do {
                        let name = try await transport.connect()
                        reply(id, ok: true, payload: name)
                    } catch {
                        transport.release(role)
                        reply(id, ok: false, payload: error.localizedDescription)
                    }
                }

            case "disconnect":
                // Only our own link: a page that is not connected must not take
                // the other tab's connection down with it.
                if holdsLink { transport.disconnect() }
                reply(id, ok: true, payload: "")

            case "saveFile":
                // The web tool saves by clicking an <a download>, which a
                // WKWebView silently ignores. On a phone "save a file" means the
                // share sheet, so write it out and hand it to iOS.
                guard let name = body["name"] as? String,
                      let text = body["text"] as? String else {
                    reply(id, ok: false, payload: "saveFile without name or text")
                    return
                }
                // wait: answer when the sheet is closed, and only "ok" if the
                // file went somewhere (the Trainer counts an export as done).
                // Without it, answer at once, as the Config Tool expects.
                presentShareSheet(named: name, text: text, id: id,
                                  waitForResult: body["wait"] as? Bool ?? false)

            case "write":
                guard let text = body["text"] as? String else {
                    reply(id, ok: false, payload: "write without text")
                    return
                }
                guard holdsLink else {
                    reply(id, ok: false, payload: "not connected")
                    return
                }
                // The text already carries the trailing newline that sendLine()
                // appended, so send it raw rather than adding a second one.
                transport.send(raw: Data(text.utf8))
                reply(id, ok: true, payload: "")

            default:
                reply(id, ok: false, payload: "unknown bridge command \"\(command)\"")
            }
        }

        // MARK: Hand-over (LinkHandoverPage)

        func handOver() { evaluate("window.__m32Native.handover()") }

        func cancelHandOver() { evaluate("window.__m32Native.cancelHandover()") }

        func attach() {
            guard pageReady else { attachWaiting = true; return }
            attachWaiting = false
            evaluate("window.__m32Native.attach()")
        }

        /// A reload (or a restarted web content process) starts a new page: it
        /// is not ready until its bridge says so again.
        func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
            pageReady = false
        }

        // MARK: Saving

        private func presentShareSheet(named name: String, text: String, id: Int, waitForResult: Bool) {
            // The name comes from a text field in the page: keep it to a
            // filename so it cannot climb out of the temporary directory.
            let safe = (name as NSString).lastPathComponent
            let filename = safe.isEmpty ? "multipart.txt" : safe
            let url = FileManager.default.temporaryDirectory.appendingPathComponent(filename)

            do {
                try text.write(to: url, atomically: true, encoding: .utf8)
            } catch {
                reply(id, ok: false, payload: error.localizedDescription)
                return
            }

            guard let presenter = frontmostController() else {
                reply(id, ok: false, payload: "no window to present from")
                return
            }

            let sheet = UIActivityViewController(activityItems: [url], applicationActivities: nil)
            // An iPad presents this as a popover and traps without an anchor.
            if let host = webView {
                sheet.popoverPresentationController?.sourceView = host
                sheet.popoverPresentationController?.sourceRect =
                    CGRect(x: host.bounds.midX, y: host.bounds.midY, width: 0, height: 0)
            }
            if waitForResult {
                sheet.completionWithItemsHandler = { [weak self] _, completed, _, error in
                    if completed { self?.reply(id, ok: true, payload: filename) }
                    else { self?.reply(id, ok: false, payload: error?.localizedDescription ?? "cancelled") }
                }
            }
            presenter.present(sheet, animated: true)
            if !waitForResult { reply(id, ok: true, payload: filename) }
        }

        /// Present from whatever is frontmost: a share sheet or another dialog
        /// may already be up, and presenting on top of a presenting controller
        /// throws the new one away silently.
        private func frontmostController() -> UIViewController? {
            guard var presenter = webView?.window?.rootViewController else { return nil }
            while let above = presenter.presentedViewController { presenter = above }
            return presenter
        }

        // MARK: Links out of the page (WKNavigationDelegate, WKUIDelegate)

        func webView(_ webView: WKWebView, decidePolicyFor action: WKNavigationAction,
                     decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            guard let url = action.request.url, let scheme = url.scheme?.lowercased() else {
                decisionHandler(.cancel)
                return
            }
            switch scheme {
            case BundleWeb.scheme:
                // This tab's own page only: the other bundled page loaded in
                // here would come up without its bridge (a link from the Config
                // Tool to the Trainer, say). It has its own tab.
                let mainFrame = action.targetFrame?.isMainFrame ?? true
                decisionHandler(!mainFrame || url.lastPathComponent == pageFile ? .allow : .cancel)
            case "about", "blob", "data":
                decisionHandler(.allow)
            case "http", "https", "mailto":
                // Only what the user tapped: a page's own subresource loads
                // never come through here as a main-frame link activation.
                if action.navigationType == .linkActivated || action.targetFrame == nil {
                    UIApplication.shared.open(url)
                }
                decisionHandler(.cancel)
            default:
                decisionHandler(.cancel)
            }
        }

        /// target="_blank": WebKit asks for a new web view. Open the link in
        /// Safari instead and give it none.
        func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration,
                     for action: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
            if let url = action.request.url, ["http", "https", "mailto"].contains(url.scheme?.lowercased() ?? "") {
                UIApplication.shared.open(url)
            }
            return nil
        }

        // MARK: JavaScript dialogs (WKUIDelegate)
        //
        // The page's alert() / confirm() / prompt() as native alerts. Every
        // completion handler must be called exactly once, or WebKit raises an
        // exception; with no window to present from, answer at once (alert: OK,
        // confirm: Cancel, prompt: nil), which is what the page got before.

        func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
                     initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
            guard let presenter = frontmostController() else { completionHandler(); return }
            let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in completionHandler() })
            presenter.present(alert, animated: true)
        }

        func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
                     initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
            guard let presenter = frontmostController() else { completionHandler(false); return }
            let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in completionHandler(false) })
            alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in completionHandler(true) })
            presenter.present(alert, animated: true)
        }

        func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String,
                     defaultText: String?, initiatedByFrame frame: WKFrameInfo,
                     completionHandler: @escaping (String?) -> Void) {
            guard let presenter = frontmostController() else { completionHandler(nil); return }
            let alert = UIAlertController(title: nil, message: prompt, preferredStyle: .alert)
            alert.addTextField { $0.text = defaultText }
            alert.addAction(UIAlertAction(title: "Cancel", style: .cancel) { _ in completionHandler(nil) })
            alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak alert] _ in
                completionHandler(alert?.textFields?.first?.text ?? "")
            })
            presenter.present(alert, animated: true)
        }

        // MARK: native → JS

        private func deliverToPage(_ data: Data) {
            // Base64 across the bridge, decoded in JS with a streaming
            // TextDecoder: a notification may split a multi-byte sequence and
            // {stream:true} holds the tail until it is complete.
            evaluate("window.__m32Native.rx('\(data.base64EncodedString())')")
        }

        private func reply(_ id: Int, ok: Bool, payload: String) {
            evaluate("window.__m32Native.reply(\(id), \(ok), \(Self.jsString(payload)))")
        }

        private func evaluate(_ js: String) {
            guard let webView else { return }
            webView.evaluateJavaScript(js, completionHandler: nil)
        }

        /// Quote a Swift string as a JavaScript string literal, safely.
        private static func jsString(_ value: String) -> String {
            let encoded = (try? JSONSerialization.data(withJSONObject: [value]))
                .flatMap { String(data: $0, encoding: .utf8) }
            // JSONSerialization gives us ["…"]; strip the array brackets.
            guard let encoded, encoded.count >= 2 else { return "\"\"" }
            return String(encoded.dropFirst().dropLast())
        }
    }
}
