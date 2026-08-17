//
//  ContentView.swift
//  Two tabs: the web tool itself, and a native link test.
//
//  The link test exists to answer the one question this whole approach turns
//  on — is BLE fast enough to be pleasant? — without the web tool in the way.
//  Run it first on a new device.
//

import SwiftUI

struct ContentView: View {
    @ObservedObject var transport: M32BleTransport

    var body: some View {
        TabView {
            ConfigTab(transport: transport)
                .tabItem { Label("Config", systemImage: "slider.horizontal.3") }

            LinkTestTab(transport: transport)
                .tabItem { Label("Link test", systemImage: "waveform.path.ecg") }
        }
    }
}

// MARK: - The web tool

private struct ConfigTab: View {
    @ObservedObject var transport: M32BleTransport

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Circle()
                    .fill(transport.state.isConnected ? Color.green : Color.secondary.opacity(0.4))
                    .frame(width: 8, height: 8)
                Text(transport.state.label)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Spacer()
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 6)
            .background(.bar)

            WebToolView(transport: transport)
                .ignoresSafeArea(.container, edges: .bottom)
        }
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
                   + "them. Disconnect the Config tab first — only one side may drive "
                   + "the conversation at a time.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal)

                Button(running ? "Running…" : "Run link test") {
                    Task { await runTest() }
                }
                .buttonStyle(.borderedProminent)
                .disabled(running || transport.state.isConnected)
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
        running = true
        lines = []
        defer { running = false }

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
