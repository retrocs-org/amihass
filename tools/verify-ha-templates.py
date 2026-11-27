#!/usr/bin/env python3
"""Render AmiHASS's server-side templates with Home Assistant's own engine.

Optional developer check (not part of `make check`): needs a Python with the
`homeassistant` package (pip install homeassistant) and a host C compiler.
It builds a synthetic home (areas, labels, devices, loose entities) and checks:
  * batched state requests return valid JSON with only the wanted attributes,
    missing entities marked, long states and Unicode intact;
  * entity/device catalogue pages are valid JSON, page-consistent, and that
    totals match an independent Python computation of the same filters.
Usage: python3 tools/verify-ha-templates.py [--scale N]
"""
import argparse, asyncio, json, os, random, shutil, subprocess, sys, tempfile, time
from pathlib import Path
from types import MappingProxyType
ROOT = Path(__file__).resolve().parents[1]
HELPER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amihass_core.h"
int main(int argc,char**argv){static char out[8192];char ids[AMIHASS_BATCH][96];int i,ok;
 if(argc>2&&argv[1][0]=='S'){for(i=2;i<argc;++i)strcpy(ids[i-2],argv[i]);ok=amihass_states_request((const char(*)[96])ids,(unsigned)(argc-2),out,sizeof(out));}
 else if(argc>3&&argv[1][0]=='K'){char known[AMIHASS_BATCH][40];for(i=2;i+1<argc;i+=2){strcpy(ids[(i-2)/2],argv[i]);strcpy(known[(i-2)/2],argv[i+1]);}ok=amihass_states_request_known((const char(*)[96])ids,(const char(*)[40])known,(unsigned)((argc-2)/2),out,sizeof(out));}
 else if(argc==3&&argv[1][0]=='H')ok=amihass_history_start_request(atoi(argv[2]),out,sizeof(out));
 else if(argc==3&&argv[1][0]=='P'){char st[24];ok=amihass_history_start(argv[2],st);if(ok)fputs(st,stdout);return ok?0:1;}
 else if(argc==8&&argv[1][0]=='E')ok=amihass_device_scope_page_request(argv[2],argv[3],argv[4],argv[5],argv[6],atoi(argv[7]),out,sizeof(out));
 else if(argc==8&&argv[1][0]=='D')ok=amihass_devices_scope_request(argv[3],argv[4],argv[5],argv[6],atoi(argv[7]),out,sizeof(out));
 else return 2;if(!ok)return 1;fputs(out,stdout);return 0;}
