# AmiHASS 0.3 releases

The release workflow follows Beam68: build once, verify the exact artifacts,
prepare reviewed copies, then publish those same bytes. Version stays at 0.3.
The pipeline does not require Git for local building or preparation.

## Configure

Copy `examples/release-config.json` outside the source directory. Set its `env`
object to the non-secret toolchain configuration: `AMIHASS_VC` is the vbcc
command including its Amiga target configuration; `AMIHASS_CFLAGS` defaults
to `-O`. Archives are compressed (LH5) by the built-in writer in
tools/lh5.py, which also checks every member by unpacking it again; members
that would not shrink are stored (LH0). An optional `LHA` executable is used
instead when configured. Both writers use ASCII
forward slashes in level-0 member paths for Amiga OLHa compatibility; compressed
outputs are normalized with recalculated header checksums, leaving payloads intact. Python 3.9+, make,
a host C compiler and the configured Amiga compiler are required.
Do not use local.mk for release builds. Supply host compiler selection through
`CC` in the invoking environment. Credentials belong in environment variables
or exact 1Password references, never in build env or committed configuration.

Use the same external configuration and output directory throughout:

```sh
python3 tools/release.py plan --config /path/to/release.json --output outputs/release-0.3
python3 tools/release.py build --config /path/to/release.json --output outputs/release-0.3
python3 tools/release.py check --config /path/to/release.json --output outputs/release-0.3
python3 tools/release.py prepare --config /path/to/release.json --output outputs/release-0.3
```

Build scans public inputs, runs portable tests, forces all native builds,
records executable/toolchain/source hashes, checks legacy ZIP compatibility,
and creates the runtime LHA, SDK ZIP and their `.sha256` files. Check rejects
changed source, native executables, configuration, archive contents or checksums.
Prepare derives release notes from the matching CHANGELOG.MD entry and writes
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
python3 tools/stage-builds.py --output outputs/release-0.3 --destination /path/to/builds
```

This verifies the archive and checksum, backs up replaced owned files and
updates the AmiHASS drawer atomically without replacing the drawer itself.
Unrelated files and existing custom drawer icons are retained. Runtime payload
and SDK inventories are shared by the builders and verification tools.
After building the native QA driver with `make qa`, prepare the verified runtime
and SDK together with `python3 tools/prepare_release_qa.py
outputs/release-0.3/AmiHASS-0.3.lha build/qa-release` (one command on one line).
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
python3 tools/release.py publish --config /path/to/release.json --output outputs/release-0.3 --to github --confirm-publish
```

Publication checks remote conflicts before mutation, uses annotated tags,
verifies existing assets for resumable uploads and retains publication receipts.
Never reuse a receipted output directory for a different build. GitLab is optional;
configure both web and Git URLs before selecting it. Aminet is disabled by default;
configure author/uploader and use `prepare-aminet` to review its LHA/readme before
explicit publication. The Aminet readme includes the full changelog, newest version
first; GitHub/GitLab release notes contain only the selected version.
Existing release receipts must remain intact.

Release-tool copyright notices are retained in NOTICE and
`tools/RELEASE-LICENSE.txt`, also included in runtime and SDK downloads.

## Release readiness and native evidence

The build compiles the example from the actual SDK ZIP in a disposable directory.
Run each native suite against the verified runtime and SDK:

```sh
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.3 --suite installer
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.3 --suite integration
python3 tools/run-release-qa.py --config /path/to/release.json --output outputs/release-0.3 --suite sample-memory
python3 tools/release.py readiness --config /path/to/release.json --output outputs/release-0.3
```

For the optional extended sample-only run, use `--suite sample-stability`.
It records browsing, page switching, cancellation, reconnect and bounded RAM
checks through the same source-bound report.

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

## Public release format

GitHub and GitLab use the exact numeric version as the tag (`0.3`) and
`AmiHASS 0.3` as the release title. Tags with a `v` prefix or project name are
rejected. VERSION remains 0.3 until the maintainer explicitly changes it.

Release bodies contain change bullets under these headings only, in this order:

```markdown
### Features

### Enhancements

### Bugs
```

Empty sections are omitted. The first 0.1 release contains Features only.
Prepare reads CHANGELOG.MD automatically; there is no repeated release title,
introduction, download list or installation guide in the body. A changelog
heading named Bug fixes is rendered as Bugs. Optional --notes files must match
the generated change notes exactly. docs/RELEASE-NOTES.md is a review copy.

Aminet uploads are always named AmiHASS.lha with AmiHASS.readme. GitHub and
GitLab retain versioned archive names such as AmiHASS-0.3.lha and the separate
AmiHASS-0.3-sdk.zip. The Aminet archive is an exact copy of the verified runtime.

## Explicit replacement of published 0.1

Only use same-version replacement when the maintainer explicitly requests it.
Build and validate in a new output directory; keep the original directory and
publication receipts. Commit and push the updated source before preparation.
After all required native suites pass, prepare the new release and run:

```sh
python3 tools/replace-release.py --previous outputs/release-0.1-final --output outputs/release-0.1-replacement-20261119 --confirm-replace
```

The tool verifies and retains the original packages, metadata and annotated tag,
checks both repository identities and package hashes, and uses an exact tag
lease when updating 0.1 to the rebuilt commit. Existing release objects keep
version/title 0.1/AmiHASS 0.1. Only their four known assets and change notes are
replaced. The update is not atomic across hosts; replacement-receipt.json records
progress and permits a retry against the same prepared manifest. Never rebuild
or edit that directory once replacement has started.

Aminet receives AmiHASS.lha and AmiHASS.readme, with a Replaces header for
util/wb/AmiHASS.lha. A pending incoming submission is not overwritten; moderation
remains external. The original release receipts are kept intact.

## Fresh GitHub App tokens

For automatic token generation at publication time, configure the GitHub
credential provider as `github_app` with an absolute `helper` path to the
existing create-repo-token.sh script and exact 1Password `vault`/`item` names.
The helper receives the configured repository and rw mode with workflow
permissions disabled. Its output stays in process memory; the temporary token
is not printed or saved to 1Password. Failure or cancellation stops publication
before remote changes. The active 1Password account supplies App credentials.
Environment-token and exact 1Password-token providers remain supported.
