# Self-hosted GitHub Actions runner — Morserino-32 release pipeline

One-time setup on the developer's Mac. After this is done, pushing a release tag (e.g. `git tag V8.1 && git push --tags`) fires the workflow in [`.github/workflows/release.yml`](.github/workflows/release.yml) directly on this Mac.

Companion to [RELEASE_AUTOMATION_DESIGN.md](RELEASE_AUTOMATION_DESIGN.md). All design choices below are explained there.

---

## 1. Prerequisites — verify these first

Run each command; each should print a path/version, not "command not found":

```sh
which pandoc        # expect: /usr/local/bin/pandoc
which weasyprint    # for PDF rendering
which platformio    # or: which pio
which gh            # GitHub CLI
gh auth status      # expect: Logged in to github.com as oe1wkl
fc-list | grep -i lato | head -1   # Lato font must be installed for PDF builds
```

If any is missing:

- `pandoc`: `brew install pandoc`
- `weasyprint`: `pip3 install --user weasyprint`
- `platformio`: `pip3 install --user platformio` (or whatever method you use locally for builds)
- `gh`: `brew install gh && gh auth login`
- **Lato font**: download from <https://www.latofonts.com/> and install (double-click each `.ttf` to install via Font Book), or `brew install --cask font-lato`

Also verify the Dropbox folder exists and is writable as your user:

```sh
ls -la "/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware"
touch "/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware/.write-test" && \
    rm "/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware/.write-test"
```

---

## 2. Register the runner

In a browser: open https://github.com/oe1wkl/Morserino-32/settings/actions/runners → **New self-hosted runner** → select **macOS** → architecture **x64** (assuming Intel Mac; pick ARM64 if Apple Silicon).

GitHub will show you a block of commands to copy. Run them in Terminal. The commands look approximately like this (the actual `--token` value is one-shot and only valid for an hour, so copy from the GitHub page when you do it):

```sh
mkdir ~/actions-runner && cd ~/actions-runner
curl -o actions-runner-osx-x64.tar.gz -L https://github.com/actions/runner/releases/download/v2.XXX.X/actions-runner-osx-x64-2.XXX.X.tar.gz
tar xzf ./actions-runner-osx-x64.tar.gz
```

Then configure — **important**: use the label `macos-morserino`, which the workflow references:

```sh
./config.sh \
    --url https://github.com/oe1wkl/Morserino-32 \
    --token <TOKEN-FROM-GITHUB-PAGE> \
    --name morserino-mac \
    --labels macos-morserino \
    --work _work \
    --unattended
```

Accept defaults for any prompts that remain.

---

## 3. Set the Dropbox-root environment variable

The workflow reads `MORSERINO_DROPBOX_ROOT` to find your local Dropbox firmware folder. **Launchd does not load your shell's `~/.zshrc` or `~/.bash_profile`**, so setting it there alone won't work when the runner runs as a service. The runner has a built-in `.env` file mechanism — create one:

```sh
cd ~/actions-runner
cat > .env <<'EOF'
MORSERINO_DROPBOX_ROOT=/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware
EOF
```

(You can also add `PATH` overrides here if needed — for example if `pandoc` or `pio` isn't on the default PATH the runner inherits.)

---

## 4. Install as a launchd service

So the runner starts automatically at login and survives Terminal being closed:

```sh
cd ~/actions-runner
./svc.sh install
./svc.sh start
```

Verify it's running:

```sh
./svc.sh status
```

On the GitHub page (`Settings → Actions → Runners`), the runner should now show as **Idle** with the `macos-morserino` label.

---

## 5. Smoke-test the runner

Trigger any workflow that targets the new runner — the simplest is to manually re-run `pio-ci.yml` after temporarily editing it, but it's cleaner to wait until `release.yml` is in place and use that with a throwaway tag. See §16 of the design doc for the dry-run plan.

> ⚠️ **Order matters for the first end-to-end dry-run.** Use a **beta** tag first (e.g. `V0.0-beta.1`) — beta runs skip the in-repo archive step, so there's no chance of accidentally pushing a junk commit to `master`. Once that's green, validate the stable path in a scratch fork (so the archive commit lands somewhere harmless), not in the real `oe1wkl/Morserino-32`.

If something is wrong, useful diagnostics:

```sh
# Latest runner log
ls -lt ~/actions-runner/_diag/ | head
tail -f ~/actions-runner/_diag/Runner_*.log

# Stop / restart the service
cd ~/actions-runner
./svc.sh stop
./svc.sh start
```

---

## 6. Tearing it down

If you ever need to re-register or move to a different machine:

```sh
cd ~/actions-runner
./svc.sh stop
./svc.sh uninstall
./config.sh remove --token <REMOVAL-TOKEN-FROM-GITHUB-PAGE>
rm -rf ~/actions-runner
```

Then remove the runner from `Settings → Actions → Runners` on GitHub.

---

## 7. Things to know

- **The Mac must be on (not asleep) when you push a release tag.** If it's asleep, the workflow stays queued until the runner comes back. To prevent sleep during builds: `caffeinate -d` in a Terminal window before tagging, or set the Mac's energy settings to never sleep while plugged in.
- **The runner runs as your user.** It has full access to your filesystem, Dropbox folder, and any credentials in your home directory. That's why this design doesn't need any GitHub-side secrets — but it also means the runner is no more (and no less) trusted than you are.
- **Updates:** the runner auto-updates by default when GitHub releases a new version. No action needed.
