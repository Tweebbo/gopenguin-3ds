export function exportMovieFonts(movie,usedArt,art,nextId){
 const styles=[],fields={},byStyle=new Map();
 for(const id of usedArt){
  const field=movie.data.editTexts?.[id];if(!field)continue;
  const key=JSON.stringify([movie.font(field),field.color,field.alpha,field.nativeOutline]);let style=byStyle.get(key);
  if(style===undefined){
   style=styles.length;byStyle.set(key,style);
   const canvas=document.createElement('canvas'),ctx=canvas.getContext('2d'),glyphs={},kern={};ctx.font=movie.font(field);ctx.textBaseline='alphabetic';
   for(let code=32;code<256;code++){
    if(code>=127&&code<160)continue;
    const char=String.fromCharCode(code),metrics=ctx.measureText(char),pad=field.nativeOutline?.width??0,left=Math.floor(-metrics.actualBoundingBoxLeft-pad)-1,top=Math.floor(-metrics.actualBoundingBoxAscent-pad)-1,w=Math.max(1,Math.ceil(metrics.actualBoundingBoxRight+pad)-left+1),h=Math.max(1,Math.ceil(metrics.actualBoundingBoxDescent+pad)-top+1),image=document.createElement('canvas');image.width=w;image.height=h;
    const draw=image.getContext('2d');draw.font=movie.font(field);draw.textBaseline='alphabetic';draw.fillStyle=field.color;draw.globalAlpha=field.alpha;if(field.nativeOutline){draw.lineJoin='round';draw.lineWidth=field.nativeOutline.width;draw.strokeStyle=field.nativeOutline.color;draw.strokeText(char,-left,-top);}draw.fillText(char,-left,-top);
    let stroke=0;if(field.nativeOutline){draw.clearRect(0,0,w,h);draw.strokeText(char,-left,-top);stroke=nextId++;art.push({id:stroke,w,h,left,top,width:w,height:h,png:image.toDataURL(),baked:true});draw.clearRect(0,0,w,h);draw.fillText(char,-left,-top);}
    const id=nextId++;art.push({id,w,h,left,top,width:w,height:h,png:image.toDataURL(),baked:true});glyphs[code]={art:id,stroke,advance:metrics.width};
   }
   const codes=Object.keys(glyphs).map(Number);for(const a of codes)for(const b of codes){const delta=ctx.measureText(String.fromCharCode(a,b)).width-glyphs[a].advance-glyphs[b].advance;if(Math.abs(delta)>.01)kern[a+':'+b]=delta;}
   styles.push({glyphs,kern});
  }
  fields[id]={...field,style};
 }
 return {styles,fields};
}
