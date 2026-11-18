from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('push_design',root/'tools/analyze_push.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
assert module.subscription([]) is None
assert module.subscription(['light.a','light.a','sensor.b'])['entity_ids']==['light.a','sensor.b']
for bad in (['light.a']*0+['light.bad\nsecret'],['light.'+'x'*96],[f'light.a{i}' for i in range(49)]):
    try:module.subscription(bad)
    except ValueError:pass
    else:raise AssertionError('Invalid/unbounded subscription accepted')
assert len(module.measurements())==4
assert all(row['subscription_bytes']<8192 for row in module.measurements())
print('Push design: explicit selection only, deduplication, limits and bounded subscription frames passed; no live transport enabled.')
