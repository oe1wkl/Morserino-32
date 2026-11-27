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

    static func fileURL(named name: String) -> URL? {
        Bundle.main.url(forResource: name, withExtension: nil, subdirectory: directory)
    }

    static func string(named name: String) -> String? {
        guard let url = fileURL(named: name) else { return nil }
        return try? String(contentsOf: url, encoding: .utf8)
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
        default:
            return UTType(filenameExtension: url.pathExtension)?.preferredMIMEType
                ?? "application/octet-stream"
        }
    }
}
