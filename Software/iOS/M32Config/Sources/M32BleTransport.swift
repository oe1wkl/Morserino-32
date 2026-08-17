//
//  M32BleTransport.swift
//  M32 Config — a byte pipe to the Morserino-32 over BLE (Nordic UART Service).
//
//  This is the iOS counterpart of MorseBleSerial.cpp in the firmware. Several
//  of the rules below are *firmware contracts*, not style choices; before
//  changing them read devdocs/ble-serial/DESIGN.md and the comments in
//  MorseBleSerial.cpp.
//
//  Threading: the central manager is created on the main queue, so every
//  CoreBluetooth callback and every mutation of this object happens on the
//  main thread. That is what makes the @Published properties safe without any
//  further synchronisation. Do not move the manager to a background queue
//  without revisiting that.
//

import Combine
import CoreBluetooth
import Foundation

// MARK: - Service definition

/// UUIDs and names as advertised by the firmware (`MorseBleSerial.cpp`).
enum NordicUART {
    static let service = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    /// Central → device. The firmware exposes WRITE and WRITE_NR on this one.
    static let rx = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    /// Device → central, NOTIFY only (with a CCCD, so notifications must be enabled).
    static let tx = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

    /// Advertised local name. The firmware puts the 128-bit service UUID in the
    /// ADV PDU and the name in the scan response, so scanning *by service UUID*
    /// finds the device and the name arrives a moment later.
    static let advertisedNameFragment = "Morserino"
}

// MARK: - Public types

struct M32Device: Identifiable, Equatable {
    let id: UUID
    let name: String
    var rssi: Int
    fileprivate let peripheral: CBPeripheral

    static func == (a: M32Device, b: M32Device) -> Bool { a.id == b.id }
}

enum M32LinkState: Equatable {
    case bluetoothUnavailable(String)
    case idle
    case scanning
    case connecting(String)
    case connected(String)

    var isConnected: Bool { if case .connected = self { return true }; return false }

    var label: String {
        switch self {
        case .bluetoothUnavailable(let why): return why
        case .idle: return "Not connected"
        case .scanning: return "Scanning…"
        case .connecting(let name): return "Connecting to \(name)…"
        case .connected(let name): return name
        }
    }
}

enum M32TransportError: LocalizedError {
    case bluetoothUnavailable(String)
    case noDeviceFound
    case connectionFailed(String)
    case servicesMissing
    case notConnected
    case timedOut(String)

    var errorDescription: String? {
        switch self {
        case .bluetoothUnavailable(let why): return why
        case .noDeviceFound:
            return "No Morserino-32 found. Is the device switched on, and is "
                 + "\"Bluetooth Use\" set to \"BLE Serial\" in its preferences?"
        case .connectionFailed(let why): return "Connection failed: \(why)"
        case .servicesMissing:
            return "The device connected but does not offer the Nordic UART service. "
                 + "Is it running firmware V9 or newer?"
        case .notConnected: return "Not connected."
        case .timedOut(let what): return "Timed out \(what)."
        }
    }
}

// MARK: - Transport

/// A line-oriented byte pipe. It knows nothing about the M32 protocol — it
/// delivers received bytes to `onReceive` and sends whatever `send(_:)` is
/// given. Protocol semantics live in `M32Client` (native) or in the web tool's
/// own JavaScript (via `bridge.js`).
final class M32BleTransport: NSObject, ObservableObject {

    @Published private(set) var state: M32LinkState = .idle
    @Published private(set) var discovered: [M32Device] = []

    /// Raw bytes as they arrive from the TX characteristic, broadcast to every
    /// registered receiver. A notification can split a multi-byte UTF-8
    /// sequence, so each *consumer* decodes incrementally — the transport never
    /// guesses at character boundaries.
    ///
    /// Receiving is multicast, but *sending* is not: two consumers issuing
    /// commands into the same session will read each other's replies. Exactly
    /// one of them may drive the conversation at a time.
    private var receivers: [UUID: (Data) -> Void] = [:]

    @discardableResult
    func addReceiver(_ handler: @escaping (Data) -> Void) -> UUID {
        let token = UUID()
        receivers[token] = handler
        return token
    }

    func removeReceiver(_ token: UUID) {
        receivers.removeValue(forKey: token)
    }

    /// Called when an established link drops for any reason other than our own
    /// `disconnect()`.
    var onLinkLost: (() -> Void)?

    /// Free-text progress, for the log pane.
    var onLog: ((String) -> Void)?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxChar: CBCharacteristic?
    private var txChar: CBCharacteristic?

