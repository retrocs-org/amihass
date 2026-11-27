<?php
declare(strict_types=1);
// Public GitHub metadata only. The cache contains no credentials or private data.
const AH_RELEASE_HOME = 'https://github.com/retrocs-org/amihass';
const AH_RELEASE_API = 'https://api.github.com/repos/retrocs-org/amihass/releases';
const AH_RELEASE_LIMIT = 2097152;
const AH_RELEASE_TTL = 300;
const AH_RELEASE_RETRY = 60;
const AH_RELEASE_STALE = 86400;

function ah_html(string $value): string
{
    return htmlspecialchars($value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function ah_release_payload(array $raw, int $now): ?array
{
    $tag = $raw['tag_name'] ?? null;
    if (!is_string($tag) || !preg_match('/^v?([0-9]{1,6}\.[0-9]{1,6}(?:\.[0-9]{1,6})?)$/D', $tag, $match)) return null;
    if (($raw['draft'] ?? null) !== false || ($raw['prerelease'] ?? null) !== false) return null;
    $published = $raw['published_at'] ?? null;
    if (!is_string($published) || ($date = strtotime($published)) === false || $date <= 0 || $date > $now) return null;
    $url = AH_RELEASE_HOME . '/releases/tag/' . rawurlencode($tag);
    if (($raw['html_url'] ?? null) !== $url || !is_array($raw['assets'] ?? null) || count($raw['assets']) > 1000) return null;
    $assets = [];
    foreach ($raw['assets'] as $asset) {
        if (!is_array($asset)) continue;
        $name = $asset['name'] ?? null;
        if (!is_string($name) || !preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]{0,180}$/D', $name) || isset($assets[$name])) continue;
        $download = AH_RELEASE_HOME . '/releases/download/' . rawurlencode($tag) . '/' . rawurlencode($name);
        if (($asset['browser_download_url'] ?? null) !== $download || ($asset['state'] ?? null) !== 'uploaded') continue;
        if (!is_int($asset['size'] ?? null) || $asset['size'] <= 0) continue;
        $count = $asset['download_count'] ?? null;
        $assets[$name] = ['name' => $name, 'url' => $download, 'size' => $asset['size'],
            'downloads' => is_int($count) && $count >= 0 ? $count : null];
    }
    return ['tag' => $tag, 'version' => $match[1], 'published_at' => $date, 'url' => $url, 'assets' => $assets];
}

function ah_release_fetch(string $endpoint): array
{
    if (!in_array($endpoint, ['latest', 'history'], true)) throw new InvalidArgumentException('Unknown release endpoint');
    $url = AH_RELEASE_API . ($endpoint === 'latest' ? '/latest' : '?per_page=20');
    $headers = ['User-Agent: AmiHASS-website/0.3', 'Accept: application/vnd.github+json'];
    if (function_exists('curl_init')) {
        $handle = curl_init($url);
        if ($handle === false) return ['status' => 0];
        $body = '';
        curl_setopt_array($handle, [CURLOPT_HTTPHEADER => $headers, CURLOPT_CONNECTTIMEOUT => 2, CURLOPT_TIMEOUT => 4,
            CURLOPT_FOLLOWLOCATION => false, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2,
            CURLOPT_WRITEFUNCTION => static function ($curl, string $chunk) use (&$body): int {
                if (strlen($body) + strlen($chunk) > AH_RELEASE_LIMIT) return 0;
                $body .= $chunk;
                return strlen($chunk);
            }]);
        $success = curl_exec($handle);
        $status = $success === false ? 0 : (int)curl_getinfo($handle, CURLINFO_RESPONSE_CODE);
        unset($handle);
        return ['status' => $status, 'body' => $body];
    }
    $context = stream_context_create(['http' => ['header' => implode("\r\n", $headers), 'timeout' => 4,
        'follow_location' => 0, 'ignore_errors' => true], 'ssl' => ['verify_peer' => true, 'verify_peer_name' => true]]);
    $http_response_header = [];
    $body = @file_get_contents($url, false, $context, 0, AH_RELEASE_LIMIT + 1);
    preg_match('/\s([0-9]{3})(?:\s|$)/', $http_response_header[0] ?? '', $status);
    return ['status' => is_string($body) && strlen($body) <= AH_RELEASE_LIMIT ? (int)($status[1] ?? 0) : 0,
        'body' => is_string($body) && strlen($body) <= AH_RELEASE_LIMIT ? $body : ''];
}

function ah_release_json(array $response): ?array
{
    $body = $response['body'] ?? null;
    if (($response['status'] ?? 0) !== 200 || !is_string($body) || strlen($body) > AH_RELEASE_LIMIT) return null;
    $data = json_decode($body, true, 64);
    return json_last_error() === JSON_ERROR_NONE && is_array($data) ? $data : null;
}

function ah_cache_read(string $path, int $now): array
{
    if (is_link($path) || !is_file($path) || filesize($path) > AH_RELEASE_LIMIT * 2) return [];
    $body = @file_get_contents($path, false, null, 0, AH_RELEASE_LIMIT * 2 + 1);
    if (!is_string($body) || strlen($body) > AH_RELEASE_LIMIT * 2) return [];
    $record = json_decode($body, true, 64);
    if (!is_array($record) || ($record['schema'] ?? null) !== 1 || ($record['repository'] ?? null) !== AH_RELEASE_HOME) return [];
    foreach (['checked_at', 'confirmed_at'] as $key) {
        if (!is_int($record[$key] ?? null) || $record[$key] < 0 || $record[$key] > $now) return [];
    }
    if (!array_key_exists('latest', $record) || !is_array($record['history'] ?? null) || !array_is_list($record['history']) || count($record['history']) > 20) return [];
    if ($record['latest'] !== null && (!is_array($record['latest']) || ah_release_payload($record['latest'], $now) === null)) return [];
    return $record;
}

function ah_cache_write(string $path, array $record): void
{
    if (is_link($path) || !is_dir(dirname($path))) return;
    $body = json_encode($record, JSON_UNESCAPED_SLASHES | JSON_INVALID_UTF8_SUBSTITUTE);
    if (!is_string($body) || strlen($body) > AH_RELEASE_LIMIT * 2) return;
    $temporary = @tempnam(dirname($path), 'amihass-release-');
    if ($temporary === false) return;
    @chmod($temporary, 0600);
    if (@file_put_contents($temporary, $body, LOCK_EX) !== false) @rename($temporary, $path);
    if (is_file($temporary)) @unlink($temporary);
}

function ah_release_catalog(?callable $fetch = null, ?string $cachePath = null, ?int $now = null): array
{
    $now ??= time();
    $cachePath ??= sys_get_temp_dir() . '/amihass-releases-' . hash('sha256', __DIR__) . '.json';
    $record = ah_cache_read($cachePath, $now);
    $offline = $fetch === null && getenv('AMIHASS_RELEASE_OFFLINE') === '1';
    $fetch ??= 'ah_release_fetch';
    $age = $now - ($record['checked_at'] ?? 0);
    $ttl = !empty($record['failed']) || !empty($record['history_unavailable']) ? AH_RELEASE_RETRY : AH_RELEASE_TTL;
    $lock = null;
    if (!$offline && (!$record || $age >= $ttl)) {
        $lockPath = $cachePath . '.lock';
        // Concurrent page requests use the last result while one worker refreshes.
        if (!is_link($lockPath)) $lock = @fopen($lockPath, 'c');
        $refresh = $lock === false || $lock === null || flock($lock, LOCK_EX | LOCK_NB);
        if ($refresh) {
            try {
                $response = $fetch('latest');
                $raw = ah_release_json($response);
                if (($response['status'] ?? 0) === 404) {
                    $record = ['latest' => null, 'history' => [], 'confirmed_at' => $now, 'failed' => false];
                } elseif ($raw !== null && ah_release_payload($raw, $now) !== null) {
                    $history = null;
                    try { $history = ah_release_json($fetch('history')); } catch (Throwable $error) { /* Preserve latest. */ }
                    $complete = is_array($history) && array_is_list($history) && count($history) <= 20;
                    $record = ['latest' => $raw, 'history' => $complete ? $history : [], 'confirmed_at' => $now,
                        'failed' => false, 'history_unavailable' => !$complete];
                } else {
                    $record['failed'] = true;
                }
            } catch (Throwable $error) {
                $record['failed'] = true;
            }
            $record['schema'] = 1; $record['repository'] = AH_RELEASE_HOME; $record['checked_at'] = $now;
            $record += ['latest' => null, 'history' => [], 'confirmed_at' => 0];
            ah_cache_write($cachePath, $record);
        }
        if (is_resource($lock)) { flock($lock, LOCK_UN); fclose($lock); }
    }
    if ($offline) $record['failed'] = true;
    $confirmed = $record['confirmed_at'] ?? 0;
    $valid = $confirmed > 0 && $now >= $confirmed && $now - $confirmed < AH_RELEASE_STALE;
    $latest = $valid && is_array($record['latest'] ?? null) ? ah_release_payload($record['latest'], $now) : null;
    $history = [];
    if ($latest !== null) {
        $seen = [$latest['tag'] => true];
        foreach ($record['history'] as $raw) {
            $release = is_array($raw) ? ah_release_payload($raw, $now) : null;
            if ($release === null || isset($seen[$release['tag']])) continue;
            $history[] = $release; $seen[$release['tag']] = true;
        }
    }
    $failed = !empty($record['failed']) || !$valid || $now - ($record['checked_at'] ?? 0) >= $ttl;
    return ['status' => $latest !== null ? ($failed ? 'stale' : 'ready') : ($valid && !$failed ? 'empty' : 'unavailable'),
        'latest' => $latest, 'older' => $history, 'confirmed_at' => $confirmed,
        'history_unavailable' => !empty($record['history_unavailable'])];
}

function ah_release_archives(array $release, bool $sdk = false): array
{
    return array_values(array_filter($release['assets'], static function (array $asset) use ($sdk): bool {
        if (!preg_match('/\.(lha|lzh|zip|tar\.gz|tgz)$/iD', $asset['name'])) return false;
        $isSdk = (bool)preg_match('/(?:^|[-_.])sdk(?:[-_.]|$)/i', $asset['name']);
        return $sdk ? $isSdk : (!$isSdk && !preg_match('/(?:source|src|project|symbols|debug)/i', $asset['name']));
    }));
}

function ah_release_main(array $release): ?array
{
    $files = ah_release_archives($release);
    foreach (['lha', 'zip'] as $extension) {
        foreach ($files as $file) if (preg_match('/^AmiHASS[-_v0-9.]*\.' . $extension . '$/iD', $file['name'])) return $file;
    }
    return $files[0] ?? null;
}

function ah_release_size(int $bytes): string
{
    if ($bytes < 1024) return $bytes . ' B';
    if ($bytes < 1048576) return number_format($bytes / 1024, 0) . ' KB';
    return number_format($bytes / 1048576, 1) . ' MB';
}

function ah_release_count(?int $count): string
{
    if ($count === null) return 'Download count unavailable';
    return number_format($count) . ($count === 1 ? ' download' : ' downloads') . ' on GitHub';
}

function ah_release_checksum(array $release, array $asset): ?array
{
    return $release['assets'][$asset['name'] . '.sha256'] ?? $release['assets'][$asset['name'] . '.sha256sum'] ?? null;
}

function ah_checksum_command(?array $release): string
{
    $main = $release !== null ? ah_release_main($release) : null;
    $sum = $main !== null ? ah_release_checksum($release, $main) : null;
    return 'shasum -a 256 -c ' . ($sum['name'] ?? '<archive-name>.sha256');
}

function ah_render_asset(array $release, array $asset, string $label): void
{
    $checksum = ah_release_checksum($release, $asset);
    ?>
    <div class="download-action"><a class="button primary" href="<?= ah_html($asset['url']) ?>"><?= ah_html($label) ?></a><span class="download-size"><?= ah_html(strtoupper(pathinfo($asset['name'], PATHINFO_EXTENSION)) . ' · ' . ah_release_size($asset['size']) . ' · ' . ah_release_count($asset['downloads'])) ?></span><?php if ($checksum !== null): ?><a class="checksum-link" href="<?= ah_html($checksum['url']) ?>">SHA-256 checksum</a><?php endif; ?></div>
    <?php
}

function ah_render_downloads(array $catalog): void
{
    $latest = $catalog['latest'];
    if ($latest === null) {
        ?><article class="download-panel"><h3><?= $catalog['status'] === 'empty' ? 'AmiHASS hasn’t been released yet.' : 'Release information unavailable' ?></h3><p class="download-caption"><?= $catalog['status'] === 'empty' ? 'Downloads appear here with the first release.' : 'GitHub could not be checked. Try again later.' ?></p><a class="text-link" href="<?= AH_RELEASE_HOME ?>/releases">View GitHub releases ↗</a></article><?php
        return;
    }
    $main = ah_release_main($latest);
    ?><article class="download-panel"><div class="download-heading"><img src="assets/amihass-logo.png" width="48" height="48" alt=""><div><p class="eyebrow">LATEST PUBLISHED RELEASE</p><h3>AmiHASS <?= ah_html($latest['version']) ?></h3><p>Published <?= gmdate('Y-m-d', $latest['published_at']) ?></p></div></div><?php
    if ($main !== null) ah_render_asset($latest, $main, 'Download for AmigaOS ↓');
    else { ?><p class="download-caption">No application archive attached. See release notes.</p><?php }
    ?><p class="download-caption">Bridge, Preferences, Device Browser and Dashboard.</p><?php
    $extras = array_filter(ah_release_archives($latest), static fn(array $asset): bool => $asset['name'] !== ($main['name'] ?? null));
    if ($extras) {
        ?><div class="asset-list"><?php foreach ($extras as $asset): ?><div class="asset-row"><div><h4><?= ah_html($asset['name']) ?></h4></div><?php ah_render_asset($latest, $asset, 'Download archive ↓'); ?></div><?php endforeach; ?></div><?php
    }
    ?><div class="release-links"><a href="<?= ah_html($latest['url']) ?>">Release notes ↗</a><a href="<?= AH_RELEASE_HOME ?>/archive/refs/tags/<?= rawurlencode($latest['tag']) ?>.zip">Source ZIP</a><a href="<?= AH_RELEASE_HOME ?>/archive/refs/tags/<?= rawurlencode($latest['tag']) ?>.tar.gz">Source tar.gz</a><a href="<?= AH_RELEASE_HOME ?>/releases">All releases ↗</a></div></article><?php
    if ($catalog['older']) {
        ?><details class="older-releases"><summary>Earlier releases</summary><?php foreach ($catalog['older'] as $release): ?><article class="older-release"><header><strong>AmiHASS <?= ah_html($release['version']) ?></strong><small>Published <?= gmdate('Y-m-d', $release['published_at']) ?></small></header><ul><?php foreach (ah_release_archives($release) as $asset): $sum = ah_release_checksum($release, $asset); ?><li><a href="<?= ah_html($asset['url']) ?>"><?= ah_html($asset['name']) ?></a><small> · <?= ah_html(ah_release_size($asset['size']) . ' · ' . ah_release_count($asset['downloads'])) ?></small><?php if ($sum !== null): ?> · <a href="<?= ah_html($sum['url']) ?>">SHA-256</a><?php endif; ?></li><?php endforeach; ?></ul><a href="<?= ah_html($release['url']) ?>">Release notes ↗</a></article><?php endforeach; ?></details><?php
    }
}

function ah_render_sdk(array $catalog): void
{
    ?><h3>Developer SDK</h3><?php
    if ($catalog['latest'] === null) {
        ?><p><?= $catalog['status'] === 'empty' ? 'The SDK hasn’t been released yet.' : 'SDK releases could not be checked.' ?></p><?php
        return;
    }
    ?><p>C headers, client helpers, documentation and an example app.</p><?php
    $files = ah_release_archives($catalog['latest'], true);
    foreach ($files as $asset) ah_render_asset($catalog['latest'], $asset, 'Download SDK ↓');
    if (!$files) { ?><p>No SDK archive is attached yet.</p><?php }
}

function ah_release_status(array $catalog): string
{
    if ($catalog['status'] === 'stale') return 'GitHub unavailable. Cached data from ' . gmdate('Y-m-d H:i', $catalog['confirmed_at']) . ' UTC; counts may be outdated.';
    if ($catalog['status'] === 'unavailable') return 'Releases could not be checked. Try again later.';
    if ($catalog['status'] === 'empty') return 'No published GitHub release is available.';
    return 'Release details and counts refresh every five minutes.' . ($catalog['history_unavailable'] ? ' Earlier releases could not be loaded.' : '');
}
