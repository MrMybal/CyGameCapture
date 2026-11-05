// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
import { chromium } from 'playwright';
import { createDocumentation, createLink } from 'cydoc';
import { readFile, mkdir, access } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import assert from 'node:assert/strict';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const folder=resolve(process.argv[2] || resolve(root,'Docs/UserGuide'));
const output=resolve(root,'build/CyDoc-QA');await mkdir(output,{recursive:true});
const data=JSON.parse(await readFile(resolve(folder,'manifest.json'),'utf8')),docs=createDocumentation(data);
let executable=process.env.CYDOC_BROWSER;
if(!executable)for(const candidate of ['C:/Program Files/Google/Chrome/Application/chrome.exe','C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe']){
  if(await access(candidate).then(()=>true,()=>false)){executable=candidate;break;}
}
assert(executable,'Set CYDOC_BROWSER to an installed Chromium browser; no browser is downloaded.');
const browser=await chromium.launch({executablePath:executable,headless:true});
try {
  const context=await browser.newContext({viewport:{width:1440,height:1000},offline:true,reducedMotion:'reduce'});
  const page=await context.newPage(),errors=[],network=[];
  page.on('pageerror',e=>errors.push(e.message));
  page.on('request',r=>{if(/^https?:/.test(r.url()))network.push(r.url());});
  const base=pathToFileURL(resolve(folder,'index.html')).href;
  const nav=async (id,locale='en',anchor,state) => {
    const expected=docs.resolve(docs.canonicalRoute({locale,pageId:id}).pageId,locale)?.page;
    assert(expected,`Unknown test page: ${id}/${locale}`);
    await page.goto(createLink({locale,pageId:id,anchor,state},base));
    await page.waitForFunction(title=>document.querySelector('.article-title h1')?.textContent===title,expected.title);
  };
  await page.goto(base);await page.waitForSelector('.article-title h1');
  assert.equal(await page.locator('html').getAttribute('lang'),'en');
  assert.equal(await page.locator('#app').getAttribute('data-theme'),'nord');
  await page.screenshot({path:resolve(output,'english-overview.png')});
  for(const original of data.pages) {
    await nav(original.id,original.locale);
    assert.equal(await page.locator('article').getAttribute('lang'),original.locale);
  }
  await nav('inspector','en','timeline-and-debug');
  await page.locator('.locale-control select').selectOption('fr');
  await page.waitForURL(/#\/fr\/inspector\/timeline-and-debug$/);
  assert.equal(await page.locator('article').getAttribute('lang'),'fr');
  await nav('ue-sender','fr');
  assert.equal(await page.locator('article').getAttribute('lang'),'en');
  assert.match(await page.locator('.callout').first().innerText(),/Traduction indisponible/);
  await nav('depth'); // stable alias to derived#depth
  assert.match(await page.locator('.article-title h1').innerText(),/Depth/);
  await nav('inspector','fr',undefined,{media:'m1-buffer-inspector',note:'buffer-list'});
  const inspector=page.locator('[data-cydoc-id="m1-buffer-inspector"]');
  await inspector.locator('.media-stage').waitFor();
  await page.waitForFunction(()=>document.querySelector('[data-image-note="buffer-list"]')?.getAttribute('aria-pressed')==='true');
  await page.screenshot({path:resolve(output,'french-annotated-inspector.png')});
  await inspector.hover();
  await inspector.locator('.media-stage').press('=');
  assert.notEqual(await inspector.locator('.zoom-value').textContent(),'100%');
  await inspector.locator('.media-settings > summary').click();
  await inspector.locator('[data-action="in"]').press('Enter');
  assert.equal(await inspector.locator('.zoom-value').textContent(),'156%');
  await inspector.locator('[data-action="expand"]').click();
  await page.waitForFunction(()=>!!document.querySelector('.media-expanded'));
  assert.equal(await page.evaluate(()=>document.fullscreenElement),null);
  const bounds=await inspector.boundingBox();assert(bounds.x>=0 && bounds.y>=0 && bounds.width<=1440 && bounds.height<=1000);
  await page.keyboard.press('Escape');assert.equal(await page.locator('.media-expanded').count(),0);
  await nav('quickstart','en',undefined,{media:'player-and-output',wipe:'35'});
  const comparison=page.locator('[data-cydoc-id="player-and-output"]');
  await comparison.locator('.media-stage').waitFor();await comparison.hover();await comparison.locator('.media-settings > summary').click();
  const compare=page.locator('.media-compare-control input[type=range]');await compare.waitFor();
  assert.equal(await compare.inputValue(),'35');await compare.focus();await page.keyboard.press('ArrowRight');assert.equal(await compare.inputValue(),'36');
  // Every original screenshot must actually decode through file://, including lazy media.
  for(const id of ['quickstart','inspector','capture','ingame']) {
    await nav(id);
    for(const figure of await page.locator('[data-cydoc-module="image"]').all()) {
      await figure.scrollIntoViewIfNeeded();await figure.locator('.media-stage img').first().waitFor();
      await figure.locator('img').evaluateAll(imgs=>Promise.all(imgs.map(img=>img.decode())));
    }
  }
  await nav('configuration');await page.evaluate(()=>window.scrollTo(0,600));
  await nav('obs');await nav('configuration');await page.waitForFunction(()=>Math.abs(scrollY-600)<5);
  await page.locator('[data-search]').click();
  await page.locator('.search-form input').fill('depth');
  await page.waitForFunction(()=>document.querySelector('#search-results')?.textContent.includes('Depth'));
  await page.locator('.search-form input').fill('zzzzzzzzzzzzzzzzz');
  await page.waitForFunction(()=>/No results|Aucun résultat/.test(document.querySelector('#search-results')?.textContent||''));
  await page.keyboard.press('Escape');
  for(const locale of ['en','fr']) {
    await page.goto(pathToFileURL(resolve(folder,`${locale}.html`)).href+'#moment');
    await page.waitForURL(new RegExp('#/'+locale+'/moment$'));
    await page.waitForSelector('.article-title h1');assert.equal(await page.locator('html').getAttribute('lang'),locale);
  }
  await page.goto(base+'#title-quickstart');await page.waitForURL(/#\/en\/quickstart$/);
  const nativeUrl=await readFile(resolve(root,'build/native-cydoc-url.txt'),'utf8').catch(()=>null);
  if(nativeUrl){await page.goto(nativeUrl);await page.waitForSelector('.article-title h1');assert.equal(await page.locator('html').getAttribute('lang'),'fr');assert.match(page.url(),/timeline-and-debug\?media=m1-timeline$/);}
  await nav('quickstart','fr');
  await page.setViewportSize({width:390,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+2));
  await page.locator('[data-menu]').click();assert.equal(await page.locator('[data-menu]').getAttribute('aria-expanded'),'true');
  await page.locator('.side-nav a[data-page="moment"]').click();
  // Hash navigation is asynchronous; wait for the new route and its menu state.
  await page.waitForURL(/#\/fr\/moment$/);
  await page.waitForFunction(()=>document.querySelector('[data-menu]')?.getAttribute('aria-expanded')==='false');
  assert.equal(await page.locator('[data-menu]').getAttribute('aria-expanded'),'false');
  await page.screenshot({path:resolve(output,'french-mobile.png')});
  const touch=await browser.newContext({viewport:{width:390,height:844},hasTouch:true,isMobile:true,offline:true});
  const tp=await touch.newPage();await tp.goto(createLink({locale:'fr',pageId:'quickstart'},base));
  await tp.locator('[data-cydoc-module="image"]').scrollIntoViewIfNeeded();await tp.locator('.media-stage').waitFor();
  assert.equal(await tp.locator('.media-toolbar').isVisible(),true);
  await touch.close();
  const restricted=await browser.newContext({offline:true});
  await restricted.addInitScript(()=>{Object.defineProperty(window,'localStorage',{get(){throw new DOMException('Blocked for test','SecurityError');}});});
  const rp=await restricted.newPage();await rp.goto(base);await rp.waitForSelector('.article-title h1');await restricted.close();
  await page.setViewportSize({width:1440,height:1000});await nav('configuration');
  await page.evaluate(()=>{window.print=()=>{window.printTested=true;};});await page.locator('[data-print]').click();
  assert.equal(await page.evaluate(()=>window.printTested),true);
  await page.pdf({path:resolve(output,'print-check.pdf'),format:'A4'});
  assert.deepEqual(errors,[]);assert.deepEqual(network,[]);
  assert.equal(await page.evaluate(()=>typeof globalThis.CyDocMedia_mesh),'undefined');
  console.log('PASS file:// Chromium: every page, EN/FR + fallback, aliases/legacy links, section/media deep links, six decoded screenshots, annotation, keyboard zoom, in-window enlargement/Escape, A/B slider, search, reading position, mobile/touch and print; no network requests.');
} finally {await browser.close();}
