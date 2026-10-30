#!/usr/bin/env python3
import json,pathlib
root=pathlib.Path(__file__).resolve().parents[2];folder=root/'mobile/assets/i18n'
source=json.loads((folder/'en.json').read_text());codes=['de','fr','it','es','pt','ja','zh']
catalogs={code:json.loads((folder/f'{code}.json').read_text()) for code in codes}
for line in (root/'tools/mobile/mobile-drafts.tsv').read_text().splitlines():
 if not line or line.startswith('#'):continue
 row=[v.replace(r'\n','\n') for v in line.split('|')]
 if len(row)!=8:raise ValueError('Expected 8 columns: '+row[0])
 for code,text in zip(codes,row[1:]):
  if row[0] in source:catalogs[code][row[0]]=text
for code,data in catalogs.items():
 (folder/f'{code}.json').write_text(json.dumps({k:data[k] for k in source if k in data},ensure_ascii=False,indent=2)+'\n')
print('Missing:')
for key in source:
 if any(key not in data for data in catalogs.values()):print(key)
