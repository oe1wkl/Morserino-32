//
//  ContentView.swift
//  Four tabs: the web Config Tool, the web M32 Trainer, a native link test,
//  and help.
//
//  The link test exists to answer the one question this whole approach turns
//  on — is BLE fast enough to be pleasant? — without the web tool in the way.
//  Run it first on a new device.
//
//  All tabs share ONE Bluetooth link (the firmware accepts one central), and
//  only one of them may drive it at a time: see M32BleTransport.driver.
//

import SwiftUI

struct ContentView: View {
    @ObservedObject var transport: M32BleTransport
    @StateObject private var handover: LinkHandover

    private enum Tab: Hashable { case config, trainer, linkTest, help }
    @State private var tab: Tab = .config

    init(transport: M32BleTransport) {
        self.transport = transport
        _handover = StateObject(wrappedValue: LinkHandover(transport: transport))
    }

    var body: some View {
        TabView(selection: $tab) {
            WebPageTab(transport: transport, handover: handover, page: .config)
                .tabItem { Label("Config", systemImage: "slider.horizontal.3") }
                .tag(Tab.config)

            WebPageTab(transport: transport, handover: handover, page: .trainer)
                .tabItem { Label("Trainer", systemImage: "graduationcap") }
                .tag(Tab.trainer)

            LinkTestTab(transport: transport)
                .tabItem { Label("Link test", systemImage: "waveform.path.ecg") }
                .tag(Tab.linkTest)

            // Someone who found this in the App Store has no way to guess that a
            // preference has to be switched on with the encoder before anything
            // can happen. A visible tab, not a button hidden behind a connection.
            HelpView()
                .tabItem { Label("Help", systemImage: "questionmark.circle") }
                .tag(Tab.help)
        }
        // The link follows the web tab on screen (LinkHandover.swift).
        .onChange(of: tab) { handover.show(role: Self.role(of: $0)) }
        .onAppear { handover.show(role: Self.role(of: tab)) }
        // A copy session can run for many minutes with nobody touching the
        // screen (keyed answers come from the paddle). Auto-Lock would suspend
        // the app and drop the Bluetooth link mid-item, so keep the screen on
        // while the Trainer holds the link, and only then.
        .onChange(of: trainerConnected) { UIApplication.shared.isIdleTimerDisabled = $0 }
    }

    private static func role(of tab: Tab) -> String? {
        switch tab {
        case .config: return WebPage.config.role
        case .trainer: return WebPage.trainer.role
        case .linkTest, .help: return nil
        }
    }

    private var trainerConnected: Bool {
        transport.driver == WebPage.trainer.role && transport.state.isConnected
    }
}

// MARK: - The web pages

private struct WebPageTab: View {
    @ObservedObject var transport: M32BleTransport
    @ObservedObject var handover: LinkHandover
    let page: WebPage

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Circle()
                    .fill(ownsLink ? Color.green : Color.secondary.opacity(0.4))
                    .frame(width: 8, height: 8)
                Text(statusText)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Spacer()
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 6)
            .background(.bar)

            WebToolView(transport: transport, handover: handover, page: page)
                .ignoresSafeArea(.container, edges: .bottom)
        }
    }

    private var ownsLink: Bool { transport.state.isConnected && transport.driver == page.role }

    /// The link is shared: say so when another tab holds it, rather than show
    /// a green "connected" above a page that is not.
    private var statusText: String {
        if let holder = handover.waitingFor, holder != page.role {
            return "Waiting for the \(holder) tab to finish…"
        }
        if let holder = transport.driver, holder != page.role, transport.state != .idle {
            return "In use by the \(holder) tab: \(transport.state.label)"
        }
        return transport.state.label
    }
}

// MARK: - Native link test

private struct LinkTestTab: View {
    @ObservedObject var transport: M32BleTransport

    @State private var lines: [String] = []
    @State private var running = false

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 12) {
                Text("Connects on its own, runs a handshake and two reads, and times "
                   + "them. Disconnect the Config and Trainer tabs first — only one side "
                   + "may drive the conversation at a time.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal)

                Button(running ? "Running…" : "Run link test") {
                    Task { await runTest() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(running || transport.state.isConnected || transport.driver != nil)
                .padding(.horizontal)

                List(Array(lines.enumerated()), id: \.offset) { _, line in
                    Text(line)
                        .font(.system(.footnote, design: .monospaced))
                        .textSelection(.enabled)
                }
                .listStyle(.plain)
            }
            .padding(.top, 8)
            .navigationTitle("Link test")
        }
    }

    @MainActor
    private func runTest() async {
        guard transport.claim("Link test") else { return }
        running = true
        lines = []
        defer { running = false; transport.release("Link test") }

        let client = M32Client(transport: transport)
        client.attach()
        defer { client.detach() }

        do {
            emit("Scanning…")
            let name = try await timed("connect") { try await transport.connect() }
            emit("  device: \(name)")

            let device = try await timed("put device/protocol/on") { try await client.handshake() }
            if let info = device["device"] as? [String: Any] {
                emit("  hardware:  \(info["hardware"] as? String ?? "?")")
                emit("  firmware:  \(info["firmware"] as? String ?? "?")")
                emit("  protocol:  \(info["protocol"] as? String ?? "?")")
                // Absent means a build older than 56d21e9, not necessarily a
                // pre-V9 one — device.edition landed inside the V9 cycle.
                emit("  edition:   \(info["edition"] as? String ?? "— (not reported by this build)")")
            }

            _ = try await timed("get hardware") { try await client.request("get hardware") }

            // The big one: this is the reply that tells you whether BLE is
            // pleasant or merely possible.
            let configs = try await timed("get configs") {
                try await client.request("get configs", timeout: 20, retries: 1)
            }
            if let list = configs["configs"] as? [Any] {
                emit("  \(list.count) preferences read")
            }

            await client.farewell()
            transport.disconnect()
            emit("Done — disconnected.")

        } catch {
            emit("FAILED: \(error.localizedDescription)")
            transport.disconnect()
        }
    }

    @MainActor
    private func timed<T>(_ label: String, _ work: () async throws -> T) async rethrows -> T {
        let start = Date()
        let result = try await work()
        emit(String(format: "%@ — %.2f s", label, Date().timeIntervalSince(start)))
        return result
    }

    @MainActor
    private func emit(_ line: String) { lines.append(line) }
}
