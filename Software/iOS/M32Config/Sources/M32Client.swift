//
//  M32Client.swift
//  Request/response for the M32 serial protocol, on top of M32BleTransport.
//
//  The web tool does this in JavaScript (`sendAndParse` / `waitForResponse`),
//  so the WKWebView path does NOT use this file. It exists because it is the
//  piece that is genuinely fiddly to get right, and because anything native —
//  the diagnostics screen here, or a future SwiftUI replacement for the web
//  tool — needs it.
//
//  Two firmware constraints shape the design; see devdocs/ble-serial/DESIGN.md:
//
//  * `bleSerialEvent()` dispatches EXACTLY ONE completed line per poll of
//    loop(). Commands must therefore be issued strictly one at a time, each
//    awaited before the next. `gate` enforces that.
//  * A multi-KB reply can be torn (documented protocol behaviour — the client
//    is expected to re-issue the GET). Hence `retries`.
//

import Foundation

// MARK: - Serialisation gate

/// A one-slot async semaphore: only one request may be in flight at a time.
/// An `actor` alone would not do — actors are reentrant, so a method that
/// awaits inside can interleave with the next caller.
private actor RequestGate {
    private var busy = false
    private var waiting: [CheckedContinuation<Void, Never>] = []

    func acquire() async {
        if !busy {
            busy = true
            return
        }
        await withCheckedContinuation { waiting.append($0) }
    }

    func release() {
        if waiting.isEmpty {
            busy = false
        } else {
            waiting.removeFirst().resume()
        }
    }
}

enum M32ProtocolError: LocalizedError {
    case timeout(command: String, partial: String)
    case notJSON(String)

    var errorDescription: String? {
        switch self {
        case .timeout(let command, let partial):
            return partial.isEmpty
                ? "No answer to \"\(command)\"."
                : "Incomplete answer to \"\(command)\" (got: \(partial.prefix(80))…)"
        case .notJSON(let text):
            return "Could not parse the answer: \(text.prefix(120))"
        }
    }
}

// MARK: - Client

@MainActor
final class M32Client {

    private let transport: M32BleTransport
    private let gate = RequestGate()

    private var receiverToken: UUID?
    private var pendingBytes = Data()
    private var textBuffer = ""
    private var waiter: CheckedContinuation<String, Never>?

    init(transport: M32BleTransport) {
        self.transport = transport
    }

    /// Start consuming bytes. Call before the first request, and `detach()`
    /// when finished, so the web-tool bridge is the only reader again.
    func attach() {
        guard receiverToken == nil else { return }
        receiverToken = transport.addReceiver { [weak self] data in
            self?.ingest(data)
        }
    }

    func detach() {
        if let token = receiverToken { transport.removeReceiver(token) }
        receiverToken = nil
        pendingBytes.removeAll()
        textBuffer = ""
    }

    // MARK: Requests

    /// `put device/protocol/on` — the handshake. Until this succeeds the
    /// firmware answers nothing else over BLE.
    @discardableResult
    func handshake(timeout: TimeInterval = 5) async throws -> [String: Any] {
        try await request("put device/protocol/on", timeout: timeout, retries: 0)
    }

    func farewell() async {
        _ = try? await request("put device/protocol/off", timeout: 2, retries: 0)
    }

    /// Send one command and wait for one complete JSON object.
    ///
    /// Timeouts are generous compared with USB on purpose: the firmware paces
    /// notifications (at most 2 chunks per poll, fewer than 4 in flight), so a
    /// large reply such as `get configs` arrives noticeably more slowly.
    @discardableResult
    func request(_ command: String,
                 timeout: TimeInterval = 8,
                 retries: Int = 1) async throws -> [String: Any] {
        await gate.acquire()
        defer { Task { await gate.release() } }

        var lastError: Error = M32ProtocolError.timeout(command: command, partial: "")

        for attempt in 0...retries {
            textBuffer = ""
            pendingBytes.removeAll()
            transport.send(line: command)

            if let json = await awaitObject(timeout: timeout) {
                guard let data = json.data(using: .utf8),
                      let object = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                    throw M32ProtocolError.notJSON(json)
                }
                return object
            }

            lastError = M32ProtocolError.timeout(command: command, partial: textBuffer)
            if attempt < retries {
                // A torn reply leaves debris in the buffer; drop it before retrying.
                try? await Task.sleep(nanoseconds: 300_000_000)
            }
        }
        throw lastError
    }

    // MARK: Reply assembly

    private func awaitObject(timeout: TimeInterval) async -> String? {
        if let ready = takeCompleteObject() { return ready }

        let timeoutTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(timeout * 1_000_000_000))
            guard !Task.isCancelled else { return }
            self?.resumeWaiter(with: "")   // Task inherits this @MainActor context
        }
        defer { timeoutTask.cancel() }

        let result = await withCheckedContinuation { (c: CheckedContinuation<String, Never>) in
            waiter = c
        }
        return result.isEmpty ? nil : result
    }

    private func resumeWaiter(with value: String) {
        guard let c = waiter else { return }
        waiter = nil
        c.resume(returning: value)
    }

    private func ingest(_ data: Data) {
        textBuffer += decodeIncrementally(data)
        guard waiter != nil, let object = takeCompleteObject() else { return }
        resumeWaiter(with: object)
    }

    /// A BLE notification can end mid-UTF-8-sequence. Decode the longest valid
    /// prefix and keep the remainder for the next notification.
    private func decodeIncrementally(_ incoming: Data) -> String {
        pendingBytes.append(incoming)

        // A UTF-8 sequence is at most 4 bytes, so at most 3 trailing bytes can
        // be an incomplete tail.
        for back in 0...min(3, pendingBytes.count) {
            let cut = pendingBytes.count - back
            if let text = String(data: pendingBytes.prefix(cut), encoding: .utf8) {
                pendingBytes.removeFirst(cut)
                return text
            }
        }
        // Genuinely invalid bytes (line noise, not a split sequence): drop one
        // so the buffer cannot wedge forever.
        if pendingBytes.count > 4 { pendingBytes.removeFirst() }
        return ""
    }

    /// Pull the first complete `{…}` object out of the buffer.
    ///
    /// Brace counting, but *string-aware*: the web tool's JavaScript version
    /// counts raw braces, so a `}` inside a string value (a CW memory, a file
    /// listing) miscounts. Cheap to get right here, so it is done right here.
    private func takeCompleteObject() -> String? {
        var depth = 0
        var start: String.Index?
        var inString = false
        var escaped = false

        var index = textBuffer.startIndex
        while index < textBuffer.endIndex {
            let ch = textBuffer[index]

            if escaped {
                escaped = false
            } else if inString {
                if ch == "\\" { escaped = true }
                else if ch == "\"" { inString = false }
            } else {
                switch ch {
                case "\"": inString = true
                case "{":
                    if depth == 0 { start = index }
                    depth += 1
                case "}":
                    depth -= 1
                    if depth == 0, let from = start {
                        let object = String(textBuffer[from...index])
                        textBuffer = String(textBuffer[textBuffer.index(after: index)...])
                        return object
                    }
                    if depth < 0 { depth = 0; start = nil }   // stray brace, resynchronise
                default: break
                }
            }
            index = textBuffer.index(after: index)
        }
        return nil
    }
}
