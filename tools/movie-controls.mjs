const PURCHASES={buyInventory:0,buyFurniture:1,buyIgloo:2,buyFloor:3,buyIglooFloor:3,upgradeIgloo:2,upgradeIglooFloor:3,buyIglooUpgrade:2};
export function controlCommands(source){
 const adoption=/adoptPuffle\((\d+)\)/.exec(source);if(adoption)return [{op:11,id:Number(adoption[1])}];
 const purchase=/\b(buyInventory|buyFurniture|buyIgloo|buyFloor|buyIglooFloor|upgradeIgloo|upgradeIglooFloor|buyIglooUpgrade)\((\d+)(?:\)|,)/.exec(source);if(purchase)return [{op:10,kind:PURCHASES[purchase[1]],id:Number(purchase[2])}];
 if(/(?:close|closeContent)\(\)/.test(source))return [{op:12}];
 const room=/^\s*(?:SHELL\.closeMail\(\);?\s*)?SHELL\.sendJoinRoom\("(\w+)"/.exec(source);if(room)return [{op:14,room:room[1]}];
 if(source.includes('secret.count == 3'))return [{op:13,target:'secret'}];
 if(/\b(if|for|while)\s*\(/.test(source))return [{op:24}];
 return [...source.matchAll(/([\w.]*?)(nextFrame|prevFrame|gotoAndStop|gotoAndPlay)\(\s*(?:(\d+)|"([^"]+)")?\s*\)/g)].map(([,prefix,command,number,label])=>({op:{gotoAndStop:1,gotoAndPlay:2,nextFrame:3,prevFrame:4}[command],target:prefix.replace(/\.$/,''),frame:number?Number(number):label??0}));
}
export function inspectMovie(movie,seen,catalogue=''){
 const all={0:movie.data.main,...movie.data.sprites},rawAssignments=new Map(),hitIds=new Set(),controls=[],assignments=[];
 const targets=(timeline,frame,parts)=>{
  const result=[];for(const entry of timeline.frames[frame-1]??[])if(entry.name===parts[0]){
   if(parts.length===1)result.push(entry.id);
   else{const child=movie.data.sprites[entry.id];if(child)for(let frame=1;frame<=child.frames.length;frame++)result.push(...targets(child,frame,parts.slice(1)));}
  }return result;
 };
 for(const [id,timeline] of Object.entries(all))for(const [frame,action] of Object.entries(timeline.actions??{})){
  const source=Array.isArray(action)?'':action?.source??'',list=[];
  for(const [,relative,body] of source.matchAll(/([\w.]+)\.onRelease\s*=\s*function\s*\(\)\s*\{([^{}]*)\}/g)){
   const commands=controlCommands(body);if(!commands.length)continue;list.push({relative,commands});for(const target of targets(timeline,Number(frame),relative.split('.')))hitIds.add(target);
  }
  if(list.length)rawAssignments.set(id+':'+frame,list);
 }
 for(const id of seen){const character=movie.data.characters[id];if(character?.kind==='button')hitIds.add(id);}
 for(const timeline of Object.values(all))for(const frame of timeline.frames)for(const entry of frame)if(entry.events?.some(event=>event[0]==='Release'))hitIds.add(entry.id);
 if(catalogue)for(const timeline of Object.values(all))for(const frame of timeline.frames)for(const entry of frame)if(/^(?:[ics][0-4]|tshirtbuy_btn|furnbuy_btn|penguin)$/.test(entry.name??''))hitIds.add(entry.id);
 for(const id of hitIds){if(!seen.has(id))continue;const timeline=movie.data.sprites[id],count=timeline?.frames.length??1;if(count>1024)throw Error('Interactive clip exceeds frame budget');const bounds=Array.from({length:count},(_,i)=>movie.bounds(id,i+1)??null),source=movie.data.buttonActions?.[id]?.release??'';controls.push({id,bounds,commands:controlCommands(source)});}
 return {
  hitIds,controls,assignments,
  collect(symbol,scope,frame){for(const a of rawAssignments.get(symbol+':'+frame)??[])assignments.push({symbol,scope,frame,path:(scope?scope+'.':'')+a.relative,commands:a.commands});},
  result(){return {controls,assignments:[...new Map(assignments.map(a=>[JSON.stringify(a),a])).values()],texts:movie.data.editTexts??{},fonts:movie.data.fonts??{}};},
 };
}