    private var outbox = Data()
    private var writeInFlight = false

    private var poweredOnWaiters: [CheckedContinuation<Void, Error>] = []
    private var scanWaiter: CheckedContinuation<M32Device, Error>?
    private var attachWaiter: CheckedContinuation<String, Error>?
    private var scanDeadline: Task<Void, Never>?
    private var settleTask: Task<Void, Never>?
    private var settleWindow: TimeInterval = 1.2
    private var intentionalDisconnect = false

    /// The device uses a *static random* address derived from its BT MAC, so
    /// its CoreBluetooth identifier is stable across reboots and we can go
    /// straight back to it without scanning.
    private static let lastDeviceKey = "M32Config.lastPeripheralID"

    /// The manager is created on first use, not in `init()`: instantiating
    /// CBCentralManager is what raises the system Bluetooth permission prompt,
    /// and that should happen when the user asks to connect — not the instant
    /// the app opens, with no context for the question.
    private func ensureCentral() {
        guard central == nil else { return }
        central = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: Connecting

    /// Scan (or reconnect to the last device) and bring the link fully up:
    /// services discovered, notifications enabled. Returns the device name.
    ///
    /// `@MainActor` is not decoration: a plain non-isolated `async` method would
    /// run its body on the generic executor while CoreBluetooth delivers its
    /// callbacks on the main queue, and the two would race over `scanWaiter`,
    /// `attachWaiter` and `state`.
    @discardableResult @MainActor
    func connect(scanTimeout: TimeInterval = 6, settle: TimeInterval = 1.2) async throws -> String {
        ensureCentral()
        try await waitForPoweredOn(timeout: 4)

        if let known = knownPeripheral() {
            log("Trying the last known device…")
            do { return try await attach(to: known, name: known.name ?? "Morserino-32") }
            catch { log("Quick reconnect failed, falling back to a scan.") }
        }

        let device = try await scanForBest(timeout: scanTimeout, settle: settle)
        return try await attach(to: device.peripheral, name: device.name)
    }

    func disconnect() {
        intentionalDisconnect = true
        stopScan()
        outbox.removeAll()
        writeInFlight = false
        if let p = peripheral {
            central?.cancelPeripheralConnection(p)
        }
        peripheral = nil
        rxChar = nil
        txChar = nil
        state = .idle
    }

    // MARK: Sending

    /// Queue one protocol line. The newline is added here — the firmware
    /// dispatches on `\n` and nothing happens without it.
    func send(line: String) {
        send(raw: Data((line + "\n").utf8))
    }

    func send(raw data: Data) {
        guard peripheral != nil, rxChar != nil else { return }
        outbox.append(data)
        pumpWrites()
    }

    private func pumpWrites() {
        guard !writeInFlight, !outbox.isEmpty,
              let p = peripheral, let rx = rxChar, p.state == .connected else { return }

        // Chunk to the *write-without-response* limit (ATT_MTU − 3) even though
        // we write WITH response: staying under that limit keeps every write a
        // single ATT packet. Larger writes would become an ATT long write
        // (Prepare/Execute), a path the firmware's RX ring has never been
        // exercised on. `.withResponse` still gives us CoreBluetooth's ordering
        // and backpressure for free.
        let limit = max(20, p.maximumWriteValueLength(for: .withoutResponse))
        let n = min(limit, outbox.count)
        let chunk = outbox.prefix(n)
        outbox.removeFirst(n)

        writeInFlight = true
        p.writeValue(Data(chunk), for: rx, type: .withResponse)
    }

    // MARK: Scanning

    func startScan() {
        ensureCentral()
        guard central.state == .poweredOn else { return }
        discovered.removeAll()
        state = .scanning
        central.scanForPeripherals(withServices: [NordicUART.service], options: nil)
    }

    func stopScan() {
        scanDeadline?.cancel(); scanDeadline = nil
        settleTask?.cancel(); settleTask = nil
        if central?.isScanning == true { central.stopScan() }
    }

    /// Scan, wait `settle` seconds after the first hit so a second Morserino in
    /// the room can announce itself too, then return the strongest signal.
    ///
    /// Timeouts everywhere in this file resume the *same* continuation rather
    /// than racing it in a task group: a task group would leave the losing child
    /// suspended on a continuation nobody ever resumes, and the group waits for
    /// its children — an immediate hang.
    @MainActor
    private func scanForBest(timeout: TimeInterval, settle: TimeInterval) async throws -> M32Device {
        startScan()
        log("Scanning for a Morserino-32…")
        settleWindow = settle

        scanDeadline = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(timeout * 1_000_000_000))
            guard !Task.isCancelled else { return }
            self?.finishScan(with: .failure(M32TransportError.noDeviceFound))
        }

