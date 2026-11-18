# AmiHASS 0.1 releases

The release workflow follows Beam68: build once, verify the exact artifacts,
prepare reviewed copies, then publish those same bytes. Version stays at 0.1.
The pipeline does not require Git for local building or preparation.

## Configure

Copy `examples/release-config.json` outside the source directory. Set its `env`
object to the non-secret toolchain configuration: `AMIHASS_VC` is the vbcc
command including its Amiga target configuration; `AMIHASS_CFLAGS` defaults
to `-O`. An optional `LHA` executable enables compressed LH5 archives; without
it, portable stored LH0 archives need no external tool. Python 3.9+, make,
a host C compiler and the configured Amiga compiler are required.
Do not use local.mk for release builds. Supply host compiler selection through
`CC` in the invoking environment. Credentials belong in environment variables
or exact 1Password references, never in build env or committed configuration.

Use the same external configuration and output directory throughout:

```sh
python3 tools/release.py plan --config /path/to/release.json --output outputs/release-0.1
python3 tools/release.py build --config /path/to/release.json --output outputs/release-0.1
python3 tools/release.py check --config /path/to/release.json --output outputs/release-0.1
python3 tools/release.py prepare --config /path/to/release.json --output outputs/release-0.1
```

Build scans public inputs, runs portable tests, forces all seven native builds,
records executable/toolchain/source hashes, checks legacy ZIP compatibility,
and creates the runtime LHA, SDK ZIP and their `.sha256` files. Check rejects
changed source, native executables, configuration, archive contents or checksums.
Prepare validates `docs/RELEASE-NOTES.md` against the 0.1 changelog and writes
`release/manifest.json` plus reviewed copies under `release/publish/outputs/`.
Changing source after a build requires a rebuild and preparation again.
Build success does not certify execution on AmigaOS; keep native QA reports.

Equivalent make targets are `release-plan`, `release`, `release-verify` and
`release-prepare`, with `RELEASE_CONFIG` and `RELEASE_OUTPUT` variables.
`make package-zip` retains the earlier ZIP format. `make package-project`
creates a complete portable source/binaries/public-web ZIP under outputs/project,
excluding local configuration, credentials, build products and nested archives.

## Stage and test

```sh
python3 tools/stage-builds.py --output outputs/release-0.1 --destination /path/to/builds
```

This verifies the archive and checksum, backs up replaced owned files and
updates the AmiHASS drawer atomically without replacing the drawer itself.
Unrelated files and existing custom drawer icons are retained. Runtime payload
and SDK inventories are shared by the builders and verification tools.
After building the native QA driver with `make qa`, prepare the verified runtime
and SDK together with `python3 tools/prepare_release_qa.py
outputs/release-0.1/AmiHASS-0.1.lha build/qa-release` (one command on one line).
Use Amiga Workbench.app's isolated test workflow described in `qa/README.md`.
Never interrupt an existing user or another project's emulator session.

## Publish when authorized

The GitHub destination is https://github.com/retrocs-org/amihass.
The website is https://amihass.retrocs.org. A repository must exist before
publication. The website reads GitHub releases and otherwise reports unreleased.
No local build command creates a repository or publishes anything.

Source upload is a separate `source` command with `--dry-run` for inspection
and `--confirm-push` for execution. Initial Git setup additionally requires
`--init`; it refuses a remote with history. Only public source inputs are staged.
Review the resulting checkout and commit before preparing the final release.
Publication requires a clean Git checkout, an unchanged prepared manifest,
valid credentials, and explicit `--confirm-publish`:

```sh
python3 tools/release.py publish --config /path/to/release.json --output outputs/release-0.1 --to github --confirm-publish
```

Publication checks remote conflicts before mutation, uses annotated tags,
verifies existing assets for resumable uploads and retains publication receipts.
Never reuse a receipted output directory for a different build. GitLab is optional;
configure both web and Git URLs before selecting it. Aminet is disabled by default;
configure author/uploader and use `prepare-aminet` to review its LHA/readme before
explicit publication. Existing release receipts must remain intact.

Release-tool copyright notices are retained in NOTICE and
`tools/RELEASE-LICENSE.txt`, also included in runtime and SDK downloads.

## Release readiness and native evidence

The build compiles the example from the actual SDK ZIP in a disposable directory.
Run each native suite against the verified runtime and SDK:

```sh
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.1 --suite installer
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.1 --suite integration
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.1 --suite sample-memory
python3 tools/release.py readiness --config /path/to/release.json --output outputs/release-0.1
```

`release/readiness.md` and `.json` collect portable tests, native build, archive,
SDK and isolated Workbench results. Missing, failed, skipped or changed evidence
never counts as passed. Native results bind to source and executable hashes,
archive hashes and the test definition. Publishing requires the three native
suites above to pass for the current build. The runner never interrupts an
existing emulator session; close it before starting a suite.

Optional `--suite https-memory --credentials /private/path/to/credentials` reads
a private two-line URL/token fixture for eight authenticated GET probes. It
records before-request, lowest observed free RAM, after-request and after-flush
values plus retained growth after warmup. It does not control real devices.
The fixture is not packaged; its disposable guest profile is removed afterward.
`resource-usage.json` retains numeric measurements only. Sample-memory checks
500 controls and 500 reads after warmup, with a 64 KiB retained-growth threshold;
HTTPS uses 128 KiB after warmup. Measurements cover system-wide Fast RAM.

The installer suite substitutes only prompts, notices and source lookup in its
private script copies. A guest-only helper acknowledges the initial Installer
welcome after matching the exact generated script title and its welcome gadgets.
Other requesters are never accepted automatically. The CLI stays in Novice mode
to suppress per-file UI, while each script explicitly exercises the 0/1/2 mode
branches and custom destinations. Production installer scripts are unchanged.

For HTTPS measurements without real server credentials, run `--suite https-memory
--fixture` (one command line). It makes eight verified HTTPS GET probes to the
reserved example.com domain, expecting HTTP 404. That response proves the TLS
handshake completed; it is never treated as a successful Home Assistant connection.
Only a generated dummy token is sent. The report labels this transport-only test.

`--local-fixture` instead creates a temporary authenticated TLS endpoint bound to
en0, with a generated token and test certificate. It needs guest-to-host routing;
some bridged setups cannot reach their host. Only its public test certificate is
trusted in the disposable guest; verification stays enabled. The test private key
never enters the guest. Both variants remove temporary credentials and guest
profiles afterward. An explicit private fixture can still test a real server.