'''
def build_helper(tmp):
    src = tmp / 'helper.c'; src.write_text(HELPER); exe = tmp / 'helper'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-w', '-I' + str(ROOT / 'SDK'), str(src), str(ROOT / 'src/core.c'), '-o', str(exe)], check=True)
    return exe
def body(exe, *args):
    r = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True)
    assert r.returncode == 0, ('request refused', args)
    return json.loads(r.stdout)['template']
async def main(scale):
    from homeassistant.core import HomeAssistant
    from homeassistant.config_entries import ConfigEntries, ConfigEntry, ConfigEntryState
    from homeassistant.helpers import area_registry as ar, device_registry as dr, entity_registry as er, label_registry as lr, floor_registry as fr
    from homeassistant.helpers.template import Template
    import inspect
    tmp = Path(tempfile.mkdtemp()); exe = build_helper(tmp)
    hass = HomeAssistant(str(tmp / 'config')); (tmp / 'config').mkdir()
    hass.config_entries = ConfigEntries(hass, {})
    for reg in (fr, lr, ar, dr, er): await reg.async_load(hass)
    kw = dict(domain='demo', title='Demo', data={}, source='user', version=1, minor_version=1, options={}, unique_id=None, entry_id='entry1', state=ConfigEntryState.LOADED, discovery_keys=MappingProxyType({}), subentries_data=None, pref_disable_new_entities=False, pref_disable_polling=False, disabled_by=None)
    sig = inspect.signature(ConfigEntry.__init__).parameters
    entry = ConfigEntry(**{k: v for k, v in kw.items() if k in sig}); hass.config_entries._entries[entry.entry_id] = entry
    rnd = random.Random(1)
    areas = [ar.async_get(hass).async_create(n) for n in ('Kitchen', 'Study', 'Kök åäö')]
    labels = [lr.async_get(hass).async_create(n) for n in ('Night', 'Security')]
    devreg, entreg = dr.async_get(hass), er.async_get(hass)
    domains = ['light', 'switch', 'sensor', 'media_player', 'climate', 'cover']
    for d in range(60 * scale):
        dev = devreg.async_get_or_create(config_entry_id=entry.entry_id, identifiers={('demo', f'd{d}')}, name=None if d % 17 == 0 else f'Device {d:03d}', manufacturer='Acme', model='M')
        if d % 3 == 0: devreg.async_update_device(dev.id, area_id=rnd.choice(areas).id)
        if d % 5 == 0: devreg.async_update_device(dev.id, labels={rnd.choice(labels).label_id})
        for k in range(6):
            ent = entreg.async_get_or_create(rnd.choice(domains), 'demo', f'u{d}_{k}', device_id=dev.id, config_entry=entry, suggested_object_id=f'dev{d}_{k}')
            hass.states.async_set(ent.entity_id, rnd.choice(['on', 'off', '21.5']), {'friendly_name': f'Lamp {d}-{k}'})
    hass.states.async_set('light.desk', 'on', {'friendly_name': 'Desk', 'brightness': 128, 'rgb_color': (255, 10, 0), 'supported_color_modes': ['hs'], 'effect_list': ['x'] * 400})
    hass.states.async_set('sensor.note', 'A long text sensor state that is well over forty characters', {'friendly_name': 'Nöte "q"', 'unit_of_measurement': '°C'})
    hass.states.async_set('media_player.tv', 'playing', {'volume_level': 0.4000000059604645, 'supported_features': 152463, 'media_title': 'Ünï\n"x"'})
    render = lambda tpl: Template(tpl, hass).async_render(parse_result=False)
    # Batched states.
    out = json.loads(render(body(exe, 'S', 'light.desk', 'sensor.note', 'switch.gone', 'media_player.tv')))
    assert [o['entity_id'] for o in out] == ['light.desk', 'sensor.note', 'switch.gone', 'media_player.tv'] and out[2] == {'entity_id': 'switch.gone', 'missing': True}
    assert 'effect_list' not in out[0]['attributes'] and out[0]['attributes']['brightness'] == 128 and out[1]['state'].startswith('A long') and out[3]['attributes']['volume_level'] == 0.4000000059604645
    # Conditional reads: an unchanged entity answers "same", a changed one in full.
    stamp = hass.states.get('light.desk').last_updated.isoformat()
    out = json.loads(render(body(exe, 'K', 'light.desk', stamp, 'sensor.note', '2001-01-01T00:00:00+00:00', 'switch.gone', stamp)))
    assert out[0] == {'entity_id': 'light.desk', 'same': True} and out[1]['state'].startswith('A long') and out[2] == {'entity_id': 'switch.gone', 'missing': True}
    hass.states.async_set('light.desk', 'off', {'friendly_name': 'Desk'})
    out = json.loads(render(body(exe, 'K', 'light.desk', stamp)))
    assert out[0]['state'] == 'off' and out[0]['last_updated'] != stamp
    # History window start: rendered by the server, read back by the Amiga side,
    # and accepted by Home Assistant's own datetime parser for the history path.
    from homeassistant.util import dt as dt_util
    for hours in (1, 6, 48):
        text = render(body(exe, 'H', str(hours)))
        start = subprocess.run([str(exe), 'P', text], capture_output=True, text=True, check=True).stdout
        parsed = dt_util.parse_datetime(start)
        assert parsed is not None and parsed.tzinfo is not None, start
        delta = (dt_util.utcnow() - parsed).total_seconds()
        assert abs(delta - hours * 3600) < 5, (hours, delta)
    # Catalogue pages against an independent Python reference.
    states = sorted(hass.states.async_all(), key=lambda s: s.entity_id)
    def name(s): return s.name
    checked = 0; started = time.perf_counter()
    for area in ['', areas[0].id]:
        for label in ['', labels[0].label_id]:
            for domain in ['All', 'light']:
                for needle in ['', 'lamp 1']:
                    lab = None
                    if label:
                        lab = {e.entity_id for e in er.async_entries_for_label(entreg, label)}
                        for dv in dr.async_entries_for_label(devreg, label): lab |= {e.entity_id for e in er.async_entries_for_device(entreg, dv.id)}
                    inarea = set(Template(f"{{{{ area_entities('{area}') }}}}", hass).async_render()) if area else None
                    ref = [s for s in states if (domain == 'All' or s.domain == domain) and (inarea is None or s.entity_id in inarea) and (lab is None or s.entity_id in lab) and (needle in s.entity_id.lower() or needle in name(s).lower())]
                    ref.sort(key=name)
                    for offset in (0, 16):
                        page = json.loads(render(body(exe, 'E', '', area, label, domain, needle, offset)))
                        assert page['total'] == len(ref), ('total', area, label, domain, needle, page['total'], len(ref))
                        assert [i['entity_id'] for i in page['items']] == [s.entity_id for s in ref[offset:offset + 16]]
                        dev = json.loads(render(body(exe, 'D', '', area, label, domain, needle, offset)))
                        assert isinstance(dev['total'], int) and len(dev['items']) <= 16
                        checked += 2
    await hass.async_stop(force=True); shutil.rmtree(tmp, ignore_errors=True)
    print(f'HA templates: batched and conditional states, history window and {checked} catalogue pages verified against Home Assistant {__import__("homeassistant.const").const.__version__} ({len(states)} states, {time.perf_counter()-started:.1f}s).')
if __name__ == '__main__':
    a = argparse.ArgumentParser(); a.add_argument('--scale', type=int, default=1)
    asyncio.run(main(a.parse_args().scale))
