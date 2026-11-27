# AmiHASS ARexx 0.1

The running bridge exposes the **AMIHASS** command port when
`rexxsyslib.library` is available. Run RexxMast first when using scripts.
The ARexx port uses the same central state, sample provider and action policy
as C clients. It never opens a separate Home Assistant connection.

Enable `OPTIONS RESULTS` in a script, then use `ADDRESS AMIHASS`:

```
options results
address AMIHASS 'STATUS'
if rc = 0 then say result
address AMIHASS 'QUERY light.amihass_sample_living_room'
if rc = 0 then say result
address AMIHASS 'ACTION light.amihass_sample_living_room Toggle'
```

Choose Sample data in Preferences and start the service before trying the
sample entity. Use `RX Status.rexx`, `RX Discover.rexx` or `RX SampleLight.rexx` from this folder.
On some installations RX is in `SYS:Rexxc/`. The supplied light script only
uses reserved sample IDs, so it cannot target a real light in Server data mode.

## Commands

- `HELP`
- `STATUS`
- `DEVICES ["filter"] [offset]`
- `LIST [domain] ["filter"] [offset]`
- `ENTITIES device_id [domain] ["filter"] [offset]`
- `QUERY entity_id`
- `ACTION entity_id "Turn on"` (also Turn off, Toggle and supported Activate);
  for an alarm panel that needs a code: `ACTION entity_id Disarm 1234`
- `ATTRIBUTES entity_id [offset]`: every attribute, 16 lines per page as
  `key<TAB>value<TAB>type` (type is string, number, boolean, list, object or null)
- `HISTORY entity_id [hours] [offset]`: numeric history over 1-48 hours
  (default 24), up to 64 points oldest first, as `time<TAB>value`; an empty
  value marks a time the entity was unavailable
- `NOTIFICATIONS [offset]`: Home Assistant persistent notifications (live
  updates must be on) as `total`, `serial`, `count` and lines
  `id<TAB>title<TAB>created<TAB>message start`; `serial` changes on updates
- `NOTIFICATION id`: one notification as `id`, `title`, `created` and `message`
- `WAITEVENT entity_id[,entity_id...] [seconds] [state]`: wait (default 60 s,
  at most 3600) until the state of the entity, or of any of up to eight
  entities separated by commas, changes, or until it is `state` when given (also
  when it already is). Returns `result=changed` or `result=timeout`, `id` (the entity that
  changed),
  `state`, `previous` and `updated`. Attribute-only updates do not count. One
  WAITEVENT runs at a time (another gets RC 5); other commands keep working
  meanwhile. With live updates enabled changes arrive at once, otherwise
  within about five seconds.

Quote filters and actions containing spaces. Use `""` for an empty filter
before an offset. For example `LIST light "" 16`. Commands are case-insensitive;
entity IDs and action names retain the same case rules as the C SDK.

STATUS returns newline-separated `state`, `ready`, `source` and `selected`
keys. `state=sample` and `source=sample://amihass` explicitly identify simulation.
ATTRIBUTES and HISTORY wait (up to about 30 seconds) while the service fetches
from Home Assistant, then return `total`, `offset`, `count` and the lines; RC 5
means the server was too slow, so try again.
QUERY returns `result`, `id`, `name`, `state` and `unit`. Discovery returns total,
offset and count headers followed by tab-separated ID/name/state rows. Pages
contain at most 16 records. Preserve device IDs for ENTITIES and entity IDs for
QUERY/ACTION. Request results using OPTIONS RESULTS.

Return codes: 0 means a response is available in RESULT; 5 means updating or
busy (retry reads later); 10 means invalid syntax or a rejected operation;
20 means the bridge stopped, is unavailable or timed out. On failures the ARexx
secondary result contains a numeric bridge error where applicable. For QUERY,
unavailable or stale readings return RC 0 with the corresponding numeric
`result` field; they are not permission to control a device. An ACTION that
was not sent returns a non-zero RC: RC 5 (secondary result 5, STALE) when the
bridge has no current reading yet, so QUERY the entity and retry, and RC 10
when the entity is unavailable or the action is unsupported. Unlocking or
opening a lock and opening or positioning a cover return RC 10 with
`state=confirmation_required` unless General Settings > Script doors is on.
When all the bridge's entity slots are in use (48 to 128 depending on free memory), requests return RC 5 with
`state=selection_full`.

ACTION is sent once. RC 0 with `result=2` means accepted (WAIT), not that a real
device already changed. Use QUERY to read it back. Read current state before
controlling real entities; QUERY can return RC 5 on the first request. Never
repeat an action merely because its state readback is pending.

The host permits one outstanding ARexx request; concurrent callers receive
RC 5 while it is busy. Replies are bounded to 4 KB. No token is returned.
If another program owns AMIHASS or ARexx is missing, the C bridge keeps working
but this instance does not create an ARexx port.

Parameter controls use quoted ACTION strings, for example:

```
ACTION light.amihass_sample_living_room "Brightness 40"
ACTION light.amihass_sample_living_room "Color #123456"
ACTION climate.amihass_sample_hallway "Temperature 22.5"
```

Use QUERY first. QUERY results include `control.*` capability values and climate
limits. WAIT requires a later QUERY for state readback. All clients share the
sample provider; these examples never need Home Assistant credentials.
