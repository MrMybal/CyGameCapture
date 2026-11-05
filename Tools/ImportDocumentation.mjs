// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve, dirname, relative, basename } from 'node:path';
import { createHash } from 'node:crypto';

export const sourceId = file => `source-${file.replace(/[^a-zA-Z0-9]+/g, '-').replace(/^-|-$/g, '').toLowerCase()}`;
const slug = value => value.replace(/\{#[\w-]+\}/g,'').normalize('NFD').replace(/\p{M}/gu,'').replace(/[`*_]/g,'').toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'') || 'section';

export async function importReferences(root) {
  const records = JSON.parse(await readFile(resolve(root, 'Docs/CyDoc/references.json'), 'utf8'));
  const byPath = new Map(records.map(item => [resolve(root, item.source).toLowerCase(),item]));
  const retained = [], externalSourceLinks = [];
  for (const record of records) {
    const locale = record.locale || 'en';
    let body = await readFile(resolve(root, record.source), 'utf8');
    const sourceHash = createHash('sha256').update(body).digest('hex');
    if (record.source.endsWith('.txt')) {
      body = `## ${locale==='fr'?'Instructions d’origine':'Original instructions'} {#instructions}\n\n\`\`\`text\n${body.trim()}\n\`\`\`\n`;
    } else {
      // Replace the README's decorative HTML logo; CyDoc escapes raw HTML by design.
      body = body.replace(/<p align="center">[\s\S]*?<\/p>/g,'');
      body = body.replace(/\[(!\[[^\]]*\]\([^)]+\))\]\([^)]+\)/g,'$1');
      // Stable explicit IDs preserve the original headings when text later changes.
      let inFence = false; const ids = new Map();
      body = body.split('\n').map(line => {
        if (/^\s*(```|~~~)/.test(line)) inFence = !inFence;
        if (!inFence && /^#{1,6}\s/.test(line) && !/\{#[\w-]+\}/.test(line)) {
          const base = slug(line.replace(/^#+\s*/,''));
          const count = (ids.get(base) || 0) + 1; ids.set(base,count);
          return line.trimEnd()+` {#${base}${count>1?'-'+count:''}}`;
        }
        return line;
      }).join('\n');
      const rewriteLink = (whole,image,label,href) => {
        if (/^(?:https?:|mailto:|doc:|#\/)/.test(href)) return whole;
        if (href.startsWith('#')) return `[${label}](doc:${record.id}${href})`;
        const [pathPart, fragment] = href.split('#');
        const absolute = resolve(dirname(resolve(root,record.source)),decodeURIComponent(pathPart));
        if (image) return `![${label}](assets/${basename(absolute)})`;
        if (/\/(?:en|fr|index)\.html$/i.test(pathPart) && /UserGuide/.test(pathPart)) return `[${label}](doc:overview)`;
        const target = byPath.get(absolute.toLowerCase());
        if (target) return `[${label}](doc:${target.id}${fragment?'#'+fragment:''})`;
        const rel = relative(root,absolute).replaceAll('\\','/');
        if (rel==='LICENSE') return `[${label}](notices/CyGameCapture-AGPL-3.0.txt)`;
        if (rel==='CyGameCaptureUE/LICENSE') return `[${label}](notices/CyGameCaptureUE-MIT.txt)`;
        if (/^(?:CyGameCaptureCore\/Include\/CyGameCaptureCore|CyGameCaptureUE\/Source\/CyGameCaptureUE\/Public)\/.+\.(?:h|hpp)$/.test(rel)) return `[${label}](doc:${sourceId(rel)}${fragment?'#'+fragment:''})`;
        externalSourceLinks.push({page:record.id,target:rel});
        return `[${label}](https://github.com/MrMybal/CyGameCapture/blob/main/${rel.split('/').map(encodeURIComponent).join('/')}${fragment?'#'+fragment:''})`;
      };
      // C++ lambdas such as [](FCySenderStream& S) are code, not Markdown links.
      let codeFence = '';
      body = body.split('\n').map(line => {
        const fence = line.match(/^\s*(`{3,}|~{3,})/);
        if (fence) { if (!codeFence) codeFence=fence[1];else if(fence[1][0]===codeFence[0] && fence[1].length>=codeFence.length)codeFence='';return line; }
        return codeFence ? line : line.replace(/(!?)\[([^\]]*)\]\(([^)]+)\)/g,rewriteLink);
      }).join('\n');
      if (record.historical) body = '```callout\n'+JSON.stringify({type:'warning',title:'Historical architecture notes',body:'This document preserves the original technical detail. Some early descriptions still call the implemented OBS plugin and D3D12 bridge “future” or “reserved”. Treat those labels as historical, not the current feature status. Profiles remain a plan. See the current [capability matrix](doc:performance) and [user guide](doc:overview).'})+'\n```\n\n'+body;
    }
    const revision = createHash('sha256').update(body).digest('hex').slice(0,12);
    // Match the original of paired EN/FR installation instructions without pretending
    // that English-only developer references have a French translation.
    const original = records.find(item => item.id === record.id && !item.locale);
    let metadata = `---\nid: ${record.id}\ntitle: ${record.title}\ndescription: ${record.source}\nsection: ${record.section}\norder: ${record.order}\n`;
    if (locale==='en') metadata += `revision: ${revision}\n`;
    else if (original) {
      // Build the EN revision on a subsequent pass from the generated Markdown.
      metadata += 'sourceRevision: pending\n';
    }
    metadata += '---\n\n';
    const file = resolve(root,`Docs/CyDoc/manual/${locale}/import-${record.id}.md`);
    await mkdir(dirname(file),{recursive:true});await writeFile(file,metadata+body);
    retained.push({...record,locale,sourceHash,revision});
  }
  for (const item of retained.filter(item=>item.locale!=='en')) {
    const original = retained.find(other=>other.id===item.id && other.locale==='en');
    if (!original) continue;
    const file = resolve(root,`Docs/CyDoc/manual/${item.locale}/import-${item.id}.md`);
    await writeFile(file,(await readFile(file,'utf8')).replace('sourceRevision: pending',`sourceRevision: ${original.revision}`));
  }
  // Compiler-generated heading sets are used by the complete build/check; no HTML
  // fragments or content are written directly into the manifest here.
  return {retained, externalSourceLinks};
}
