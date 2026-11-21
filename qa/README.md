# Native validation

Use `/Applications/Amiga Workbench.app` and follow the
[AmigaMac QA guide](../../AmigaMac/docs/QA.md).
Close the existing Workbench session first; the app permits one emulator session.

Build the native check with your configured Amiga compiler:

```sh
make qa VC='vc +aos68k'
```

Run an isolated check with a new results directory:

```sh
'/Applications/Amiga Workbench.app/Contents/MacOS/AmigaWorkbench' \
  --workbench workbench-3.2.1 \
  --mount "QATests:$PWD/build/qa" \
  --test "$PWD/qa/unreachable-server.json" \
  --output /tmp/amihass-validation-001
```

The check expects a closed loopback endpoint to fail validation, or a missing
TCP/IP network to block the request before it starts. It fails if validation
succeeds, diagnostics are missing, or the fixture credential appears in the log.
AmigaMac reports the result in `junit.xml`; the guest saves the diagnostic log
in `AutoRun:AmiHASS-Service.log`. The test never saves server configuration.

Use `qa/token-input.json` with the same command to validate the native Token
text box, imported-text updates, clearing, and its 1024-character capacity.

`qa/preferences.json` checks native Start/Stop button states, offline settings
saving, About metadata and both diagnostic pages from a real AmiHASSService.
It writes only fixture credentials inside the app’s disposable guest and sends
no Home Assistant device actions.

`qa/installer.json` runs the actual system Installer with generated variants that
replace maintenance questions/messages with fixed answers. Installer may still
show its built-in installation-mode page; proceed to continue. File copies, stop/restart, boot
shortcuts, configuration writes and removal use the release code. It checks
fresh installation, running/stopped updates, both uninstall settings choices,
Preferences icons and unrelated-file preservation. All targets and fixture
credentials stay inside the disposable guest. The interactive prompts and
Novice final-results behavior also have portable controller coverage.

`qa/maintenance-choice.json` is an interactive check of the unmodified release
installer. Choose Novice, verify Update/Uninstall, then use Abort Install and
dismiss the abort requester. The harness checks that fixture files remain.

`qa/browser.json` opens the bundled Browser with SELFTEST. It checks the actual
native window, search and type filtering, empty-result selection handling and
all sample entity detail windows. No service, settings or network is required.

The Token input check also opens all four reconnect interval choices and verifies
the selector is disabled with automatic reconnect off. Preferences validation
saves reconnect policy offline, starts the actual service against a closed
loopback endpoint, and checks that clients see a running service with a
disconnected Home Assistant state. No real device actions are sent.

`qa/live-readonly.json` is an explicitly authorized real-server test. Supply an
external private `LiveCredentials:` mount containing a `credentials` file with
the HTTPS server origin on line one and token on line two. Do not place this
file in the repository or package. The test uses the real service to validate
authenticated API reachability, catalogue discovery and one entity state read.
It sends no ACTION messages, checks that action counters remain zero, stops
the isolated service and removes its guest cfg/credential files. Remove the
host credential mount after the run and check retained reports for credentials.
The 17 November 2026 run passed with 3,040 discovered entities.

Live validation also removes the calling task's CLI context temporarily while
starting the Preferences probe. This reproduces Workbench launch semantics,
where GetProgramName is unavailable, and verifies the actual AmiHASSPrefs PROBE
worker can authenticate before the normal service/catalogue/state checks.

The live read-only harness also runs AmiHASSBrowser LIVE-SELFTEST against the
service. It checks live entity counts, paging, server-side search and a selected
entity detail window, then verifies the service still reports zero action
requests. The standalone Browser SELFTEST explicitly selects sample mode.

Release checks also include `sample-memory.json` (500 commands plus 500 reads)
and `https-memory.json` (eight authenticated, read-only HTTPS probes, with warmup
and reclamation measurements). HTTPS credentials belong only in an external
`LiveCredentials:credentials` mount, never the package. Configurations can use
`--config` for 68000/8 MiB/640x256 validation. Only one FS-UAE session may run.

`tools/prepare_release_qa.py ARCHIVE FRESH_DIRECTORY` verifies the archive's
manifest, extracts its exact application payload and adds the separate QA helper.
Installer variants suppress questions for automation but retain file operations;
all three logical user levels are exercised. Real Installer prompt presentation
and final results pages still need manual inspection. A timeout is a failed gate.

`live-controls.json` is an opt-in real-device test. It requires a private
`LiveCredentials:controls` file containing three explicitly approved entity IDs
(climate, color light, brightness light) and a separate credentials file.
Never run it without user approval and an external snapshot/restore driver.
The driver must capture original settings, restore them on success/failure,
and verify restoration before removing private test data. No device IDs or
credentials belong in release archives or public QA logs.

`qa/sample-novadock.json` also checks dashboard backup/restore, cross-page tile
duplication/moves, shared-entity client diagnostics and the native Clients view.
It uses the sample source and makes no HTTP or real-device control requests.
Host `make check` covers replacement/cancellation dialogs, corrupt backups,
client activity expiry and DNS/auth/TLS/timeout error classifications.

The sample regression also checks native Undo/page navigation, entity control
capabilities and allowlisted diagnostic report export. `qa/transport.json`
records the bundled curl protocol list and memory before/after invocation.
`qa/https-memory.json` needs a private LiveCredentials mount with the existing
read-only test fixture format; never put real credentials in the repository.
Measured push findings are in `docs/SELECTED_UPDATES.md`; push is not enabled.

The sample regression covers Read-only Dashboard action blocking and its inspector,
refresh/reading/timestamp native widgets, template preview, and option persistence.
Host tests exercise template replacement confirmation/cancellation and server-mode
preview-only behavior, per-request cache intervals shared by fast/slow clients,
new-format round trips and migration from older dashboard files.

## Website screenshots

`tools/capture-website.py --compiler 'vc +aos68k'` builds the QA-only native
capture helper and runs `qa/website-screenshots.json` in an isolated guest.
It refuses to interrupt an existing emulator, configures shared Sample data
without a token and captures actual rendered windows. Original guest PPM files
and the test report remain in `outputs/website-captures/`; lossless PNG copies
are written to `public-web/assets/screenshots/`.

## NovaDock 0.5 / AmiHASS 0.2 candidate pairing

The 21 November 2026 local pairing check passed against the packaged AmiHASS
0.2 ZIP and NovaDock 0.5 LHA. Selected JUnit/transcripts and archive hashes are
retained in `outputs/novadock-0.5-pairing/qa/`.

- The actual native Installer file/startup/removal operations passed with the
  generated answers in all three logical user modes: fresh install,
  running/stopped updates, reinstall, both settings policies and unrelated files.
- Two-client sample controls, SDK example and ARexx passed with zero HTTP calls.
- Five hundred controls plus five hundred reads retained 32 bytes after warm-up,
  below the 64 KiB budget; this is a system-wide Fast RAM observation.
- The standalone SDK compiled from the archive payload. Public SDK hashes match
  NovaDock's recorded ABI 1 snapshot, paired with AmiHASS 0.2.
- NovaDock's actual native HASS client passed missing-service guidance,
  room/type/device/search filtering, brightness readback and three outages.

No live device actions or publication occurred. These local development-package
results do not replace source-bound readiness after freezing a clean release.
Physical-device tests and interactive Installer prompt inspection remain separate
checks. The application version remains 0.2.
