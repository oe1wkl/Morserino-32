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
                           + "device for radio amateurs."))
                        Text(.init("The Morserino needs firmware **version 9.0 or newer**. Earlier "
                           + "firmware cannot speak to an app over Bluetooth at all."))
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
                        step(2, "Tap **Connect** on the Config tab.")
                        step(3, "The Morserino asks **“Allow connect?”** — press the **FN button** "
                              + "on the device to allow it.")
                        Text(.init("Asking is deliberate. Anything within radio range can reach a "
                           + "Bluetooth device, so the Morserino wants your say-so before it "
                           + "hands over control. Doing nothing for about 20 seconds refuses."))
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
                              + "sure M32 Config is allowed.")
                    }

                    section("More") {
                        Text(.init("Manuals, firmware updates and the desktop version of this tool "
                           + "are all on the project site."))
                        Link("morserino.info", destination: URL(string: "https://www.morserino.info")!)
                        Link("The Morserino-32 on GitHub",
                             destination: URL(string: "https://github.com/oe1wkl/Morserino-32")!)
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
