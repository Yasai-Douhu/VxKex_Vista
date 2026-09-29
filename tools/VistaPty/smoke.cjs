const fs=require('fs'),net=require('net');
const addon=require(process.argv[2]);
const output=process.argv[3];
const log=s=>{console.log(s);if(output)fs.appendFileSync(output,s+'\r\n');};
if(output)fs.writeFileSync(output,'');
log('backend='+addon.backend);
const t=addon.startProcess('cmd.exe',80,25,false,'vistapty-test',false,true);
const input=new net.Socket({fd:fs.openSync(t.conin,'w'),readable:false,writable:true});
const stream=net.connect(t.conout);let data='',exitCode;
const timeout=setTimeout(()=>{addon.kill(t.pty,true);log('FAIL timeout');process.exitCode=1;},15000);
stream.on('data',b=>{data+=b.toString('utf8');});
stream.on('connect',()=>{
 const r=addon.connect(t.pty,'cmd.exe /d /q',process.env.SystemRoot,Object.entries(process.env).map(([k,v])=>k+'='+v),true,code=>{exitCode=code;finish();});
 log('pid='+r.pid);addon.resize(t.pty,100,30,true);
 input.write('echo VISTAPTY_OK\r\nexit /b 7\r\n');
});
stream.on('end',finish);
function finish(){if(exitCode===undefined)return;clearTimeout(timeout);input.destroy();stream.destroy();log('exit='+exitCode+' output='+JSON.stringify(data));const ok=exitCode===7&&data.includes('VISTAPTY_OK');log('PASS='+ok);process.exitCode=ok?0:1;}
