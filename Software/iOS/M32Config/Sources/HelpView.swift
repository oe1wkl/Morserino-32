//
//  HelpView.swift
//  What to do when you have just downloaded this and nothing happens.
//
//  Deliberately native rather than a page inside the web tool: it has to be
//  readable before any connection exists (the tool's own help lives behind a
//  connected device), and a SwiftUI view gets VoiceOver for free.
//
//  NOTE on Text(.init(...)): SwiftUI parses markdown in Text only for string
//  LITERALS. Every body string here is built by concatenation, which makes it a
//  runtime String — and Text(String) renders "**bold**" as literal asterisks.
//  Wrapping in .init() makes it a LocalizedStringKey again, which does parse.
//
//  The on-device steps are taken from the user manual's "Preferences" chapter —
//  double click to enter, click to descend into the value, long press to leave.
//  If that interaction ever changes, this text changes with it.
//

import SwiftUI

struct HelpView: View {

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 26) {

                    section("You need a Morserino-32") {
                        Text(.init("This app has nothing to configure on its own. It sets up a "
                           + "**Morserino-32** — an open-source Morse code (CW) training "
                           + "device for radio amateurs — and runs the **M32 Trainer** on it."))
                        Text(.init("The Morserino needs firmware **version 9.0 or newer**. Earlier "
                           + "firmware cannot speak to an app over Bluetooth at all. The "
                           + "Trainer needs **version 10.0 or newer**."))
                        Link("morserino.info", destination: URL(string: "https://www.morserino.info")!)
                    }

                    section("The two controls") {
                        Text("**The encoder knob** — the rotary knob you turn and press.")
                        Text("**The FN button** — the other button on your device.")
                        Text(.init("Named by what they do, not by colour: the classic "
                           + "Morserino labels them black and red, the M32 Pocket does not "
                           + "colour-code its controls at all."))
                            .foregroundStyle(.secondary)
                    }

                    section("Switch Bluetooth on — once") {
                        Text(.init("Out of the box the Morserino's Bluetooth is off. You only have to "
                           + "do this once; it is remembered."))
                        step(1, "At the Morserino's main menu, **double click** the encoder knob "
                              + "to open the preferences.")
                        step(2, "**Turn** the knob until you reach **Bluetooth Use**.")
                        step(3, "**Click once.** The “>” marker moves down to the value.")
                        step(4, "**Turn** to **BLE Serial**.")
                        step(5, "**Click once** to confirm, then **press and hold** to leave "
                              + "the preferences.")
                        step(6, "You are back at the main menu, and Bluetooth is now on.")
                        Text(.init("The setting takes effect when you return to the main menu, which "
                           + "is exactly what step 6 does."))
                            .foregroundStyle(.secondary)
                    }

                    section("Connect") {
                        step(1, "Leave the Morserino sitting at its **main menu**.")
                        step(2, "Tap **Connect** on the Config tab (or on the Trainer tab).")
                        step(3, "The Morserino asks **“Allow connect?”** — press the **FN button** "
                              + "on the device to allow it.")
                        Text(.init("Asking is deliberate. Anything within radio range can reach a "
                           + "Bluetooth device, so the Morserino wants your say-so before it "
                           + "hands over control. Doing nothing for about 20 seconds refuses."))
                            .foregroundStyle(.secondary)
                    }

                    section("The Trainer tab") {
                        Text(.init("Copy practice: the Morserino plays Morse, you copy it — typed "
                           + "here or keyed back on the paddle — and the Trainer checks your copy "
                           + "and decides what comes next. It starts the Morserino's CW Keyer by "
                           + "itself; the Morserino never keys a transmitter for it."))
                        Text(.init("The Config and Trainer tabs share one Bluetooth connection, and it "
                           + "**follows the tab you look at**: connect once, and switching between "
                           + "the two tabs takes the connection along. Whatever the other tab is "
                           + "doing — loading, saving, sounding an item — is finished first; the top "
                           + "bar says when it is waiting. An item you were answering comes back later."))
                        Text(.init("While the Trainer is connected the screen stays on, so that "
                           + "Auto-Lock cannot cut the connection while you key your answers. "
                           + "Your practice history stays in this app; **Export** it now and then."))
                            .foregroundStyle(.secondary)
                    }

                    section("If it will not connect") {

                        trouble("The app says DEVICE BUSY",
                                "The Morserino only accepts a connection from its main menu. "
                              + "Press and hold the encoder knob to leave whatever mode it is in, "
                              + "then tap Connect again.")

                        trouble("No Morserino found",
                                "Check that **Bluetooth Use** really is set to **BLE Serial**, "
                              + "and that you went back to the main menu afterwards — that is "
                              + "when the setting takes effect. Check the Morserino is switched on.")

                        trouble("“Waiting for the … tab to finish”",
                                "The other tab is still busy (a file upload can take a while). The "
                              + "connection comes over by itself when it is done.")

                        trouble("It worked before and now it does not",
                                "Only one phone or computer can be connected at a time. If the "
                              + "Morserino is already talking to something else, yours cannot "
                              + "get in.")

                        trouble("It dropped while I was using the Morserino",
                                "Anything on the device that needs WiFi — the WiFi transceiver, "
                              + "a file upload, a firmware update — switches Bluetooth off for "
                              + "the duration. It comes back when you return to the main menu.")

                        trouble("The app never asks for Bluetooth",
                                "Check iOS **Settings → Privacy & Security → Bluetooth** and make "
                              + "sure Morserino (this app) is allowed.")
                    }

                    section("More") {
                        Text(.init("Manuals, firmware updates and the desktop version of this tool "
                           + "are all on the project site."))
                        Link("morserino.info", destination: URL(string: "https://www.morserino.info")!)
                        Link("The Morserino-32 on GitHub",
                             destination: URL(string: "https://github.com/oe1wkl/Morserino-32")!)
                        NavigationLink("Open-source components and their licences") { LicencesView() }
                    }
                }
                .padding(.horizontal)
                .padding(.bottom, 28)
            }
            .navigationTitle("Help")
        }
    }

    // MARK: - Building blocks

    @ViewBuilder
    private func section(_ title: String, @ViewBuilder _ body: () -> some View) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.title3.bold())
                .padding(.top, 6)
            body()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private func step(_ number: Int, _ text: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Text("\(number)")
                .font(.footnote.bold().monospacedDigit())
                .foregroundStyle(.white)
                .frame(width: 22, height: 22)
                .background(Circle().fill(Color.accentColor))
            Text(.init(text))
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        // One announcement per step, rather than the number and the text as two.
        .accessibilityElement(children: .combine)
    }

    private func trouble(_ symptom: String, _ cure: String) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(symptom).font(.subheadline.bold())
            Text(.init(cure)).foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .combine)
    }
}

/// The licences of what the app bundles so that it needs no network: pdf.js,
/// mammoth and JSZip for the File Builder, and the Lato font (Web/vendor).
private struct LicencesView: View {
    private let text = BundleWeb.string(named: "vendor/THIRD_PARTY_LICENSES.txt")
        ?? "The licence file is missing from this build."

    var body: some View {
        ScrollView {
            Text(text)
                .font(.system(.footnote, design: .monospaced))
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding()
        }
        .navigationTitle("Licences")
        .navigationBarTitleDisplayMode(.inline)
    }
}
