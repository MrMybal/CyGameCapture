// Pages-only compatibility shell; the installed reader remains Documentation.html.
import { cp,rm,mkdir,lstat,realpath } from 'node:fs/promises';
import { resolve,dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
const work=resolve(dirname(fileURLToPath(import.meta.url)),'..'),target=resolve(work,'internal/pages');
if(await lstat(target).catch(e=>{if(e.code!=='ENOENT')throw e;})){if((await lstat(target)).isSymbolicLink()||await realpath(target)!==target)throw new Error('Linked Pages output refused');await rm(target,{recursive:true});}
await mkdir(target,{recursive:true});await cp(resolve(work,'../Documentation'),target,{recursive:true});await cp(resolve(work,'PagesIndex.template.html'),resolve(target,'index.html'));
console.log('Pages staging contains only the complete generated reader and its root-URL compatibility entry.');
