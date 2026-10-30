#!/usr/bin/env python3
"""Optional Google Translate drafts for static UI text. Requires user approval."""
import json
from pathlib import Path
import re
import time
import urllib.parse
import urllib.request
import urllib.error
import sys

folder=Path(__file__).resolve().parents[2]/'mobile/assets/i18n'
for code in ['de','fr','it','es','pt','ja','zh']:
    path=folder/f'{code}.json';catalog=json.loads(path.read_text())
    missing=[key for key,value in catalog.items() if key==value and not re.fullmatch(r'[\W\d]+',key)]
    batches=[];batch=[];size=0
    for key in missing:
        length=len(urllib.parse.quote(key))+30
        if size+length>5000 and batch:batches.append(batch);batch=[];size=0
        batch.append(key);size+=length
    if batch:batches.append(batch)
    updated=0
    for batch in batches:
        protected=[]
        for key in batch:
            value=key
            for index,brand in enumerate(['Arbiter','WebAuthn','Touch ID','Qt','Lucide']):
                value=value.replace(brand,f'ZXBRAND{index}XZ')
            protected.append(value)
        text='\n'.join(f'[ARB{i:03}]\n{value}' for i,value in enumerate(protected))+'\n[ARB999]'
        query=urllib.parse.urlencode({'client':'gtx','sl':'en','tl':code,'dt':'t','q':text})
        request=urllib.request.Request('https://translate.googleapis.com/translate_a/single?'+query,headers={'User-Agent':'Arbiter-UI-Draft-Tool/1.0'})
        try:
            with urllib.request.urlopen(request,timeout=25) as response:
                result=json.load(response)
        except urllib.error.HTTPError as error:
            if error.code==429:
                sys.exit("Google Translate rate limited drafts; existing translations were preserved.")
            raise
        translated=''.join(part[0] for part in result[0] if part and part[0])
        for match in re.finditer(r'\[\s*ARB\s*(\d{3})\s*\](.*?)(?=\[\s*ARB\s*\d{3}\s*\])',translated,re.S):
            index=int(match.group(1))
            if index>=len(batch):continue
            value=match.group(2).strip()
            for n,brand in enumerate(['Arbiter','WebAuthn','Touch ID','Qt','Lucide']):value=value.replace(f'ZXBRAND{n}XZ',brand)
            source=batch[index]
            if not value or 'ZXBRAND' in value or sorted(re.findall(r'\{\d+\}|%\d+',source))!=sorted(re.findall(r'\{\d+\}|%\d+',value)):continue
            catalog[source]=value;updated+=value!=source
        path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
        time.sleep(.4)
    print(f'{code}: {updated} draft translations added',flush=True)
