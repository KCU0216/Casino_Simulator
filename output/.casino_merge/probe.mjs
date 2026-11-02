import fs from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {FileBlob, PresentationFile} from 'file:///C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';
const dir = fileURLToPath(new URL('./', import.meta.url));
const p = await PresentationFile.importPptx(await FileBlob.load('C:/Users/user1/Desktop/CASINO_발표자료_20분_최종.pptx'));
console.log('slides',p.slides.items.length);
console.log('shape text',String(p.slides.items[0].shapes.items[0].text));
console.log((await p.help('*',{search:'slide.moveTo|slide.duplicate|shapes.delete|shape.delete|tables.delete',include:['index','examples','notes'],maxChars:5000})).ndjson);
await fs.writeFile(dir+'source-inspect.ndjson',(await p.inspect({kind:'slide,textbox,shape,table,image',maxChars:150000})).ndjson);
for (const i of Array.from({length:20},(_,i)=>i)) {
  const s=p.slides.items[i];
  const blob=await s.export({format:'png',scale:1});
  await fs.writeFile(dir+`source-${i+1}.png`,new Uint8Array(await blob.arrayBuffer()));
}
console.log('rendered');
