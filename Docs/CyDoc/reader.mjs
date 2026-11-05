// CyGameCapture host adapter, using the public CyDoc viewer API.
// The local classic-script providers follow CyDoc 0.1.0 web/boot.mjs (MIT).
import { mountViewer } from 'cydoc/viewer';
import { createLink } from 'cydoc';
const data = globalThis.CYDOC_DATA;
const options = globalThis.CYDOC_OPTIONS;
const legacyPages = new Set(['overview','install','quickstart','inspector','moment','ingame','capture','obs','recording','assistant','derived','unreal','configuration','performance','troubleshooting','diagnostics','glossary']);
function normalizeLegacyHash() {
  if (!location.hash || location.hash.startsWith('#/')) return;
  let old='';try{old=decodeURIComponent(location.hash.slice(1)).replace(/^title-/,'');}catch{/* Invalid old anchors return home. */}
  const pageId = legacyPages.has(old) ? old : 'overview';
  history.replaceState(null,'',createLink({locale:'en',pageId},location.href.split('#')[0]));
}
normalizeLegacyHash();
window.addEventListener('hashchange',normalizeLegacyHash);
const pending = new Map();
const loadAsset = src => {
  if (pending.has(src)) return pending.get(src);
  const file = options.assets?.[src];
  if (!file) return Promise.reject(new Error(`Local asset is not packaged: ${src}`));
  const result = new Promise((resolve,reject) => {
    const script = document.createElement('script');script.src = file;
    const clean = () => {pending.delete(src);script.remove();};
    script.onload = () => {const asset=globalThis.CYDOC_ASSETS?.[src];if(globalThis.CYDOC_ASSETS)delete globalThis.CYDOC_ASSETS[src];clean();asset?resolve(asset):reject(new Error(`Invalid local asset: ${src}`));};
    script.onerror = () => {clean();reject(new Error(`Cannot load local asset: ${src}`));};
    document.head.append(script);
  });
  pending.set(src,result);return result;
};
const modules = Object.fromEntries(options.modules.map(name => [name,() => new Promise((resolve,reject) => {
  const script=document.createElement('script');script.src=`media-${name}.js`;
  script.onload=()=>resolve(globalThis[`CyDocMedia_${name}`].default);
  script.onerror=()=>{script.remove();reject(new Error(`Cannot load media-${name}.js`));};
  document.head.append(script);
})]));
let initialTheme = 'nord';
try { initialTheme = localStorage.getItem('cydoc:theme') || initialTheme; } catch { /* Storage can be disabled on file origins. */ }
const viewer = mountViewer(document.querySelector('#app'),data,{homePage:'overview',initialLocale:'en',initialTheme,storageId:'cygamecapture-help',modules,loadAsset});
// Clarify the stock reader's MIT badge: the native application remains AGPL.
// Host-owned labelling only; neither CyDoc nor its generated data is modified.
const licenceLabels = () => {
  const tag=document.querySelector('.mit-tag');
  if(tag && tag.textContent!=='CyDoc MIT')tag.textContent='CyDoc MIT';
  const footer=document.querySelector('.footer > div > span:last-child');
  if(footer && footer.textContent!=='CyDoc reader · MIT')footer.textContent='CyDoc reader · MIT';
};
const labels=new MutationObserver(licenceLabels);labels.observe(document.querySelector('#app'),{childList:true,subtree:true});licenceLabels();
window.addEventListener('pagehide',event=>{if(!event.persisted){labels.disconnect();viewer.destroy();}});
