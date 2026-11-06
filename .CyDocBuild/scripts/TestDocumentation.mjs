// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
// Explicit browser regression command for developer machines and CI. It opens
// local files; run only where browser/automation policy permits file URLs.
import { chromium } from 'playwright';
import { createDocumentation,createLink } from 'cydoc';
import { readFile,mkdir,access } from 'node:fs/promises';
import { resolve,dirname } from 'node:path';
import { fileURLToPath,pathToFileURL } from 'node:url';
import assert from 'node:assert/strict';
const work=resolve(dirname(fileURLToPath(import.meta.url)),'..'),root=resolve(work,'..');
const folder=resolve(process.argv[2]||resolve(root,'Documentation'));
const manifest=JSON.parse(await readFile(resolve(folder,'DocData/manifest.json'),'utf8')),docs=createDocumentation(manifest);
const output=resolve(work,'internal/qa');await mkdir(output,{recursive:true});
let executable=process.env.CYDOC_BROWSER;
if(!executable)for(const path of ['C:/Program Files/Google/Chrome/Application/chrome.exe','C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'])if(await access(path).then(()=>true,()=>false)){executable=path;break;}
assert(executable,'Set CYDOC_BROWSER to an installed browser; this command downloads none.');
const browser=await chromium.launch({executablePath:executable,headless:true});
try{
 const context=await browser.newContext({viewport:{width:1440,height:1000},offline:true,reducedMotion:'reduce'}),page=await context.newPage(),errors=[],network=[];
 page.on('pageerror',error=>errors.push(error.message));page.on('request',request=>{if(!/^(file|data|blob):/.test(request.url()))network.push(request.url());});
 const base=pathToFileURL(resolve(folder,'Documentation.html')).href;
 async function nav(id,locale='en',anchor,state){const route=docs.canonicalRoute({pageId:id,locale,anchor,state}),target=docs.resolve(route.pageId,locale);await page.goto(createLink({pageId:id,locale,anchor,state},base));await page.waitForFunction(({title,locale})=>document.querySelector('.article-title h1')?.textContent===title&&document.documentElement.lang===locale,{title:target.page.title,locale});}
 await page.goto(base);await page.waitForSelector('.article-title h1');assert.equal(await page.locator('html').getAttribute('lang'),'en');
 assert(await page.locator('.header-search [data-header-search]').isVisible());
 assert.equal(await page.locator('.mit-tag,.side-card,.sidebar-footer').count(),0);
 assert((await page.locator('.sidebar').boundingBox()).width>=325,'Expected the widened sidebar');
 for(const original of manifest.pages)await nav(original.id,original.locale);
 await nav('overview');assert(await page.locator('.nav-group .nav-group').count());assert(await page.locator('article svg').count());
 const summary=page.locator('details.nav-group > summary').first();await summary.focus();await summary.press('ArrowLeft');await summary.press('ArrowRight');
 await page.locator('.header-search input').fill('depth');await page.waitForFunction(()=>document.querySelector('#search-results')?.textContent.toLowerCase().includes('depth'));
 await page.keyboard.press('Escape');await page.keyboard.press('Control+k');await page.locator('dialog[open] input[type="search"]').waitFor();await page.keyboard.press('Escape');
 await nav('inspector','en','timeline-and-debug');await page.locator('.locale-control select').selectOption('fr');await page.waitForURL(/#\/fr\/inspector\/timeline-and-debug$/);
 await nav('inspector','fr',undefined,{media:'m1-buffer-inspector',note:'buffer-list'});
 const media=page.locator('[data-cydoc-id="m1-buffer-inspector"]');await media.scrollIntoViewIfNeeded();await media.locator('.media-stage').waitFor();
 await media.locator('.media-stage').press('=');assert.notEqual(await media.locator('.zoom-value').textContent(),'100%');
 await media.locator('[data-action="expand"]').click();await page.waitForSelector('.media-expanded');assert.equal(await page.evaluate(()=>document.fullscreenElement),null);await page.keyboard.press('Escape');
 await page.screenshot({path:resolve(output,'french-reader.png')});
 const link=page.locator('.side-nav a[data-page="inspector"]');await link.focus();await page.waitForSelector('[role="tooltip"]');assert((await page.locator('[role="tooltip"]').innerText()).includes('Trouver'));await page.keyboard.press('Escape');
 await nav('quickstart','fr',undefined,{media:'player-and-output',wipe:'35'});const comparison=page.locator('[data-cydoc-id="player-and-output"]');await comparison.scrollIntoViewIfNeeded();await comparison.locator('.media-stage').waitFor();await comparison.locator('.media-settings > summary').click();assert.equal(await comparison.locator('.media-compare-control input').inputValue(),'35');
 for(const id of ['quickstart','inspector','capture','ingame']){await nav(id);for(const figure of await page.locator('[data-cydoc-module="image"]').all()){await figure.scrollIntoViewIfNeeded();await figure.locator('img').first().waitFor();await figure.locator('img').evaluateAll(images=>Promise.all(images.map(image=>image.decode())));}}
 await nav('configuration');await page.evaluate(()=>window.scrollTo(0,600));await nav('obs');await nav('configuration');await page.waitForFunction(()=>Math.abs(scrollY-600)<5);
 for(const width of [1100,768,390]){await page.setViewportSize({width,height:844});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+2));}
 await page.locator('[data-menu]').click();await page.locator('.side-nav a[data-page="overview"]').click();await page.waitForFunction(()=>document.querySelector('[data-menu]')?.getAttribute('aria-expanded')==='false');
 const touch=await browser.newContext({viewport:{width:390,height:844},hasTouch:true,isMobile:true,offline:true});const mobile=await touch.newPage();await mobile.goto(createLink({locale:'fr',pageId:'quickstart'},base));await mobile.locator('[data-cydoc-module="image"]').scrollIntoViewIfNeeded();await mobile.locator('.media-stage').waitFor();assert(await mobile.locator('.media-toolbar').isVisible());await touch.close();
 const blocked=await browser.newContext({offline:true});await blocked.addInitScript(()=>{Object.defineProperty(window,'localStorage',{get(){throw new DOMException('Disabled','SecurityError');}});});const restricted=await blocked.newPage();await restricted.goto(base);await restricted.waitForSelector('.article-title h1');await blocked.close();
 await page.setViewportSize({width:1440,height:1000});await nav('configuration');await page.pdf({path:resolve(output,'print.pdf'),format:'A4'});
 assert.deepEqual(errors,[]);assert.deepEqual(network,[]);await context.close();
 console.log('PASS file:// browser: every EN/FR page, search/Ctrl+K, tree, media, shared anchors, reading positions, keyboard/touch, layout and print; no remote requests.');
}finally{await browser.close();}
