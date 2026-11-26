//
//  M32ConfigApp.swift
//  Morserino (formerly "M32 Config") — an iOS front end for the Morserino-32 serial protocol,
//  carried over BLE (Nordic UART Service) instead of USB.
//

import SwiftUI

@main
struct M32ConfigApp: App {
    @StateObject private var transport = M32BleTransport()

    var body: some Scene {
        WindowGroup {
            ContentView(transport: transport)
        }
    }
}
