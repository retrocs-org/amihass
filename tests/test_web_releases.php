<?php
declare(strict_types=1);
// Fixture-driven tests: no GitHub requests, credentials, uploads or releases.
require __DIR__ . '/../public-web/inc/releases.php';
function check_web(bool $ok, string $message): void
{
    if (!$ok) throw new RuntimeException($message);
}
function web_fixture(string $tag = 'v0.1', int $downloads = 17): array
{
    $version = ltrim($tag, 'v');
    $names = ["AmiHASS-$version.zip", "AmiHASS-$version.lha", "AmiHASS-$version.lha.sha256", "AmiHASS-$version-sdk.zip", "AmiHASS-$version-sdk.zip.sha256"];
    $assets = [];
    foreach ($names as $name) $assets[] = ['name' => $name, 'state' => 'uploaded', 'size' => 260000,
        'download_count' => $downloads, 'browser_download_url' => AH_RELEASE_HOME . '/releases/download/' . rawurlencode($tag) . '/' . $name];
    return ['tag_name' => $tag, 'draft' => false, 'prerelease' => false, 'published_at' => '2026-11-17T12:00:00Z',
        'html_url' => AH_RELEASE_HOME . '/releases/tag/' . rawurlencode($tag), 'assets' => $assets];
}
function web_response(array $data): array { return ['status' => 200, 'body' => json_encode($data)]; }
function web_render(array $catalog, bool $sdk = false): string
{
    ob_start();
    if ($sdk) ah_render_sdk($catalog); else ah_render_downloads($catalog);
    return ob_get_clean();
}
$directory = sys_get_temp_dir() . '/amihass-php-tests-' . bin2hex(random_bytes(8));
mkdir($directory, 0700);
$now = strtotime('2026-11-18T12:00:00Z');
try {
    $raw = web_fixture(); $release = ah_release_payload($raw, $now);
    check_web($release !== null && $release['version'] === '0.1', 'Version is taken from the release tag');
    check_web(ah_release_main($release)['name'] === 'AmiHASS-0.1.lha', 'Prefer the main LHA over ZIP');
    check_web(count(ah_release_archives($release)) === 2 && count(ah_release_archives($release, true)) === 1, 'SDK is separate from runtime archives');
    check_web(ah_release_count(0) === '0 downloads on GitHub' && ah_release_count(1) === '1 download on GitHub', 'Real zero and singular download counts');
    check_web(str_contains(ah_release_count(12345), '12,345') && str_contains(ah_release_count(null), 'unavailable'), 'Unknown counts are never invented');
    check_web(ah_release_checksum($release, ah_release_main($release)) !== null, 'Match archive checksums');
    check_web(ah_checksum_command($release) === 'shasum -a 256 -c AmiHASS-0.1.lha.sha256', 'Checksum command follows the actual asset');
    foreach (['draft', 'prerelease'] as $flag) {
        $bad = $raw; $bad[$flag] = true;
        check_web(ah_release_payload($bad, $now) === null, 'Reject ' . $flag);
    }
    foreach (['development', '0.1-rc1', '<img src=x onerror=alert(1)>'] as $tag) {
        $bad = $raw; $bad['tag_name'] = $tag;
        check_web(ah_release_payload($bad, $now) === null, 'Reject invalid or unsafe version tags');
    }
    $bad = $raw; $bad['published_at'] = '2099-01-01T00:00:00Z';
    check_web(ah_release_payload($bad, $now) === null, 'Reject future releases');
    $bad = $raw; $bad['html_url'] = 'https://evil.invalid/release';
    check_web(ah_release_payload($bad, $now) === null, 'Only link this repository');
    foreach (['javascript:alert(1)', 'https://evil.invalid/a.lha', AH_RELEASE_HOME . '/releases/download/v0.2/AmiHASS-0.1.zip'] as $url) {
        $bad = $raw; $bad['assets'][0]['browser_download_url'] = $url;
        check_web(count(ah_release_payload($bad, $now)['assets']) === 4, 'Reject wrong host, scheme and tag');
    }
    $bad = $raw; $bad['assets'][0]['name'] = '"><script>alert(1)</script>';
    check_web(count(ah_release_payload($bad, $now)['assets']) === 4, 'Reject unsafe filenames');
    foreach ([-1, '50', null] as $count) {
        $bad = $raw; $bad['assets'][0]['download_count'] = $count;
        check_web(ah_release_payload($bad, $now)['assets']['AmiHASS-0.1.zip']['downloads'] === null, 'Reject invalid counts');
    }
    $calls = 0;
    $fetch = static function (string $endpoint) use (&$calls, $raw): array {
        ++$calls;
        $draft = web_fixture('v0.0'); $draft['draft'] = true;
        return web_response($endpoint === 'latest' ? $raw : [$raw, web_fixture('0.0'), web_fixture('0.0'), $draft]);
    };
    $path = $directory . '/releases.json';
    $catalog = ah_release_catalog($fetch, $path, $now);
    check_web($calls === 2 && $catalog['status'] === 'ready' && count($catalog['older']) === 1, 'Fetch latest/history and exclude duplicate/draft releases');
    $html = web_render($catalog); $sdk = web_render($catalog, true);
    check_web(str_contains($html, 'AmiHASS 0.1') && str_contains($html, '17 downloads on GitHub') && str_contains($html, 'SHA-256 checksum'), 'Server renders version, counts and checksums');
    check_web(str_contains($html, 'Earlier releases') && str_contains($html, '/releases/download/0.0/'), 'Render history from GitHub');
    check_web(!str_contains($html, '-sdk.zip') && str_contains($sdk, '-sdk.zip') && str_contains($sdk, '17 downloads on GitHub'), 'Render SDK only in its separate download section');
    check_web(!str_contains($sdk, 'included in the main'), 'Do not claim SDK is in the runtime archive');
    ah_release_catalog($fetch, $path, $now + AH_RELEASE_TTL - 1);
    check_web($calls === 2, 'Share cache across visits without extra GitHub requests');
    $updated = ah_release_catalog(static fn(string $endpoint): array => web_response($endpoint === 'latest' ? web_fixture('v0.2', 42) : []), $path, $now + AH_RELEASE_TTL);
    check_web($updated['latest']['version'] === '0.2' && str_contains(web_render($updated), '42 downloads'), 'Automatically update version, links and download counts after cache expiry');
    $emptyCalls = 0;
    $emptyFetch = static function ($endpoint) use (&$emptyCalls): array { ++$emptyCalls; return ['status' => 404]; };
    $empty = ah_release_catalog($emptyFetch, $directory . '/empty.json', $now);
    ah_release_catalog($emptyFetch, $directory . '/empty.json', $now + 1);
    check_web($emptyCalls === 1 && $empty['status'] === 'empty', 'Cache missing repositories/releases');
    check_web(str_contains(web_render($empty), 'hasn’t been released yet') && !str_contains(web_render($empty), 'Download for AmigaOS'), 'Unreleased state has no local/fake download');
    check_web(str_contains(web_render($empty, true), 'hasn’t been released yet'), 'Unreleased SDK state');
    foreach ([0, 401, 403, 429, 500] as $status) {
        $stale = ah_release_catalog(static fn($endpoint): array => ['status' => $status], $path, $now + AH_RELEASE_TTL * 2 + $status);
        check_web($stale['status'] === 'stale' && $stale['latest']['version'] === '0.2', 'Retain recent verified data during failures');
        check_web(str_contains(ah_release_status($stale), 'Cached data') && str_contains(ah_release_status($stale), 'outdated'), 'Clearly label stale counts');
    }
    $expired = ah_release_catalog(static fn($endpoint): array => ['status' => 500], $path, $now + AH_RELEASE_TTL + AH_RELEASE_STALE);
    check_web($expired['status'] === 'unavailable' && $expired['latest'] === null, 'Never offer expired cached downloads');
    $failCalls = 0;
    $failedFetch = static function ($endpoint) use (&$failCalls): array { ++$failCalls; throw new RuntimeException('Fixture outage'); };
    $failed = ah_release_catalog($failedFetch, $directory . '/failed.json', $now);
    ah_release_catalog($failedFetch, $directory . '/failed.json', $now + 1);
    check_web($failed['status'] === 'unavailable' && $failCalls === 1, 'An outage is not an unreleased state; back off before retry');
    ah_release_catalog($failedFetch, $directory . '/failed.json', $now + AH_RELEASE_RETRY);
    check_web($failCalls === 2, 'Retry failed requests after the backoff');
    $partial = ah_release_catalog(static fn($endpoint): array => $endpoint === 'latest' ? web_response($raw) : ['status' => 500], $directory . '/partial.json', $now);
    check_web($partial['status'] === 'ready' && $partial['history_unavailable'] && str_contains(ah_release_status($partial), 'Earlier releases'), 'History failure does not hide the latest download');
    $none = $raw; $none['assets'] = [];
    $noAssets = ah_release_catalog(static fn($endpoint): array => web_response($endpoint === 'latest' ? $none : []), $directory . '/no-assets.json', $now);
    check_web(str_contains(web_render($noAssets), 'No application archive') && str_contains(web_render($noAssets, true), 'No SDK archive'), 'Handle a published release with no attached archives');
    $cleared = ah_release_catalog(static fn($endpoint): array => ['status' => 404], $directory . '/no-assets.json', $now + AH_RELEASE_TTL);
    check_web($cleared['status'] === 'empty', 'Confirmed 404 invalidates previously cached releases');
    foreach (['{"message":"rate limited"}', 'not json', str_repeat('x', AH_RELEASE_LIMIT + 1)] as $body) {
        $bad = ah_release_catalog(static fn($endpoint): array => ['status' => 200, 'body' => $body], $directory . '/bad-' . strlen($body) . '.json', $now);
        check_web($bad['status'] === 'unavailable', 'Malformed/oversized responses are not shown');
    }
    file_put_contents($directory . '/corrupt.json', '{bad cache');
    $recovered = ah_release_catalog($fetch, $directory . '/corrupt.json', $now);
    check_web($recovered['status'] === 'ready', 'Recover from malformed disk cache');
    $futureCache = json_decode(file_get_contents($directory . '/corrupt.json'), true); $futureCache['checked_at'] = $now + 60;
    file_put_contents($directory . '/corrupt.json', json_encode($futureCache));
    $recovered = ah_release_catalog($fetch, $directory . '/corrupt.json', $now);
    check_web($recovered['status'] === 'ready', 'Discard future-dated cache records');
    file_put_contents($directory . '/sentinel', 'unchanged'); symlink($directory . '/sentinel', $directory . '/linked.json');
    ah_release_catalog($fetch, $directory . '/linked.json', $now);
    check_web(file_get_contents($directory . '/sentinel') === 'unchanged', 'Do not read or replace cache symlinks');
    $page = file_get_contents(__DIR__ . '/../public-web/index.php');
    check_web(strpos($page, 'id="downloads"') < strpos($page, 'id="sdk-release-content"'), 'App downloads remain above SDK downloads');
    check_web(!str_contains($page, 'releases.js') && !str_contains($page, 'Checking release'), 'Server rendering does not depend on JavaScript');
    check_web(!is_file(__DIR__ . '/../public-web/index.html'), 'No stale HTML entry point can override PHP');
    check_web(str_contains(ah_html('<img "x">'), '&lt;img &quot;x&quot;&gt;'), 'Escape HTML output');
    echo "PHP website: GitHub versions/assets/counts, app/SDK separation, checksums/history, caching, retries, stale/empty/error states and safe rendering passed.\n";
} finally {
    foreach (glob($directory . '/*') as $file) unlink($file);
    rmdir($directory);
}