        return try await withCheckedThrowingContinuation { (c: CheckedContinuation<M32Device, Error>) in
            scanWaiter = c
        }
    }

    private func finishScan(with result: Result<M32Device, Error>) {
        guard let waiter = scanWaiter else { return }
        scanWaiter = nil
        stopScan()
        if case .failure = result, !state.isConnected { state = .idle }
        waiter.resume(with: result)
    }

    private func knownPeripheral() -> CBPeripheral? {
        guard let central, central.state == .poweredOn,
              let raw = UserDefaults.standard.string(forKey: Self.lastDeviceKey),
              let uuid = UUID(uuidString: raw) else { return nil }
        return central.retrievePeripherals(withIdentifiers: [uuid]).first
    }

    // MARK: Attaching

    @MainActor
    private func attach(to p: CBPeripheral, name: String) async throws -> String {
        stopScan()
        intentionalDisconnect = false
        peripheral = p
        p.delegate = self
        state = .connecting(name)

        let deadline = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 12_000_000_000)
            guard !Task.isCancelled, let self else { return }
            self.central.cancelPeripheralConnection(p)
            self.finishAttach(with: .failure(
                M32TransportError.timedOut("waiting for the device to answer")))
        }
        defer { deadline.cancel() }

        return try await withCheckedThrowingContinuation { (c: CheckedContinuation<String, Error>) in
            attachWaiter = c
            central.connect(p, options: nil)
        }
    }

    private func finishAttach(with result: Result<String, Error>) {
        guard let waiter = attachWaiter else { return }
        attachWaiter = nil
        waiter.resume(with: result)
    }

    // MARK: Bluetooth power

    @MainActor
    private func waitForPoweredOn(timeout: TimeInterval) async throws {
        switch central.state {
        case .poweredOn: return
        case .unsupported: throw M32TransportError.bluetoothUnavailable("This device has no Bluetooth LE.")
        case .unauthorized:
            throw M32TransportError.bluetoothUnavailable(
                "M32 Config is not allowed to use Bluetooth. Grant it in Settings → Privacy → Bluetooth.")
        case .poweredOff: throw M32TransportError.bluetoothUnavailable("Bluetooth is switched off.")
        default: break // .unknown / .resetting — the manager is still starting up
        }

        let deadline = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(timeout * 1_000_000_000))
            guard !Task.isCancelled, let self else { return }
            let waiters = self.poweredOnWaiters
            self.poweredOnWaiters.removeAll()
            waiters.forEach { $0.resume(throwing: M32TransportError.timedOut("waiting for Bluetooth to start")) }
        }
        defer { deadline.cancel() }

        try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
            poweredOnWaiters.append(c)
        }
    }

    private func log(_ msg: String) { onLog?(msg) }
}

// MARK: - CBCentralManagerDelegate

