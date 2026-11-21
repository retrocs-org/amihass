# AmiHASS project website

A PHP website for https://amihass.retrocs.org. The dark design, real AmigaOS
screenshots and separate app/SDK download sections need no JavaScript.

## Hosting and preview

Use PHP 8.1 or newer with HTTPS access to api.github.com. The PHP cURL extension
is recommended. The fallback needs allow_url_fopen, the HTTPS stream wrapper and
OpenSSL. Configure working server CA certificates; TLS verification stays enabled.
No database, GitHub token, frontend build or scheduled job is required.
These web-server requirements are separate from curl/AmiSSL on the Amiga.

From the repository root:

```sh
php -S 127.0.0.1:8765 -t public-web
```

Open http://127.0.0.1:8765/. A Python static server cannot run this site.
Upload the contents of public-web to a PHP-enabled web root. Make index.php the
default document; the included .htaccess sets this for Apache. Remove any old
index.html, releases.js and app.js from the deployed copy so an earlier static
page cannot override PHP. Static-only hosts such as GitHub Pages cannot run it.
Publication is managed separately; changing these files does not deploy the site.

## Automatic GitHub releases

inc/releases.php requests the public retrocs-org/amihass repository:

- /releases/latest selects GitHub's latest stable release.
- /releases?per_page=20 supplies earlier releases.
- Asset URLs, sizes and per-file download_count values come from GitHub.
- Matching .sha256/.sha256sum assets are linked beside their archives.
- The main LHA is preferred; SDK ZIPs stay in the developer section below it.
- Source links use the exact published tag. Drafts and prereleases are excluded.

Metadata is fetched on page requests and cached on the server for five minutes.
Reloading after that interval refreshes versions, links and download counts.
Failures retry after one minute. Recently verified metadata can be retained for
up to 24 hours during outages, with a visible warning that counts may be outdated.
A confirmed missing repository/release shows “AmiHASS hasn’t been released yet.”
Network failures show an availability error. No local build or invented count is
advertised. The same cached catalogue renders app downloads, the SDK and version.

The cache uses private files in PHP's system temporary directory, outside the web
root. Give the PHP worker a writable temporary directory (PHP sys_temp_dir) for
shared caching. If it is unwritable, fetching still works but caching is disabled.
Concurrent requests reuse prior data while one worker refreshes. Each HTTPS
request has a four-second timeout and a 2 MiB response limit. Only URLs belonging
to this repository and exact release assets are rendered; metadata is escaped.

For a network-free local preview, set AMIHASS_RELEASE_OFFLINE=1. This displays
recent cached data with an outage warning, or an availability message. It does
not invent a release or modify the normal cache.

## Checks

From the repository root:

```sh
php -l public-web/index.php
php -l public-web/inc/releases.php
php tests/test_web_releases.php
```

The tests use offline fixtures for changing versions/counts, cache freshness,
rate limits, outages, missing releases, app/SDK separation, checksums, unsafe
links and malformed metadata. PHP is needed for these website-specific checks;
the native Amiga build does not require it.

## Genuine screenshots

`assets/screenshots/` contains lossless PNG conversions of guest window captures.
They show AmiHASS 0.2 on AmigaOS 3.2.1 in Amiga Workbench.app, using shared
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

## Bridge features

The compact Features section sits between Overview and Screenshots and lists
shared connections, device controls, selected updates, sample data, SDK/ARexx
and diagnostics. Download versions and links still come from GitHub releases.
