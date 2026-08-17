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
        context.coordinator.webView = webView

        webView.load(URLRequest(url: BundleWeb.url(for: "m32_config_tool.html")))
        return webView
    }

    func updateUIView(_ webView: WKWebView, context: Context) { }

    // MARK: - Coordinator: the JS ⇄ CoreBluetooth bridge

    final class Coordinator: NSObject, WKScriptMessageHandler {

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
