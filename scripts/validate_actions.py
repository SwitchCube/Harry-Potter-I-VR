"""Check the native input contract against the actual shipped SteamVR files."""
import json
import re
from project_paths import inside


def load_contract():
    source=inside('src/hp1_action_list.h').read_text()
    specs=re.findall(r'\{"([a-z_]+)","([a-z0-9]+)"\}',source)
    if not specs or len(dict(specs))!=len(specs):
        raise ValueError('HP1 action contract is empty or contains duplicate keys')
    return dict(specs)


def validate(contract,manifest,bindings):
    actions={row['name']:row['type'] for row in manifest['actions']}
    if len(actions)!=len(manifest['actions']):
        raise ValueError('Duplicate action in SteamVR manifest')
    required={'/actions/game/in/'+key:kind for key,kind in contract.items()}
    for name,kind in required.items():
        if name not in actions:
            raise ValueError('Native action missing from SteamVR manifest: '+name)
        if actions[name]!=kind:
            raise ValueError('Wrong SteamVR action type: '+name)
    for row in manifest['default_bindings']:
        binding=bindings[row['binding_url']]
        if binding['controller_type']!=row['controller_type']:
            raise ValueError('Binding controller type differs: '+row['binding_url'])
        outputs=set()
        def collect(node):
            if isinstance(node,dict):
                if 'output' in node:
                    outputs.add(node['output'])
                for value in node.values(): collect(value)
            elif isinstance(node,list):
                for value in node: collect(value)
        collect(binding['bindings'])
        unknown=outputs-actions.keys()
        missing=required.keys()-outputs
        if unknown or missing:
            raise ValueError(row['binding_url']+': unknown='+str(sorted(unknown))+' missing='+str(sorted(missing)))


def main():
    contract=load_contract()
    manifest=json.loads(inside('config/vr/actions.json').read_text())
    bindings={row['binding_url']:json.loads(inside('config/vr/'+row['binding_url']).read_text()) for row in manifest['default_bindings']}
    source=inside('src/hp1_actions.h').read_text()
    used=set(re.findall(r'handles\.at\("([a-z_]+)"\)',source))
    used.update(re.findall(r'mappedHandle\("([a-z_]+)"\)',source))
    used.update(re.findall(r'button\("([a-z_]+)"\)',source))
    if used!=contract.keys():
        raise ValueError('Native action references differ from contract: '+str(sorted(used^contract.keys())))
    validate(contract,manifest,bindings)
    print('HP1 SteamVR input contract: %d actions and %d controller bindings verified.' % (len(contract),len(bindings)))


if __name__=='__main__':
    main()
