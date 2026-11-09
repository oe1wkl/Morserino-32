//
//  WebToolView.swift
//  Hosts the existing m32_config_tool.html and wires its transport to BLE.
//
//  The web tool is NOT modified for iOS — it stays the single source of truth
//  for both the browser and this app. All that changes is where its bytes go,
//  and `Web/bridge.js` does that in about ninety lines. See that file for how
//  little the tool actually has to be told.
//

import SwiftUI
import WebKit

struct WebToolView: UIViewRepresentable {

    let transport: M32BleTransport

    func makeCoordinator() -> Coordinator { Coordinator(transport: transport) }

    func makeUIView(context: Context) -> WKWebView {
        let controller = WKUserContentController()
        controller.add(context.coordinator, name: Coordinator.messageName)

        if let bridge = BundleWeb.string(named: "bridge.js") {
            // The classic initialiser injects into the PAGE content world, which
            // is what we need: the shim has to see and replace the tool's own
            // globals (`writer`, `doConnect`, `readBuffer`). An isolated world
            // would run without errors and do nothing at all.
            controller.addUserScript(WKUserScript(source: bridge,
                                                  injectionTime: .atDocumentEnd,
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
        context.coordinator.webView = webView

        webView.load(URLRequest(url: BundleWeb.url(for: "m32_config_tool.html")))
        return webView
    }

    func updateUIView(_ webView: WKWebView, context: Context) { }

    // MARK: - Coordinator: the JS ⇄ CoreBluetooth bridge

    final class Coordinator: NSObject, WKScriptMessageHandler, WKUIDelegate {

        static let messageName = "m32"

        private let transport: M32BleTransport
        private var receiverToken: UUID?
        weak var webView: WKWebView?

        init(transport: M32BleTransport) {
            self.transport = transport
            super.init()

            receiverToken = transport.addReceiver { [weak self] data in
                self?.deliverToPage(data)
            }
            transport.onLinkLost = { [weak self] in
                self?.evaluate("window.__m32Native.linkLost()")
            }
            transport.onLog = { [weak self] message in
                self?.evaluate("window.__m32Native.log(\(Self.jsString(message)))")
            }
        }

        deinit {
            if let token = receiverToken { transport.removeReceiver(token) }
        }

        // MARK: JS → native

        func userContentController(_ controller: WKUserContentController,
                                   didReceive message: WKScriptMessage) {
            guard let body = message.body as? [String: Any],
                  let id = body["id"] as? Int,
                  let command = body["cmd"] as? String else { return }

            switch command {
            case "connect":
                Task { @MainActor in
                    do {
                        let name = try await transport.connect()
                        reply(id, ok: true, payload: name)
                    } catch {
                        reply(id, ok: false, payload: error.localizedDescription)
                    }
                }

            case "disconnect":
                transport.disconnect()
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
                presentShareSheet(named: name, text: text, id: id)

            case "write":
                guard let text = body["text"] as? String else {
                    reply(id, ok: false, payload: "write without text")
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

        // MARK: Saving

        private func presentShareSheet(named name: String, text: String, id: Int) {
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
            presenter.present(sheet, animated: true)
            reply(id, ok: true, payload: filename)
        }

        /// Present from whatever is frontmost: a share sheet or another dialog
        /// may already be up, and presenting on top of a presenting controller
        /// throws the new one away silently.
        private func frontmostController() -> UIViewController? {
            guard var presenter = webView?.window?.rootViewController else { return nil }
            while let above = presenter.presentedViewController { presenter = above }
            return presenter
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
