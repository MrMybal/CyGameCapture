// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import { readFile, writeFile, cp, rm, lstat, realpath } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { build } from 'esbuild';
import { importReferences } from './ImportDocumentation.mjs';
const root = resolve(dirname(fileURLToPath(import.meta.url)),'..');
const config = JSON.parse(await readFile(resolve(root,'cydoc.config.json'),'utf8'));
if (config.out !== 'build/CyDoc-export') throw new Error('Unexpected CyDoc staging directory');
const stage = resolve(root,config.out);
const output = resolve(root,'Docs/UserGuide');
export async function clearGenerated(folder) {
  const allowed = [stage,output,resolve(root,'CyGameCaptureUE/Docs/UserGuide')];
  if (!allowed.includes(folder)) throw new Error(`Refusing to replace non-generated directory: ${folder}`);
  const info=await lstat(folder).catch(e=>{if(e.code!=='ENOENT')throw e;});
  if(info) {
    if(info.isSymbolicLink() || await realpath(folder)!==folder) throw new Error(`Refusing linked output: ${folder}`);
    await rm(folder,{recursive:true});
  }
}
const refs=await importReferences(root);
await clearGenerated(stage);
const command=spawnSync(process.execPath,[resolve(root,'node_modules/cydoc/tools/cli.mjs'),'check',resolve(root,'cydoc.config.json')],{cwd:root,stdio:'inherit'});
if(command.status!==0)throw new Error('CyDoc check/export failed');
// A public mountViewer host, bundled as a classic script, supplies the product theme,
// storage namespace and legacy routes while keeping CyDoc's local resource providers.
await build({entryPoints:[resolve(root,'Docs/CyDoc/reader.mjs')],bundle:true,format:'iife',target:'es2020',minify:true,outfile:resolve(stage,'reader.js'),legalComments:'eof'});
await cp(resolve(root,'Docs/CyDoc/index.template.html'),resolve(stage,'index.html'));
await rm(resolve(stage,'viewer.js')); // Replaced by the host's public-API bootstrap, not patched.
await cp(resolve(root,'LICENCES'),resolve(stage,'notices'),{recursive:true});
for(const [source,target] of [['Resources/CyDoc/LICENSE.txt','CyDoc-MIT.txt'],['LICENSE','CyGameCapture-AGPL-3.0.txt'],['CyGameCaptureUE/LICENSE','CyGameCaptureUE-MIT.txt']]) await cp(resolve(root,source),resolve(stage,'notices',target));
await writeFile(resolve(stage,'notices/CyDoc-SOURCES.txt'),'CyDoc 0.1.0, MIT, commit 774f2a7eabf7dd181a98ffc5add115b580b67f42\nhttps://github.com/MrMybal/CyDoc\nEnabled reader: image. No Three.js, mesh, video, GIF, gallery or graph runtime.\nApplication content and indexed source retain their original licences; see file headers.\n');
const legacyPages=JSON.parse(await readFile(resolve(root,'Docs/CyDoc/migration-inventory.json'),'utf8')).guideChapters.map(c=>c.id);
await writeFile(resolve(stage,'legacy.js'),`const locale=document.documentElement.lang;const pages=${JSON.stringify(legacyPages)};let h=location.hash;let page=decodeURIComponent(h.slice(1)).replace(/^title-/,'');if(!h.startsWith('#/'))h='#/'+locale+'/'+(pages.includes(page)?page:'overview');location.replace('index.html'+h);\n`);
for(const locale of ['en','fr']) await writeFile(resolve(stage,`${locale}.html`),`<!doctype html><html lang="${locale}"><meta charset="utf-8"><title>CyGameCapture</title><script src="legacy.js" defer></script><a href="index.html#/${locale}/overview">CyGameCapture documentation</a></html>\n`);
const manifest=JSON.parse(await readFile(resolve(stage,'manifest.json'),'utf8'));
const details={upstreamCommit:'774f2a7eabf7dd181a98ffc5add115b580b67f42',modules:config.modules,sourceDirectories:config.sources,references:refs.retained,externalSourceLinks:refs.externalSourceLinks,pages:manifest.pages.length};
await writeFile(resolve(stage,'build-info.json'),JSON.stringify(details,null,2)+'\n');
// Replace the old reader only after the new export and all host assets were produced.
await clearGenerated(output);await cp(stage,output,{recursive:true});
const unreal=resolve(root,'CyGameCaptureUE/Docs/UserGuide');
await clearGenerated(unreal);await cp(stage,unreal,{recursive:true});
console.log(`CyGameCapture: ${manifest.pages.length} CyDoc pages exported; image reader only; default English.`);
