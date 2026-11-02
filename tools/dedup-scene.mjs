import {createHash} from 'node:crypto';

// Source identity remains part of the key: replacing an instance with a different
// original symbol must still restart its playhead even when both symbols look alike.
export function deduplicateTimelines(timelines,root=0){
 const result={},memo=new Map(),content=new Map(),visiting=new Set();
 const visit=id=>{
  if(memo.has(id))return memo.get(id);
  if(visiting.has(id))throw Error(`Cyclic SWL timeline ${id}`);
  visiting.add(id);const source=timelines[id];
  const frames=source.frames.map(frame=>frame.map(entry=>({...entry,id:timelines[entry.id]?visit(entry.id):entry.id})));
  const value={...source,frames},serialized=JSON.stringify(value),hash=createHash('sha256').update(serialized).digest('hex');
  const candidate=content.get(hash);let canonical=id;
  if(id!==root&&candidate&&candidate.source===(source.originalSymbol??id)&&candidate.serialized===serialized)canonical=candidate.id;
  else{result[id]=value;content.set(hash,{id,source:source.originalSymbol??id,serialized});}
  visiting.delete(id);memo.set(id,canonical);return canonical;
 };
 visit(root);return result;
}

export function internRun(map,buffers,output){
 const bytes=Buffer.concat(buffers),key=createHash('sha256').update(bytes).digest('hex');
 const known=map.get(key);if(known){if(!known.bytes.equals(bytes))throw Error('Scene run hash collision');return known.first;}
 const first=output.length;output.push(...buffers);map.set(key,{first,bytes});return first;
}
