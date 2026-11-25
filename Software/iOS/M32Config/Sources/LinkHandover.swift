//
//  LinkHandover.swift
//  The Bluetooth link follows the web tab on screen (Config ⇄ Trainer).
//
//  There is one BLE link and one M32 protocol session, and only one page may
//  drive it (M32BleTransport.driver). Willi's choice (2026-11-24): instead of
//  making the user disconnect in one tab and connect in the other, the link
//  moves to whichever of the two web tabs is shown — but never in the middle of
//  something. The tab that holds the link is asked to let go; it finishes what
//  is under way first (a Config read or upload, a Trainer item that is
//  sounding), then detaches WITHOUT ending the M32's session, and only then is
//  the shown tab told to connect. It finds the session still open, so the M32
//  asks for no consent again and takes it in any mode. See devdocs/ios-app/DESIGN.md.
//
//  Switching to Link test or Help moves nothing: those tabs do not drive the
//  link, so it stays where it was and follows on the next web tab shown.
//

import Combine
import Foundation

/// The page side of a hand-over, implemented by WebToolView.Coordinator.
protocol LinkHandoverPage: AnyObject {
    func handOver()          // finish what is under way, then let go (reports back)
    func cancelHandOver()    // the user came back: keep the link if not let go yet
    func attach()            // connect again (queued until the page has loaded)
}

final class LinkHandover: ObservableObject {

    private let transport: M32BleTransport

    /// The tab that was asked to let go and has not answered yet. The shown
    /// tab's bar says it is waiting for it.
    @Published private(set) var waitingFor: String?

    /// The web tab on screen, or the last one shown (Help and Link test do not count).
    @Published private(set) var shown: String?

    private var pages: [String: WeakPage] = [:]
    private var cancellables = Set<AnyCancellable>()

    private struct WeakPage { weak var page: LinkHandoverPage? }

    init(transport: M32BleTransport) {
        self.transport = transport
        // A link that comes up while another tab is shown (Connect, then a quick
        // tab switch), or goes away mid-hand-over: look again. Async, because
        // @Published fires before the new value is stored.
        transport.$state
            .sink { [weak self] _ in DispatchQueue.main.async { self?.evaluate() } }
            .store(in: &cancellables)
    }

    func register(_ page: LinkHandoverPage, role: String) {
        pages[role] = WeakPage(page: page)
        // SwiftUI creates a tab's web view the first time it is shown, which can
        // be after the other tab has already let go of the link.
        DispatchQueue.main.async { [weak self] in self?.evaluate() }
    }

    /// Called when the selected tab changes. nil: a tab that does not drive the link.
    func show(role: String?) {
        guard let role else { return }
        shown = role
        evaluate()
    }

    private func evaluate() {
        guard transport.state.isConnected else { waitingFor = nil; return }
        guard let target = shown else { return }
        guard let holder = transport.driver else {
            // Connected and nobody drives: only after a hand-over whose new tab
            // had no page yet. Its page has registered since.
            if waitingFor == nil, let page = pages[target]?.page, transport.claim(target) { page.attach() }
            return
        }
        guard pages[holder]?.page != nil else { return }   // the Link test holds it
        if holder == target {
            // Back on the tab that holds it: stop asking it to let go.
            if waitingFor == holder { pages[holder]?.page?.cancelHandOver() }
            return
        }
        guard waitingFor == nil else { return }       // already asked
        waitingFor = holder
        pages[holder]?.page?.handOver()
    }

    /// The holder let go. The M32's session is still open: give the link to the
    /// tab on screen — which may be the same tab again, if the user came back
    /// after it had already let go.
    func released(by role: String) {
        guard waitingFor == role else { return }
        waitingFor = nil
        transport.release(role)
        guard transport.state.isConnected, let target = shown, let page = pages[target]?.page else { return }
        guard transport.claim(target) else { return }
        page.attach()
    }

    /// The holder kept the link (cancelHandOver() came in time).
    func cancelled(by role: String) {
        guard waitingFor == role else { return }
        waitingFor = nil
        evaluate()                                     // the user may have switched yet again
    }
}
