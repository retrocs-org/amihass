/* Public GitHub metadata only. No credentials or private API access. */
(function (scope) {
  'use strict';
  const REPO = 'retrocs-org/amihass';
  const HOME = `https://github.com/${REPO}`;
  const API = `https://api.github.com/repos/${REPO}/releases`;
  const TTL = 15 * 60 * 1000;
  const MAX_STALE = 24 * 60 * 60 * 1000;
  const CACHE = 'amihass-public-releases-v1';
  function safeURL(value) {
    try {
      const url = new URL(value);
      return url.protocol === 'https:' && !url.username && !url.password &&
        url.hostname === 'github.com' && url.pathname.startsWith(`/${REPO}/`) ? url.href : null;
    } catch { return null; }
  }
  function assets(release) {
    return (Array.isArray(release.assets) ? release.assets : []).filter(a =>
      a && typeof a.name === 'string' && a.state === 'uploaded' && safeURL(a.browser_download_url));
  }
  function files(release) {
    return assets(release).filter(a => /\.(zip|lha|lzh|tar\.gz|tgz)$/i.test(a.name));
  }
  function primary(release) {
    const candidates = files(release).filter(a => !/(?:sdk|source|src|symbols|debug)/i.test(a.name));
    return candidates.find(a => /^amihass[-_v\d.]*\.(lha|zip)$/i.test(a.name)) || candidates[0] || null;
  }
  function published(release) {
    return release && typeof release.tag_name === 'string' && release.tag_name.length > 0 &&
      !release.draft && !release.prerelease && safeURL(release.html_url);
  }
  function normalize(latest, history) {
    if (!published(latest)) return {latest:null, older:[]};
    return {latest, older:(Array.isArray(history) ? history : []).filter(r => published(r) && r.tag_name !== latest.tag_name)};
  }
  function size(bytes) {
    if (!Number.isFinite(bytes) || bytes < 0) return 'Size unavailable';
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${Math.round(bytes / 1024)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  }
  function count(asset) {
    if (!Number.isSafeInteger(asset.download_count) || asset.download_count < 0) return 'Download count unavailable';
    return `${asset.download_count.toLocaleString('en-US')} ${asset.download_count === 1 ? 'download' : 'downloads'} on GitHub`;
  }
  function date(value) {
    const d = new Date(value || NaN);
    return Number.isNaN(d.getTime()) ? 'Publication date unavailable' : `Published ${d.toISOString().slice(0,10)}`;
  }
  function checksum(release, asset) {
    return assets(release).find(a => a.name === asset.name + '.sha256' || a.name === asset.name + '.sha256sum');
  }
  async function request(fetcher, url, signal) {
    const response = await fetcher(url, {signal, headers:{Accept:'application/vnd.github+json'}});
    if (response.status === 404) return null;
    if (!response.ok) throw new Error(response.status === 403 || response.status === 429 ? 'rate-limit' : 'network');
    return response.json();
  }
  async function load(fetcher, storage, now = Date.now(), force = false) {
    let cached = null;
    try {
      const parsed = JSON.parse(storage.getItem(CACHE));
      if (parsed && Number.isFinite(parsed.time) && now >= parsed.time && now - parsed.time < MAX_STALE && published(parsed.data?.latest)) {
        cached = {time:parsed.time, data:normalize(parsed.data.latest, parsed.data.older)};
      }
    } catch { /* Storage may be unavailable. */ }
    if (!force && cached && now - cached.time < TTL) return {...cached, state:'cached'};
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 10000);
    try {
      const latest = await request(fetcher, `${API}/latest`, controller.signal);
      if (latest === null) {
        try { storage.removeItem(CACHE); } catch {}
        return {data:normalize(null, []), state:'unpublished', time:now};
      }
      if (!published(latest)) throw new Error('invalid-release');
      let history = [], historyUnavailable = false;
      try {
        const result = await request(fetcher, `${API}?per_page=20`, controller.signal);
        if (!Array.isArray(result)) throw new Error('invalid-history');
        history = result;
      } catch { historyUnavailable = true; }
      const data = normalize(latest, history);
      // Do not cache incomplete history; the next visit can recover immediately.
      if (!historyUnavailable) { try { storage.setItem(CACHE, JSON.stringify({time:now, data})); } catch {} }
      return {data, time:now, state:'fresh', historyUnavailable};
    } catch {
      return cached ? {...cached, state:'stale'} : {data:normalize(null, []), state:'unavailable', time:now};
    } finally { clearTimeout(timer); }
  }
  function render(doc, result) {
    const target = doc.querySelector('#release-content');
    const status = doc.querySelector('#release-status');
    const sdkTarget = doc.querySelector('#sdk-release-content');
    const release = result.data.latest;
    const el = (tag, cls, text) => {
      const node = doc.createElement(tag);
      if (cls) node.className = cls;
      if (text !== undefined) node.textContent = text;
      return node;
    };
    const link = (text, url, cls='') => { const a=el('a',cls,text); a.href=safeURL(url) || `${HOME}/releases`; return a; };
    const assetInfo = a => `${a.name.split('.').pop().toUpperCase()} · ${size(a.size)} · ${count(a)}`;
    const assetAction = (parent, a, r, label) => {
      const action=el('div','download-action');
      action.append(link(label, a.browser_download_url,'button primary'),el('span','download-size',assetInfo(a)));
      const sum=checksum(r,a);
      if (sum) action.append(link('SHA-256 checksum',sum.browser_download_url,'checksum-link'));
      parent.append(action);
    };
    if (!release) {
      doc.querySelectorAll('[data-release-version]').forEach(n=>{n.textContent='';n.hidden=true;});
      const panel=el('article','download-panel');
      const unavailable=result.state !== 'unpublished';
      panel.append(el('h3','',unavailable ? 'Release information unavailable' : 'AmiHASS hasn’t been released yet.'));
      panel.append(el('p','download-caption',unavailable ? 'GitHub could not be reached. Try refreshing, or check the releases page.' : 'Downloads will appear here automatically when the first GitHub release is published.'));
      panel.append(link('View GitHub releases ↗',`${HOME}/releases`,'text-link'));
      target.replaceChildren(panel);
      if(sdkTarget){
        sdkTarget.replaceChildren(el('h3','','Developer SDK'),el('p','',unavailable ? 'SDK releases could not be checked. Try again later.' : 'The SDK hasn’t been released yet.'));
      }
      const command=doc.querySelector('#checksum-command');
      if(command) command.textContent='shasum -a 256 -c <archive-name>.sha256';
      status.textContent = unavailable ? 'Releases could not be checked. Try again later.' : 'No published GitHub release is available.';
      return;
    }
    const version=release.tag_name;
    doc.querySelectorAll('[data-release-version]').forEach(n=>{n.textContent=version;n.hidden=false;});
    const panel=el('article','download-panel');
    const heading=el('div','download-heading');
    const logo=el('img');logo.src='assets/mark.svg';logo.width=48;logo.height=48;logo.alt='';
    const title=el('div');title.append(el('p','eyebrow','LATEST PUBLISHED RELEASE'),el('h3','',`AmiHASS ${version}`),el('p','',date(release.published_at)));heading.append(logo,title);panel.append(heading);
    const main=primary(release);
    if (main) assetAction(panel,main,release,'Download for AmigaOS ↓');
    else panel.append(el('p','download-caption','No application archive attached. See release notes.'));
    panel.append(el('p','download-caption','Bridge, Preferences, Browser, Dashboard and developer examples.'));
    const extras=files(release).filter(a=>a!==main && !/sdk/i.test(a.name));
    if (extras.length) {
      const list=el('div','asset-list');
      extras.forEach(a=>{const row=el('div','asset-row');const info=el('div');info.append(el('h4','',a.name));row.append(info);assetAction(row,a,release,/sdk/i.test(a.name)?'Download SDK ↓':'Download archive ↓');list.append(row);});panel.append(list);
    }
    const links=el('div','release-links');
    links.append(link('Release notes ↗',release.html_url),link('Source ZIP',`${HOME}/archive/refs/tags/${encodeURIComponent(version)}.zip`),link('Source tar.gz',`${HOME}/archive/refs/tags/${encodeURIComponent(version)}.tar.gz`),link('All releases ↗',`${HOME}/releases`),link('Browse source ↗',`${HOME}/tree/${encodeURIComponent(version)}`));panel.append(links);
    if(sdkTarget){
      const heading=el('h3','','Developer SDK');
      const caption=el('p','','C headers, client helpers, a compiled example and ARexx scripts are included in the main AmiHASS package.');
      sdkTarget.replaceChildren(heading,caption);
      const sdkFiles=files(release).filter(a=>/sdk/i.test(a.name));
      sdkFiles.forEach(a=>assetAction(sdkTarget,a,release,'Download SDK ↓'));
      if(!sdkFiles.length)sdkTarget.append(el('p','','No SDK archive is attached yet.'));
    }
    const nodes=[panel];
    if (result.data.older.length) {
      const details=el('details','older-releases');details.append(el('summary','','Earlier releases'));
      result.data.older.forEach(r=>{const item=el('article','older-release');const head=el('header');head.append(el('strong','',`AmiHASS ${r.tag_name}`),el('small','',date(r.published_at)));item.append(head);const list=el('ul');files(r).filter(a=>!/sdk/i.test(a.name)).forEach(a=>{const row=el('li');row.append(link(a.name,a.browser_download_url),el('small','',` · ${size(a.size)} · ${count(a)}`));const sum=checksum(r,a);if(sum)row.append(doc.createTextNode(' · '),link('SHA-256',sum.browser_download_url));list.append(row);});item.append(list,link('Release notes ↗',r.html_url));details.append(item);});nodes.push(details);
    }
    target.replaceChildren(...nodes);
    const command=doc.querySelector('#checksum-command');
    if (command) command.textContent=main && checksum(release,main) ? `shasum -a 256 -c ${checksum(release,main).name}` : 'shasum -a 256 -c <archive-name>.sha256';
    status.textContent = result.state === 'stale' ?
      `GitHub could not be refreshed. Showing cached release information from ${new Date(result.time).toISOString().slice(0,16).replace('T',' ')} UTC; counts may be out of date.` :
      `Downloads follow the latest published GitHub release. Counts refresh every 15 minutes.${result.historyUnavailable ? ' Earlier releases could not be loaded; use All releases.' : ' Up to 20 recent releases are listed; All releases shows the full history.'}`;
  }
  function init(doc, win) {
    const button=doc.querySelector('#release-refresh');
    let busy=false;
    async function refresh(force=false) {
      if (busy) return;busy=true;if(button)button.disabled=true;
      let storage;
      try {storage=win.localStorage;} catch {storage={getItem:()=>null,setItem:()=>{},removeItem:()=>{}};}
      try {render(doc,await load(win.fetch.bind(win),storage,Date.now(),force));}
      finally {busy=false;if(button)button.disabled=false;}
    }
    if(button)button.addEventListener('click',()=>refresh(true));
    refresh();
    win.setInterval(()=>{if(!doc.hidden)refresh();},TTL);
    doc.addEventListener('visibilitychange',()=>{if(!doc.hidden)refresh();});
  }
  const api={safeURL,assets,files,primary,published,normalize,size,count,checksum,load,render,init,TTL,CACHE};
  if (typeof module !== 'undefined' && module.exports) module.exports=api;
  else {scope.AmiHASSReleases=api;init(scope.document,scope);}
})(typeof window === 'undefined' ? globalThis : window);
