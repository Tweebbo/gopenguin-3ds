/* Isolated real server with a loopback proxy for delayed and missing replies. */
const fs = require('node:fs');
const http = require('node:http');
const { WebSocket, WebSocketServer } = require('/server/node_modules/ws');
require('/server/compiled/server/main.js');
const modes = new Map();
const event = (family, detail) => fs.appendFileSync('/evidence/network.jsonl', JSON.stringify({family, time:Date.now(), ...detail})+'\n');
http.createServer((req,res) => {
    const [,family,mode] = req.url.split('/');
    modes.set(family,{mode, requests:0});
    event(family,{event:'mode',mode});
    res.end('ready\n');
}).listen(24108,'127.0.0.1');
const proxy = new WebSocketServer({port:24109, host:'127.0.0.1'});
proxy.on('connection', (client,request) => {
    const family = request.url.slice(1);
    const upstream = new WebSocket('ws://127.0.0.1:24107');
    const queued = [];
    client.on('message', data => {
        const text = data.toString();
        if(text.includes('%gm%')) event(family,{event:'gm-request',mode:modes.get(family)?.mode});
        if(text.includes('%j#jr%')) event(family,{event:'join',room:Number(text.split('%')[5])});
        if(upstream.readyState===WebSocket.OPEN) upstream.send(data); else queued.push(data);
    });
    upstream.on('open',()=>queued.forEach(data=>upstream.send(data)));
    upstream.on('message', data => {
        let text = data.toString();
        if(text.startsWith('%xt%gm%')) {
            const setting = modes.get(family) ?? {mode:'normal',requests:0};
            setting.requests++;
            event(family,{event:'gm-reply',mode:setting.mode,number:setting.requests});
            if(setting.mode==='timeout'||(setting.mode==='drop'&&setting.requests===1)) return;
            if(setting.mode==='custom') {const parts=text.split('%');parts[5]='2';text=parts.join('%');}
            if(setting.mode==='delay') {setTimeout(()=>{if(client.readyState===WebSocket.OPEN)client.send(text);},4500);return;}
        }
        if(client.readyState===WebSocket.OPEN) client.send(text);
    });
    client.on('close',()=>upstream.close());
    upstream.on('close',()=>client.close());
    upstream.on('error',()=>client.close());
});
const payload = fs.readFileSync('/work/romfs/data/basic-igloo.gpk');
http.createServer((req,res)=>{
    const family=req.url.split('/')[1];
    event(family,{event:'asset',path:req.url});
    if(req.url.includes('/books/900001.gpk')){
        const timer=setTimeout(()=>res.destroy(),25000);res.on('close',()=>clearTimeout(timer));return;
    }
    if(req.url.includes('/movies/200002.gpk')){res.writeHead(200,{'Content-Length':payload.length});res.end(payload);return;}
    res.writeHead(404);res.end();
}).listen(24110,'127.0.0.1');
(async()=>{
    for(let i=0;i<100;i++){
        try {const response=await fetch('http://127.0.0.1:24105/tweebbo/me');if(response.status!==404)break;}catch{}
        await new Promise(resolve=>setTimeout(resolve,100));
    }
    for(const family of ['old','new']) {
        const response=await fetch('http://127.0.0.1:24105/tweebbo/register',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({name:'Ig'+family+Date.now().toString(36).slice(-5),password:crypto.randomUUID()})});
        if(response.status!==201)throw Error('Fixture registration failed: '+response.status);
        const identity=await response.json();
        fs.writeFileSync('/evidence/'+family+'-settings.json',JSON.stringify({api:'http://127.0.0.1:24105',world:'ws://127.0.0.1:24109/'+family,assets:'http://127.0.0.1:24110/'+family,session:identity.session,name:identity.name,audio:false}),{mode:0o600});
    }
    fs.writeFileSync('/evidence/ready','ready\n');
})().catch(error=>{console.error(error);process.exit(1);});