extension M32BleTransport: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ manager: CBCentralManager) {
        switch manager.state {
        case .poweredOn:
            state = .idle
            let waiters = poweredOnWaiters
            poweredOnWaiters.removeAll()
            waiters.forEach { $0.resume() }
        case .poweredOff:
            failEverything(M32TransportError.bluetoothUnavailable("Bluetooth is switched off."))
            state = .bluetoothUnavailable("Bluetooth is off")
        case .unauthorized:
            failEverything(M32TransportError.bluetoothUnavailable("Bluetooth permission denied."))
            state = .bluetoothUnavailable("Bluetooth not permitted")
        case .unsupported:
            failEverything(M32TransportError.bluetoothUnavailable("No Bluetooth LE on this device."))
            state = .bluetoothUnavailable("No Bluetooth LE")
        default:
            break
        }
    }

    func centralManager(_ manager: CBCentralManager,
                        didDiscover p: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
            ?? p.name
            ?? "Unnamed BLE device"

        if let index = discovered.firstIndex(where: { $0.id == p.identifier }) {
            discovered[index].rssi = RSSI.intValue
        } else {
            discovered.append(M32Device(id: p.identifier, name: name, rssi: RSSI.intValue, peripheral: p))
            log("Found \(name) (\(RSSI.intValue) dBm)")
        }

        // First hit starts a short settle window, so a second device in the room
        // gets a chance to answer before we commit to the strongest one.
        guard scanWaiter != nil, settleTask == nil else { return }
        settleTask = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64((self?.settleWindow ?? 1.2) * 1_000_000_000))
            guard !Task.isCancelled, let self else { return }

            // Anything advertising the Nordic UART service reaches this point —
            // plenty of unrelated dev boards do. Prefer the ones that name
            // themselves, and only fall back to the rest if none did.
            let named = self.discovered.filter {
                $0.name.localizedCaseInsensitiveContains(NordicUART.advertisedNameFragment)
            }
            let candidates = named.isEmpty ? self.discovered : named

            if let best = candidates.max(by: { $0.rssi < $1.rssi }) {
                self.finishScan(with: .success(best))
            } else {
                self.finishScan(with: .failure(M32TransportError.noDeviceFound))
            }
        }
    }

    func centralManager(_ manager: CBCentralManager, didConnect p: CBPeripheral) {
        log("Connected — discovering services…")
        p.discoverServices([NordicUART.service])
    }

    func centralManager(_ manager: CBCentralManager,
                        didFailToConnect p: CBPeripheral,
                        error: Error?) {
        finishAttach(with: .failure(
            M32TransportError.connectionFailed(error?.localizedDescription ?? "unknown reason")))
        state = .idle
    }

    func centralManager(_ manager: CBCentralManager,
                        didDisconnectPeripheral p: CBPeripheral,
                        error: Error?) {
        let wasConnected = state.isConnected
        peripheral = nil
        rxChar = nil
        txChar = nil
        outbox.removeAll()
        writeInFlight = false
        state = .idle

        finishAttach(with: .failure(
            M32TransportError.connectionFailed(error?.localizedDescription ?? "the device disconnected")))

        if wasConnected && !intentionalDisconnect {
            log("Link lost: \(error?.localizedDescription ?? "device disconnected")")
            onLinkLost?()
        }
        intentionalDisconnect = false
    }

    private func failEverything(_ error: Error) {
        finishScan(with: .failure(error))
        finishAttach(with: .failure(error))
        let waiters = poweredOnWaiters
        poweredOnWaiters.removeAll()
        waiters.forEach { $0.resume(throwing: error) }
    }
}

// MARK: - CBPeripheralDelegate

extension M32BleTransport: CBPeripheralDelegate {

    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil, let service = p.services?.first(where: { $0.uuid == NordicUART.service }) else {
            finishAttach(with: .failure(M32TransportError.servicesMissing))
            central.cancelPeripheralConnection(p)
            return
        }
        p.discoverCharacteristics([NordicUART.rx, NordicUART.tx], for: service)
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard error == nil, let chars = service.characteristics else {
            finishAttach(with: .failure(M32TransportError.servicesMissing))
            return
        }
        rxChar = chars.first { $0.uuid == NordicUART.rx }
        txChar = chars.first { $0.uuid == NordicUART.tx }

        guard let tx = txChar, rxChar != nil else {
            finishAttach(with: .failure(M32TransportError.servicesMissing))
            central.cancelPeripheralConnection(p)
            return
        }
        // The link is not usable until notifications are on: the firmware's CCCD
        // is what turns its TX characteristic into an actual output.
        p.setNotifyValue(true, for: tx)
    }

    func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor c: CBCharacteristic, error: Error?) {
        guard c.uuid == NordicUART.tx else { return }
        if let error {
            finishAttach(with: .failure(M32TransportError.connectionFailed(
                "could not enable notifications (\(error.localizedDescription))")))
            central.cancelPeripheralConnection(p)
            return
        }
        guard c.isNotifying else { return }

        let name = p.name ?? "Morserino-32"
        UserDefaults.standard.set(p.identifier.uuidString, forKey: Self.lastDeviceKey)
        state = .connected(name)
        log("Link ready (\(p.maximumWriteValueLength(for: .withoutResponse) + 3) byte MTU)")
        finishAttach(with: .success(name))
        pumpWrites()
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
        guard c.uuid == NordicUART.tx, error == nil, let data = c.value, !data.isEmpty else { return }
        for handler in receivers.values { handler(data) }
    }

    func peripheral(_ p: CBPeripheral, didWriteValueFor c: CBCharacteristic, error: Error?) {
        guard c.uuid == NordicUART.rx else { return }
        writeInFlight = false
        if let error {
            log("Write failed: \(error.localizedDescription)")
            outbox.removeAll()   // a torn command is worse than a lost one
            return
        }
        pumpWrites()
    }
}
