import {readFile,readdir,writeFile} from 'node:fs/promises';
import{dirname,resolve}from'node:path';import{fileURLToPath}from'node:url';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'../../..');
const nm=resolve(root,'tools/mobile/adblock/node_modules');
const paths=[];
for(const item of await readdir(nm)) {
 if(item.startsWith('.'))continue;
 if(item.startsWith('@')){for(const child of await readdir(resolve(nm,item)))paths.push(resolve(nm,item,child));}
 else paths.push(resolve(nm,item));
}
let notices='Arbiter mobile ad blocking\n\nAndroid uses the standalone Ghostery filter engine, not the Ghostery browser extension or service. No Ghostery telemetry or account is included. Sources: https://github.com/ghostery/adblocker/tree/v2.20.3 . The bundled JavaScript is generated without upstream source modifications; the Arbiter wrapper is in tools/mobile/adblock/.\n\niOS WebKit rule lists are converted with Brave adblock-rust from the same source lists. Unsupported WebKit filter features are omitted by the converter.\n\n';
for(const dir of paths.sort()) {
 const meta=JSON.parse(await readFile(resolve(dir,'package.json'),'utf8'));
 if(meta.name==='esbuild'||meta.name.startsWith('@esbuild/'))continue;
 notices+=`\n${'='.repeat(72)}\n${meta.name} ${meta.version}\nLicense: ${meta.license}\nSource: https://registry.npmjs.org/${meta.name}/-/${meta.name.split('/').at(-1)}-${meta.version}.tgz\n`;
 const licenses=(await readdir(dir)).filter(name=>/^(license|licence|copying)/i.test(name));
 if(!licenses.length)throw Error(`Missing license for ${meta.name}`);
 for(const name of licenses)notices+='\n'+await readFile(resolve(dir,name),'utf8')+'\n';
}
for(const path of ['licenses/AdBlock.txt','licenses/EasyList.txt'])notices+='\n'+await readFile(resolve(root,path),'utf8')+'\n';
notices+='\nCompiled and serialized EasyList + EasyPrivacy rule data is an adaptation under CC BY-SA 3.0, credited to the EasyList authors. Unmodified source snapshots and reproducible converters are in the Arbiter repository. https://github.com/pouya-eghbali/arbiter/tree/master/assets/adblock\n';
await writeFile(resolve(root,'mobile/assets/licenses/AdBlock.txt'),notices.split('\n').map(line=>line.trimEnd()).join('\n').trimEnd()+'\n');
