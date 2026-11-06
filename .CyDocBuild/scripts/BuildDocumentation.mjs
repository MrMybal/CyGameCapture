// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import { readFile, writeFile, cp, rm, lstat, realpath, mkdir } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { importApi } from 'cydoc/api-import';
const work=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(work,'..');
const config=JSON.parse(await readFile(resolve(work,'cydoc.config.json'),'utf8'));
if(config.out!=='../Documentation')throw new Error('Unexpected documentation output');
// Import the documented neutral declarations through CyDoc's public adapter,
// then author source Markdown wrappers for useful descriptions and parent links.
// The actual reader export is always produced by the stock CLI, never patched.
const reviewed=JSON.parse(await readFile(resolve(work,'api/translation.json'),'utf8'));
if(reviewed.sourceRevision!==createHash('sha256').update((await readFile(resolve(work,'api/en.json'),'utf8')).replaceAll('\r\n','\n')).digest('hex'))throw new Error('Review the French public API translation before generating');
const adapters=JSON.parse(await readFile(resolve(work,'api/imports.json'),'utf8'));
const imported=await importApi(adapters,{base:work,locales:config.locales});
for(const item of imported){
 const original=imported.find(page=>page.id===item.id&&page.locale==='en');
 const symbols=JSON.parse(await readFile(resolve(work,`api/${item.locale}.json`),'utf8')).symbols;
 const description=symbols.find(symbol=>symbol.name===item.symbol).description;
 const revision=createHash('sha256').update(original.markdown).digest('hex').slice(0,12);
 const parent=item.symbol.startsWith('cygc::')?'core-api':'ue-subsystem';
 const metadata=`---\nid: ${item.id}\ntitle: ${item.title}\ndescription: ${description}\nsection: ${item.locale==='fr'?'API de développement':'Developer API'}\nparent: ${parent}\nkind: api\norder: 80\n${item.locale==='en'?'revision':'sourceRevision'}: ${revision}\n---\n\n`;
 await writeFile(resolve(work,`manual/${item.locale}/${item.id}.md`),metadata+item.markdown.trimEnd()+'\n');
}
const output=resolve(root,'Documentation');
// Replace recognised generated resources, never unrelated application files.
async function clearData(folder) {
  const allowed=[resolve(output,'DocData'),resolve(root,'CyGameCaptureUE/Documentation/DocData')];
  if(!allowed.includes(folder))throw new Error('Unexpected generated directory');
  if(await lstat(folder).catch(e=>{if(e.code!=='ENOENT')throw e;})) {
    if((await lstat(folder)).isSymbolicLink() || await realpath(folder)!==folder)throw new Error('Linked export refused');
    if(JSON.parse(await readFile(resolve(folder,'manifest.json'),'utf8')).name!=='CyGameCapture')throw new Error('Unrecognised generated data');
    await rm(folder,{recursive:true});
  }
}
await clearData(resolve(output,'DocData'));
const result=spawnSync(process.execPath,[resolve(work,'node_modules/cydoc/tools/cli.mjs'),'check',resolve(work,'cydoc.config.json')],{cwd:work,stdio:'inherit'});
if(result.status!==0)throw new Error('CyDoc generation failed');
// App-owned document shell; reader, providers, styles and modules stay upstream.
await cp(resolve(work,'Documentation.template.html'),resolve(output,'Documentation.html'));
const dataPath=resolve(output,'DocData');
await cp(resolve(root,'LICENCES'),resolve(dataPath,'notices'),{recursive:true});
for(const [source,name] of [['.CyDocBuild/vendor/LICENSE.txt','CyDoc-MIT.txt'],['LICENSE','CyGameCapture-AGPL-3.0.txt'],['CyGameCaptureUE/LICENSE','CyGameCaptureUE-MIT.txt']])await cp(resolve(root,source),resolve(dataPath,'notices',name));
await writeFile(resolve(dataPath,'notices/CyDoc-SOURCES.txt'),await readFile(resolve(work,'vendor/UPSTREAM.txt'),'utf8'));
await writeFile(resolve(output,'DocAI.md'),(await readFile(resolve(output,'DocAI.md'),'utf8'))+'\n'+await readFile(resolve(work,'DocAI.application.md'),'utf8'));
const unreal=resolve(root,'CyGameCaptureUE/Documentation');
await mkdir(unreal,{recursive:true});await clearData(resolve(unreal,'DocData'));await cp(output,unreal,{recursive:true});
console.log('CyGameCapture: complete English/French reader; Documentation.html + DocAI.md + DocData; image module only.');
