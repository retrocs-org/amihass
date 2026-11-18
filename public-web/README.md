# AmiHASS project website

A buildless, responsive static site for https://amihass.retrocs.org.
No dependencies, analytics, remote fonts, credentials or backend are required.

Preview from this directory with `python3 -m http.server 8080` and open
http://localhost:8080. Deploy the contents of this directory to static hosting.
Publication is managed separately. The screenshot gallery shows actual AmigaOS
application windows; there are no simulated application controls on the page.
GitHub links target the project URLs supplied by the maintainer. Availability
of a published repository or release is managed separately.

## Automatic releases

`releases.js` reads the public GitHub REST API for `retrocs-org/amihass`.
The latest stable release comes from `/releases/latest`; `/releases?per_page=20`
supplies recent history. Drafts and prereleases are excluded. Version tags,
published dates, uploaded ZIP/LHA archives, sizes and per-asset download counts
come from GitHub. Matching `.sha256` or `.sha256sum` attachments are linked when
present. Source archive links use the exact release tag. SDK archives are shown
when attached as a separate SDK ZIP, below the main application downloads.
Use the All releases link for history beyond the 20 most recent releases.

No token, server, build step or scheduled job is needed. Successful metadata is
cached in the visitor's browser for 15 minutes; visible pages refresh on that
interval and when revisited. There is no manual Refresh control. Requests time
out after 10 seconds. Offline/rate-limited requests may use cached metadata for
up to 24 hours, clearly marked with its timestamp. No download counts are
invented. Missing repositories/releases show “AmiHASS hasn’t been released yet.”
Only published GitHub releases provide download links. Network failures show
a separate availability error. JavaScript-disabled browsers can follow the
GitHub releases link.

When the repository is ready, publish a stable release and attach the app archive
and matching checksum. The website will pick it up without changing its code.
The public website remains `https://amihass.retrocs.org/`; the source repository
is `https://github.com/retrocs-org/amihass`. No deployment is performed by these
files. If a hosting Content-Security-Policy is added, permit browser connections
to `https://api.github.com` and downloads/links to `https://github.com`.

Run `node tests/test_web_releases.cjs` from the repository root to test fixture
responses without requiring a published repository. The site uses text nodes
for release metadata and restricts remote links to this GitHub repository.

## Page structure

The dark design uses charcoal backgrounds, muted violet accents and native sans-serif typography.
The Home Assistant introduction, toolkit and shared-service diagram introduce the project.
Application releases and their history appear above the developer section.
SDK release archives are rendered separately in `#sdk-release-content`, below
the main download listing. Both sections use the same GitHub metadata request.
No local build is offered when a GitHub release is absent.

The stylesheet has one dark theme definition. Superseded declarations and rules
for removed sections are pruned while preserving desktop/mobile cascade order.

## Genuine screenshots

`assets/screenshots/` contains lossless PNG conversions of guest window captures.
They show AmiHASS 0.1 on AmigaOS 3.2.1 in Amiga Workbench.app, using shared
Sample data and no server token. Window frames and contents are captured from
the emulator screen bitmap; no interface elements are drawn by the website.

To regenerate with your configured Amiga compiler and no emulator running:

```sh
python3 tools/capture-website.py --compiler 'vc +aos68k'
```

Run that command from the repository root. It builds a QA-only capture helper,
starts the app's isolated `--test` workflow and captures Preferences, Device
Browser, the room selector, Dashboard and About. It never stops an existing
session. Original PPM files, guest logs and JUnit reports are retained under
`outputs/website-captures/`. The website PNGs preserve the captured dimensions;
only file format conversion is performed.
