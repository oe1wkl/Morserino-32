//
//  BundleSchemeHandler.swift
//  Serves the bundled web tool under m32app://local/…
//
//  Why not just loadFileURL? Because WKWebView treats a file:// page as an
//  opaque origin, and the tool does `fetch('m32_pref_help.json')` to load its
//  preference help texts — that fetch fails on file://. A custom scheme gives
//  the page a normal origin, so the tool runs exactly as it does in a browser.
//

import Foundation
import WebKit
import UniformTypeIdentifiers

// MARK: - Where the bundled web assets live

enum BundleWeb {
    /// A custom scheme rather than file:// — WKWebView gives file:// pages an
    /// opaque origin, and the tool's `fetch('m32_pref_help.json')` would fail.
    static let scheme = "m32app"
    static let host = "local"
    static let directory = "Web"

    static func url(for name: String) -> URL {
        URL(string: "\(scheme)://\(host)/\(name)")!
    }

    /// `name` may name a file in a subfolder of Web, e.g. "vendor/pdf.min.js".
    static func fileURL(named name: String) -> URL? {
        let parts = name.split(separator: "/").map(String.init)
        guard let file = parts.last else { return nil }
        let subdirectory = ([directory] + parts.dropLast()).joined(separator: "/")
        return Bundle.main.url(forResource: file, withExtension: nil, subdirectory: subdirectory)
    }

    static func string(named name: String) -> String? {
        guard let url = fileURL(named: name) else { return nil }
        return try? String(contentsOf: url, encoding: .utf8)
    }
}

// MARK: - No remote loads

/// The app makes no network requests (a promise in the App Review notes). The
/// pages' copies are pointed at the libraries and the font bundled in Web/vendor
/// (sync-webtool.sh, which also fails the build on a new remote load); this
/// content rule list is the safety net that blocks anything still asking the
/// web for a script, style sheet, font, image, fetch or the like. Links the user
/// taps are navigations, not blocked here: WebToolView opens them in Safari.
enum NoRemoteLoads {
    private static let identifier = "no-remote-loads-1"
    private static let rules = """
        [{"trigger": {"url-filter": "^https?://",
                      "resource-type": ["image", "style-sheet", "script", "font", "raw",
                                        "svg-document", "media", "ping"]},
          "action": {"type": "block"}}]
        """

    private static var compiled: WKContentRuleList?
    private static var waiting: [(WKContentRuleList?) -> Void] = []

    /// Hands over the compiled list (nil if it failed to compile), compiling it
    /// on the first call. Main thread only.
    static func withList(_ done: @escaping (WKContentRuleList?) -> Void) {
        if let compiled { done(compiled); return }
        waiting.append(done)
        guard waiting.count == 1 else { return }       // already compiling
        WKContentRuleListStore.default().compileContentRuleList(
            forIdentifier: identifier, encodedContentRuleList: rules) { list, error in
            DispatchQueue.main.async {
                if let error { print("NoRemoteLoads: \(error)") }
                compiled = list
                let callbacks = waiting
                waiting = []
                callbacks.forEach { $0(list) }
            }
        }
    }
}

// MARK: - Scheme handler

final class BundleSchemeHandler: NSObject, WKURLSchemeHandler {

    func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
        guard let url = task.request.url else {
            task.didFailWithError(URLError(.badURL))
            return
        }

        // Strip the leading slash and refuse anything trying to climb out of
        // the bundle directory.
        let name = String(url.path.dropFirst())
        guard !name.isEmpty, !name.contains(".."), let fileURL = BundleWeb.fileURL(named: name) else {
            task.didFailWithError(URLError(.fileDoesNotExist))
            return
        }

        do {
            let data = try Data(contentsOf: fileURL)
            // An HTTPURLResponse with status 200, not a plain URLResponse: for the
            // latter fetch() reports status 0 and ok === false, and the trainer
            // (rightly, on the web) treats !ok as a failure — its word list did not
            // load in the app ("HTTP 0", found on the 1.3 screenshots, 2026-11-27).
            let response = HTTPURLResponse(url: url, statusCode: 200, httpVersion: "HTTP/1.1",
                                           headerFields: [
                                               "Content-Type": Self.mimeType(for: fileURL) + "; charset=utf-8",
                                               "Content-Length": String(data.count),
                                           ])!
            task.didReceive(response)
            task.didReceive(data)
            task.didFinish()
        } catch {
            task.didFailWithError(error)
        }
    }

    func webView(_ webView: WKWebView, stop task: WKURLSchemeTask) { }

    private static func mimeType(for url: URL) -> String {
        switch url.pathExtension.lowercased() {
        case "html", "htm": return "text/html"
        case "js": return "text/javascript"
        case "json": return "application/json"
        case "css": return "text/css"
        case "svg": return "image/svg+xml"
        case "png": return "image/png"
        case "woff2": return "font/woff2"
        default:
            return UTType(filenameExtension: url.pathExtension)?.preferredMIMEType
                ?? "application/octet-stream"
        }
    }
}
