const assert=require('node:assert/strict');
const r=require('../public-web/releases.js');
const repo='https://github.com/retrocs-org/amihass';
const asset=(name,count=17)=>({name,state:'uploaded',size:260000,download_count:count,browser_download_url:`${repo}/releases/download/0.1/${name}`});
const release=(tag='0.1')=>({tag_name:tag,published_at:'2026-11-17T18:00:00Z',html_url:`${repo}/releases/tag/${tag}`,assets:[asset(`AmiHASS-${tag}.zip`),asset(`AmiHASS-${tag}.zip.sha256`),asset(`AmiHASS-${tag}-sdk.zip`,1)]});
const response=(data,status=200)=>({ok:status===200,status,json:async()=>data});
const storage=()=>{const m=new Map();return {getItem:k=>m.get(k)||null,setItem:(k,v)=>m.set(k,v),removeItem:k=>m.delete(k)};};
// Minimal DOM exercises the real renderer, including links and text-only API values.
class Node {
 constructor(tag,text=''){this.tag=tag;this.textContent=text;this.childNodes=[];}
 append(...nodes){this.childNodes.push(...nodes.map(n=>typeof n==='string'?new Node('#text',n):n));}
 replaceChildren(...nodes){this.childNodes=nodes;}
 cloneNode(){const n=new Node(this.tag,this.textContent);n.className=this.className;n.href=this.href;n.childNodes=this.childNodes.map(c=>c.cloneNode(true));return n;}
 get text(){return this.textContent+this.childNodes.map(n=>n.text).join(' ');}
 get links(){return [...(this.href?[this.href]:[]),...this.childNodes.flatMap(n=>n.links)];}
}
const document=()=>{const target=new Node('div');target.append(new Node('p','LOCAL DEVELOPMENT BUILD'));const status=new Node('p'),version=new Node('span'),command=new Node('code'),sdk=new Node('div');return {target,status,version,command,sdk,createElement:t=>new Node(t),createTextNode:s=>new Node('#text',s),querySelector:s=>({'#release-content':target,'#release-status':status,'#checksum-command':command,'#sdk-release-content':sdk}[s]),querySelectorAll:()=>[version]};};
(async()=>{
 const latest=release(),old=release('0.0');
 assert.equal(r.primary(latest).name,'AmiHASS-0.1.zip');
 assert.equal(r.checksum(latest,r.primary(latest)).name,'AmiHASS-0.1.zip.sha256');
 assert.equal(r.count(asset('x',0)),'0 downloads on GitHub');
 assert.equal(r.count(asset('x',1)),'1 download on GitHub');
 assert.equal(r.size(1024),'1 KB');
 for(const url of ['javascript:alert(1)','https://evil.test/download','https://github.com/other/repo/file','https://github.com/retrocs-org/amihass-evil/file'])assert.equal(r.safeURL(url),null);
 assert.equal(r.normalize(latest,[old,{...release('beta'),prerelease:true},{...release('draft'),draft:true},latest]).older.length,1);
 const cache=storage();let calls=0;
 const fetcher=async url=>{calls++;assert(url.startsWith('https://api.github.com/repos/retrocs-org/amihass/releases'));return response(url.endsWith('/latest')?latest:[latest,old]);};
 const result=await r.load(fetcher,cache,1000000);assert.equal(result.state,'fresh');assert.equal(calls,2);
 assert.equal((await r.load(fetcher,cache,1000001)).state,'cached');assert.equal(calls,2);
 const stale=await r.load(async()=>{throw Error('offline');},cache,1000000+r.TTL+1);assert.equal(stale.state,'stale');
 const gone=await r.load(async()=>response(null,404),cache,1000000+r.TTL+2,true);assert.equal(gone.state,'unpublished');assert.equal(cache.getItem(r.CACHE),null);
 for(const status of [403,429,500])assert.equal((await r.load(async()=>response(null,status),storage())).state,'unavailable');
 assert.equal((await r.load(async()=>response({bad:'json'}),storage())).state,'unavailable');
 assert.equal((await r.load(async()=>({ok:true,json:async()=>{throw Error('bad json');}}),storage())).state,'unavailable');
 assert.equal((await r.load(fetcher,{getItem(){throw Error();},setItem(){throw Error();}})).state,'fresh');
 const partial=await r.load(async url=>response(url.endsWith('/latest')?latest:null,url.endsWith('/latest')?200:500),storage());assert(partial.historyUnavailable);
 const aborted=await r.load(async(url,{signal})=>{throw new DOMException('aborted','AbortError');},storage());assert.equal(aborted.state,'unavailable');
 const doc=document();r.render(doc,result);
 assert(doc.target.text.includes('AmiHASS 0.1'));assert(doc.target.text.includes('17 downloads on GitHub'));assert(doc.target.text.includes('Earlier releases'));assert(!doc.target.text.includes('Download SDK'));assert(doc.sdk.text.includes('Download SDK'));assert(doc.sdk.links.includes(latest.assets[2].browser_download_url));
 assert(doc.target.links.includes(latest.assets[0].browser_download_url));assert(doc.target.links.includes(`${repo}/archive/refs/tags/0.1.zip`));assert.equal(doc.version.textContent,'0.1');assert(doc.command.textContent.includes('AmiHASS-0.1.zip.sha256'));
 r.render(doc,stale);assert(doc.status.textContent.includes('cached'));
 r.render(doc,gone);assert(doc.target.text.includes('hasn’t been released yet'));assert(doc.status.textContent.includes('No published GitHub release'));assert(!doc.target.links.some(l=>l.includes('downloads/')));assert(doc.sdk.text.includes('hasn’t been released'));assert.equal(doc.sdk.links.length,0);r.render(doc,{data:{latest:null,older:[]},state:'unavailable'});assert(doc.target.text.includes('Release information unavailable'));assert(!doc.target.text.includes('hasn’t been released'));
 const hostile=release('<img src=x onerror=alert(1)>');hostile.assets.push({...asset('bad.zip'),browser_download_url:'javascript:alert(1)'});r.render(doc,{data:r.normalize(hostile,[]),state:'fresh'});assert(doc.target.text.includes(hostile.tag_name));assert(!doc.target.links.some(l=>l.startsWith('javascript:')));
 const noAssets=release();noAssets.assets=[];r.render(doc,{data:r.normalize(noAssets,[]),state:'fresh'});assert(doc.target.text.includes('No application archive'));
 assert(require('node:fs').readFileSync(require('node:path').join(__dirname,'../public-web/index.html'),'utf8').indexOf('id="downloads"') < require('node:fs').readFileSync(require('node:path').join(__dirname,'../public-web/index.html'),'utf8').indexOf('id="sdk-release-content"'));
 const automatic=document();automatic.addEventListener=()=>{};let autoCalls=0;r.init(automatic,{fetch:async url=>{autoCalls++;return response(url.endsWith('/latest')?latest:[latest]);},localStorage:storage(),setInterval:()=>0});await new Promise(resolve=>setImmediate(resolve));assert.equal(autoCalls,2);assert(automatic.target.text.includes('AmiHASS 0.1'));
 console.log('Website releases: latest/history, assets/counts/checksums, caching, unavailable repository, rate limits, offline fallback, text safety and rendering passed.');
})().catch(e=>{console.error(e);process.exitCode=1;});
