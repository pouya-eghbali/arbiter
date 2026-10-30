#!/usr/bin/env python3
"""Validate offline mobile catalogs and generate native provider/permission resources."""
import collections,json,pathlib,plistlib,re,shutil
root=pathlib.Path(__file__).resolve().parents[2];folder=root/'mobile/assets/i18n'
codes=['en','de','fr','it','es','pt','ja','zh'];source=json.loads((folder/'en.json').read_text())
for code in codes:
 data=json.loads((folder/f'{code}.json').read_text())
 if data.keys()!=source.keys():raise ValueError(f'{code}: mismatched message set')
 for key,value in data.items():
  if not value.strip():raise ValueError(f'{code}: empty message')
  if collections.Counter(re.findall(r'\{\d+\}',key))!=collections.Counter(re.findall(r'\{\d+\}',value)):raise ValueError(f'{code}: altered placeholders: {key}')
  for name in ['Arbiter','EasyList','EasyPrivacy','GeckoView','Flutter','Lucide','WebAuthn']:
   if key.count(name)!=value.count(name):raise ValueError(f'{code}: altered brand {name}: {key}')
  if 'ZZAB' in value or 'ZXQ' in value:raise ValueError(f'{code}: draft marker')
 provider=root/'mobile/ios/CredentialProvider/Translations';provider.mkdir(exist_ok=True)
 shutil.copyfile(folder/f'{code}.json',provider/f'{code}.json')
 for target in ['Runner','CredentialProvider']:
  path=root/f'mobile/ios/{target}';info=plistlib.loads((path/'Info.plist').read_bytes())
  localized={k:data[v] for k,v in info.items() if k.endswith('UsageDescription') and isinstance(v,str)}
  out=path/f'{code}.lproj';out.mkdir(exist_ok=True)
  (out/'InfoPlist.strings').write_text('\n'.join(json.dumps(k)+' = '+json.dumps(v,ensure_ascii=False)+';' for k,v in localized.items())+'\n')
 print(code,len(data),'messages')
for target in ['Runner','CredentialProvider']:
 path=root/f'mobile/ios/{target}/Info.plist';info=plistlib.loads(path.read_bytes());info['CFBundleLocalizations']=codes;path.write_bytes(plistlib.dumps(info))
