// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import assert from 'node:assert/strict';
import { readFile,readdir,lstat } from 'node:fs/promises';
import { resolve,dirname,relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { createDocumentation,parseLink,hasAnchor } from 'cydoc';
const work=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(work,'..'),canonical=resolve(root,'Documentation');
export async function files(folder){const result=[];for(const entry of await readdir(folder,{withFileTypes:true})){const path=resolve(folder,entry.name);assert(!entry.isSymbolicLink(),`Linked exported resource: ${path}`);if(entry.isDirectory())result.push(...await files(path));else result.push(path);}return result.sort();}
const manifest=JSON.parse(await readFile(resolve(canonical,'DocData/manifest.json'),'utf8'));
const docs=createDocumentation(manifest),config=JSON.parse(await readFile(resolve(work,'cydoc.config.json'),'utf8'));
assert.deepEqual((await readdir(canonical)).sort(),['DocAI.md','DocData','Documentation.html']);
assert.equal(manifest.defaultLocale,'en');assert.deepEqual(docs.availableLocales().map(x=>x.code),['en','fr']);assert.deepEqual(config.modules,['image']);assert.equal(config.apiIndex.enabled,false);
const tree=await files(canonical),names=tree.map(f=>relative(canonical,f).replaceAll('\\','/'));
const shell=await readFile(resolve(canonical,'Documentation.html'),'utf8');
for(const match of shell.matchAll(/(?:src|href)="([^"]+)"/g))assert(names.includes(match[1]),`Missing or non-local reader shell resource: ${match[1]}`);
for(const alias of Object.keys(config.aliases))for(const locale of ['en','fr']){
  const route=docs.canonicalRoute(parseLink(`#/${locale}/${alias}`,'en','overview')),target=docs.resolve(route.pageId,route.locale);
  assert(target&&!target.fallback,`Missing alias destination: ${locale}/${alias}`);
  if(route.anchor)assert(hasAnchor(target.page,route.anchor),`Missing alias anchor: ${locale}/${alias}`);
}
assert.deepEqual(names.filter(n=>/media-.*\.js$/.test(n)),['DocData/media-image.js']);
assert(!names.some(n=>/\.CyDocBuild|node_modules|\/src\/|\.tgz$|\.cpp$|\.hpp$|\.h$|media-mesh|\.map$/.test(n)));
const lock=JSON.parse(await readFile(resolve(work,'package-lock.json'),'utf8'));assert(!lock.packages['node_modules/three']);
const excluded=['translation-report','documentation-maintenance','architecture','ue-architecture','ue-performance','changelog','licence-sources','distribution'];
const english=manifest.pages.filter(p=>p.locale==='en'),french=manifest.pages.filter(p=>p.locale==='fr');assert.equal(english.length,french.length);
for(const page of english){const translated=docs.resolve(page.id,'fr');assert(translated&&!translated.fallback);assert.notEqual(page.text,translated.page.text,`Untranslated page: ${page.id}`);assert.deepEqual(page.headings.map(h=>h.id),translated.page.headings.map(h=>h.id),`Anchor mismatch: ${page.id}`);assert.equal(page.parent,translated.page.parent,`Parent mismatch: ${page.id}`);}
assert(manifest.pages.filter(p=>p.parent).length>=40,'Missing task hierarchy');
for(const page of manifest.pages){
  assert(page.kind!=='source'&&!page.sourcePageId,'Raw source browsing must not be distributed');assert(!excluded.includes(page.id));
  assert(!/detail::MapFrameSyncBlock|class FrameSyncPublisher|migration-inventory|@doc\s+native-help-api/.test(page.html),'Implementation or internal content leaked');
  for(const match of page.html.matchAll(/href="(#\/[^"\s]+)"/g)){const route=docs.canonicalRoute(parseLink(match[1].replaceAll('&amp;','&'),'en','overview')),target=docs.resolve(route.pageId,route.locale);assert(target,`Missing page: ${match[1]}`);if(route.anchor)assert(hasAnchor(target.page,route.anchor),`Missing anchor: ${match[1]}`);if(route.state?.media)assert(target.page.html.includes(`data-cydoc-id="${route.state.media}"`));}
  for(const match of page.html.matchAll(/data-config="([^"]+)"/g)){const media=JSON.parse(match[1].replaceAll('&quot;','"').replaceAll('&#39;',"'").replaceAll('&lt;','<').replaceAll('&gt;','>').replaceAll('&amp;','&'));assert.equal(media.controls,'minimal');for(const name of [media.src,media.compare].filter(Boolean))assert(names.includes('DocData/'+name),`Missing media: ${name}`);}
}
const ai=await readFile(resolve(canonical,'DocAI.md'),'utf8');assert(ai.includes('CyGameCapture integration'));assert(!manifest.pages.some(p=>p.text.includes('No publishing or Git push is authorised')));
const translations=JSON.parse(await readFile(resolve(canonical,'DocData/translation-report.json'),'utf8'));assert.equal(translations.missing.length,0);assert.equal(translations.outdated.length,0);
const enApi=JSON.parse(await readFile(resolve(work,'api/en.json'),'utf8')),frApi=JSON.parse(await readFile(resolve(work,'api/fr.json'),'utf8'));
assert.deepEqual(enApi.symbols.map(s=>[s.key,s.signature]),frApi.symbols.map(s=>[s.key,s.signature]));
assert.equal(JSON.parse(await readFile(resolve(work,'api/translation.json'),'utf8')).sourceRevision,createHash('sha256').update(await readFile(resolve(work,'api/en.json'))).digest('hex'));
async function compare(folder){const actual=(await files(folder)).map(f=>relative(folder,f).replaceAll('\\','/'));assert.deepEqual(actual,names,`Unexpected or missing delivered resources: ${folder}`);for(const name of names)assert((await readFile(resolve(folder,name))).equals(await readFile(resolve(canonical,name))),`Changed delivered resource: ${folder}/${name}`);}
await compare(resolve(root,'CyGameCaptureUE/Documentation'));
for(const arg of process.argv.slice(2)){const folder=resolve(arg);if((await readdir(folder)).some(n=>n.endsWith('.zip'))){const checked=spawnSync('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',resolve(work,'scripts/ValidateDocumentationPackages.ps1'),'-ReleaseDirectory',folder],{stdio:'inherit'});assert.equal(checked.status,0);}else if((await readdir(folder)).includes('Documentation.html'))await compare(folder);else{const starts=(await files(folder)).filter(f=>f.endsWith('Documentation.html'));assert(starts.length);for(const path of starts)await compare(dirname(path));}}
console.log(`PASS CyDoc: ${english.length} complete English/French pages, shared anchors and parents; ${names.length} resources; image only; API declarations without implementation listings; AI/internal material outside the reader.`);
