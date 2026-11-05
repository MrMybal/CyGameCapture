// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import assert from 'node:assert/strict';
import { readFile, readdir, lstat } from 'node:fs/promises';
import { resolve, dirname, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { createDocumentation, parseLink, hasAnchor } from 'cydoc';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const canonical=resolve(root,'Docs/UserGuide');
export async function files(folder) {
  const result=[];
  for(const entry of await readdir(folder,{withFileTypes:true})) {
    const path=resolve(folder,entry.name);assert(!entry.isSymbolicLink(),`Linked reader resource: ${path}`);
    if(entry.isDirectory())result.push(...await files(path));else result.push(path);
  }
  return result.sort();
}
const config=JSON.parse(await readFile(resolve(root,'cydoc.config.json'),'utf8'));
assert.deepEqual(config.modules,['image']);assert.equal(config.defaultLocale,'en');
assert.deepEqual(config.sources,['CyGameCaptureCore/Include/CyGameCaptureCore','CyGameCaptureUE/Source/CyGameCaptureUE/Public']);
const data=JSON.parse(await readFile(resolve(canonical,'manifest.json'),'utf8'));
const docs=createDocumentation(data);
const tree=await files(canonical);
const names=tree.map(file=>relative(canonical,file).replaceAll('\\','/'));
assert.deepEqual(names.filter(name=>/^media-.*\.js$/.test(name)),['media-image.js']);
assert(!names.some(name=>/(node_modules|\.tgz$|mesh|three|\.map$|manual\.js$|^style\.css$)/i.test(name)));
for(const file of ['index.html','data.js','reader.js','viewer.css','legacy.js','en.html','fr.html','notices/CyDoc-MIT.txt']) assert(names.includes(file),`Missing reader resource: ${file}`);
const lock=JSON.parse(await readFile(resolve(root,'package-lock.json'),'utf8'));
assert(!lock.packages['node_modules/three'],'Unused Three.js installed');
const inventory=JSON.parse(await readFile(resolve(root,'Docs/CyDoc/migration-inventory.json'),'utf8'));
for(const {id} of inventory.guideChapters) {
  const en=docs.resolve(id,'en'),fr=docs.resolve(id,'fr');assert(en && fr && !fr.fallback,`Missing guide translation: ${id}`);
  assert.deepEqual(en.page.headings.map(h=>h.id),fr.page.headings.map(h=>h.id),`Translated anchors drifted: ${id}`);
  const media=page=>[...page.html.matchAll(/data-cydoc-id="([^"]+)"/g)].map(m=>m[1]);
  assert.deepEqual(media(en.page),media(fr.page),`Translated media IDs drifted: ${id}`);
}
for(const page of data.pages) {
  if(page.source)assert(config.sources.some(dir=>page.source.startsWith(dir+'/')),`Source outside public scope: ${page.source}`);
  for(const m of page.html.matchAll(/href="(#\/[^"\s]+)"/g)) {
    const route=docs.canonicalRoute(parseLink(m[1].replaceAll('&amp;','&'),'en','overview'));
    const target=docs.resolve(route.pageId,route.locale);assert(target,`Broken page in ${page.id}: ${m[1]}`);
    if(route.anchor)assert(hasAnchor(target.page,route.anchor),`Broken anchor in ${page.id}: ${m[1]}`);
    if(route.state?.media)assert(target.page.html.includes(`data-cydoc-id="${route.state.media}"`));
  }
  for(const m of page.html.matchAll(/data-config="([^"]+)"/g)) {
    const c=JSON.parse(m[1].replaceAll('&quot;','"').replaceAll('&#39;',"'").replaceAll('&lt;','<').replaceAll('&gt;','>').replaceAll('&amp;','&'));
    assert.equal(c.controls || 'minimal','minimal',`Non-minimal media: ${page.id}`);
    for(const asset of [c.src,c.compare].filter(Boolean))assert(names.includes(asset),`Missing local media: ${asset}`);
    for(const mark of c.annotations || [])assert(['arrow','rect'].includes(mark.type) && [mark.x,mark.y,mark.toX,mark.toY].every(Number.isFinite));
  }
  for(const m of page.html.matchAll(/(?:href|src)="(notices\/[^"#]+)"/g))assert(names.includes(m[1]),`Missing local notice: ${m[1]}`);
}
const translations=JSON.parse(await readFile(resolve(canonical,'translation-report.json'),'utf8'));
assert.equal(translations.outdated.length,0,'Outdated translations');assert.equal(translations.untracked.length,0,'Untracked translations');
for(const {id} of inventory.guideChapters)assert(!translations.missing.some(item=>item.id===id));
const refs=JSON.parse(await readFile(resolve(root,'Docs/CyDoc/references.json'),'utf8'));
// Examples are literal source content. In particular, do not reinterpret C++ lambdas
// as Markdown links during reference migration.
for(const entry of refs.filter(item=>item.source.endsWith('.md'))) {
  const original=await readFile(resolve(root,entry.source),'utf8');
  const imported=await readFile(resolve(root,`Docs/CyDoc/manual/${entry.locale||'en'}/import-${entry.id}.md`),'utf8');
  for(const block of original.matchAll(/^```[^\n]*\n([\s\S]*?)^```/gm))assert(imported.includes(block[1]),`Changed code example in ${entry.source}`);
}
async function compare(folder) {
  const actual=(await files(folder)).map(file=>relative(folder,file).replaceAll('\\','/'));
  assert.deepEqual(actual,names,`Incomplete/stale reader: ${folder}`);
  for(const name of names)assert((await readFile(resolve(canonical,name))).equals(await readFile(resolve(folder,name))),`Different deployed asset: ${folder}/${name}`);
}
await compare(resolve(root,'CyGameCaptureUE/Docs/UserGuide'));
for(const argument of process.argv.slice(2)) {
  const path=resolve(argument);
  if((await lstat(path)).isDirectory() && (await readdir(path)).some(n=>n.endsWith('.zip'))) {
    const check=spawnSync('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',resolve(root,'Tools/ValidateDocumentationPackages.ps1'),'-ReleaseDirectory',path],{stdio:'inherit'});
    assert.equal(check.status,0,'Packaged documentation validation failed');
  } else if((await readdir(path)).includes('index.html'))await compare(path);
  else {
    const indices=(await files(path)).filter(f=>f.endsWith('UserGuide/index.html') || f.endsWith('UserGuide\\index.html'));
    assert(indices.length,`No guide in ${path}`);for(const index of indices)await compare(dirname(index));
  }
}
console.log(`PASS CyDoc: ${data.pages.length} page/locales, ${names.length} local resources, ${inventory.guideChapters.length} preserved EN/FR chapters; public source scope, code examples, aliases, anchors, media, notices and translation tracking.`);
console.log(`Translation report: ${translations.current} current; ${translations.missing.length} existing English-only references use default-language fallback.`);
