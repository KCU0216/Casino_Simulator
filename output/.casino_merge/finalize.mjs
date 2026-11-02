import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
const build=path.dirname(fileURLToPath(import.meta.url));
const workspace=path.resolve(build,'../..');
const skill='C:/Users/user1/.codex/plugins/cache/openai-primary-runtime/presentations/26.1013.11914/skills/presentations';
const source='C:/Users/user1/Desktop/CASINO_발표자료_20분_최종.pptx';
process.env.RUNTIME_NODE_MODULES='C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules';
const {finalizePresentation}=await import(pathToFileURL(path.join(skill,'container_tools/artifact_tool_utils.mjs')).href);
const finalPath=path.join(workspace,'output','통합발표자료','CASINO_최종발표_통합본.pptx');
await fs.mkdir(path.dirname(finalPath),{recursive:true});
const originalLink=fs.link;
fs.link=async(src,dest)=>{try{return await originalLink(src,dest);}catch(error){if(error.code!=='EPERM'||dest!==finalPath)throw error;await fs.copyFile(src,dest,1);}};
const result=await finalizePresentation({workspaceDir:workspace,candidatePath:path.join(build,'candidate.pptx'),finalPath,
 pythonExecutable:'C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
 integrityValidatorPath:path.join(skill,'container_tools/inspect_presentation_package_integrity.py'),
 layoutValidatorPath:path.join(skill,'container_tools/inspect_presentation_layout_geometry.py'),
 layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit',...[2,5,20,23].flatMap(n=>['--require-native-table-slide',String(n)])],
 explicitTotalSlideCount:24,requiredNativeTableOwnerSlides:[2,5,20,23],
 fontPolicy:{basis:'reference',families:['Malgun Gothic'],referencePath:source,referenceSha256:crypto.createHash('sha256').update(await fs.readFile(source)).digest('hex')},
 verifyArtifactToolImport:true,receiptPath:path.join(build,'validation.json')});
console.log(JSON.stringify({finalPath,success:result}));
