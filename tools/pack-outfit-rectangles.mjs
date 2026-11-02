const limits={texture:2*1024*1024,pages:1};
export function packRectangles(rectangles,size=1024,height=512,bytesPerPixel=4){
 const pages=[];
 for(const r of [...rectangles].sort((a,b)=>Math.max(b.h,b.w)-Math.max(a.h,a.w)||b.h*b.w-a.h*a.w||a.id-b.id)){
  const w=r.w+2,h=r.h+2;
  if(w>size||h>height)throw Error(`SWL1: art ${r.id} exceeds ${size}x${height} atlas`);
  let best;
  for(const [p,page] of pages.entries())for(const f of page){
   if(w>f.w||h>f.h)continue;
   const score=Math.min(f.w-w,f.h-h)*size+Math.max(f.w-w,f.h-h);
   if(!best||score<best.score)best={p,x:f.x,y:f.y,w,h,score};
  }
  if(!best){
   if((pages.length+1)*size*height*bytesPerPixel>limits.texture||pages.length>=limits.pages)throw Error(`SWL1: room texture budget exceeded (${rectangles.reduce((n,a)=>n+(a.w+2)*(a.h+2),0)} source pixels)`);
   best={p:pages.length,x:0,y:0,w,h};pages.push([{x:0,y:0,w:size,h:height}]);
  }
  Object.assign(r,{page:best.p,x:best.x+1,y:best.y+1});
  const free=[];
  for(const f of pages[best.p]){
   if(best.x>=f.x+f.w||best.x+w<=f.x||best.y>=f.y+f.h||best.y+h<=f.y){free.push(f);continue;}
   if(best.x>f.x)free.push({...f,w:best.x-f.x});
   if(best.x+w<f.x+f.w)free.push({...f,x:best.x+w,w:f.x+f.w-best.x-w});
   if(best.y>f.y)free.push({...f,h:best.y-f.y});
   if(best.y+h<f.y+f.h)free.push({...f,y:best.y+h,h:f.y+f.h-best.y-h});
  }
  pages[best.p]=free.filter((a,i)=>!free.some((b,j)=>i!==j&&a.x>=b.x&&a.y>=b.y&&a.x+a.w<=b.x+b.w&&a.y+a.h<=b.y+b.h&&(i>j||a.x!==b.x||a.y!==b.y||a.w!==b.w||a.h!==b.h)));
 }
 return pages.length;
}
