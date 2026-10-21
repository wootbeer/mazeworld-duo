"""Build tools/traps.json from multiversal Toolbox defs.
Output: {"traps": {"A9A0": {"name":..., "args":[...], "ret":..., "pascal":bool}},
         "dispatchers": {"Pack3": {"trap":"A9EA","loc":"stack|D0W|...","sel":{"0001":"SFPutFile"}}}}"""
import yaml, glob, json, os, sys

root = os.path.join(os.path.dirname(__file__), '..', 'third_party', 'multiversal', 'defs')
traps, disp = {}, {}
for f in sorted(glob.glob(os.path.join(root, '*.yaml'))):
    for item in yaml.safe_load(open(f)) or []:
        if not isinstance(item, dict): continue
        for kind in ('function', 'dispatcher'):
            if kind not in item: continue
            d = item[kind]
            name = d.get('name')
            if kind == 'dispatcher' and 'trap' in d:
                disp[name] = {'trap': f"{d['trap']:04X}", 'loc': d.get('selector-location', 'stack'), 'sel': {}}
                traps.setdefault(f"{d['trap']:04X}", {'name': name, 'dispatcher': True})
            elif 'trap' in d and 'dispatcher' not in d:
                t = f"{d['trap']:04X}"
                ent = {'name': name, 'ret': d.get('return'),
                       'args': [f"{a.get('name')}:{a.get('type')}" for a in (d.get('args') or [])],
                       'file': os.path.basename(f)}
                # Prefer the first (canonical) name; keep aliases.
                if t in traps and traps[t].get('name') != name:
                    traps[t].setdefault('aliases', []).append(name)
                else:
                    traps[t] = ent
            elif 'dispatcher' in d:
                dn = d['dispatcher']
                sel = d.get('selector')
                if sel is not None:
                    disp.setdefault(dn, {'sel': {}})['sel'][f"{sel:04X}"] = name
json.dump({'traps': traps, 'dispatchers': disp}, open(os.path.join(os.path.dirname(__file__), 'traps.json'), 'w'), indent=1, sort_keys=True)
print(len(traps), 'traps', len(disp), 'dispatchers')
