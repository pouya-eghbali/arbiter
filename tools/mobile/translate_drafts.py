#!/usr/bin/env python3
"""Create offline draft catalogs from static repository UI text. No user data."""
import json,pathlib,re,time,urllib.parse,urllib.request
root=pathlib.Path(__file__).resolve().parents[2]
folder=root/'mobile/assets/i18n'
source=json.loads((folder/'en.json').read_text())
brands=re.compile(r'Arbiter|Widevine|Touch ID|Windows Hello|WebAuthn|GeckoView|EasyList|EasyPrivacy|Flutter|Lucide|QtKeychain|\{\d+\}')
def protect(text):
 keep={}
 def replace(m):
  token=f'ZZAB{len(keep)}ZZ';keep[token]=m[0];return token
 return brands.sub(replace,text),keep

def request(text,language):
 url='https://translate.googleapis.com/translate_a/single?'+urllib.parse.urlencode({'client':'gtx','sl':'en','tl':language,'dt':'t','q':text})
 for attempt in range(5):
  try:
   with urllib.request.urlopen(url,timeout=40) as response:data=json.load(response)
   return ''.join(part[0] or '' for part in data[0])
  except Exception as error:
   if getattr(error,"code",None)==429:raise RuntimeError("Google is rate-limiting requests; retry later. Existing catalogs are preserved.") from error
   if attempt==4:raise
   time.sleep(1+attempt*2)

def batch(language,items):
 protected=[protect(item) for item in items]
 text='\n\n'.join(f'ZXQ{i}ZXQ\n{pair[0]}' for i,pair in enumerate(protected))
 result=request(text,language)
 chunks=re.split(r'ZXQ\s*(\d+)\s*ZXQ\s*',result)
 mapped={int(chunks[i]):chunks[i+1].strip() for i in range(1,len(chunks)-1,2)}
 output={}
 for i,(original,(_,keep)) in enumerate(zip(items,protected)):
  translated=mapped.get(i,'')
  if not translated or any(token not in translated for token in keep): translated=request(protected[i][0],language)
  for token,value in keep.items():translated=re.sub(re.escape(token),lambda _:value,translated)
  if re.findall(r'\{\d+\}',translated)!=re.findall(r'\{\d+\}',original):
   if sorted(re.findall(r'\{\d+\}',translated))!=sorted(re.findall(r'\{\d+\}',original)):raise RuntimeError(f'Invalid placeholders in {language}')
  if 'ZZAB' in translated or 'ZXQ' in translated:raise RuntimeError(f'Invalid marker in {language}')
  output[original]=translated
 return language,output

languages=['de','fr','it','es','pt','ja','zh']
results={lang:json.loads((folder/f'{lang}.json').read_text()) if (folder/f'{lang}.json').exists() else {} for lang in languages}
for language in languages:
 pending=[s for s in source if s not in results[language]]
 for start in range(0,len(pending),8):
  _,data=batch(language,pending[start:start+8]);results[language].update(data)
  (folder/f'{language}.json').write_text(json.dumps({s:results[language][s] for s in source if s in results[language]},ensure_ascii=False,indent=2)+'\n')
  print(language,len(results[language]),'/',len(source),flush=True)
  time.sleep(1)
