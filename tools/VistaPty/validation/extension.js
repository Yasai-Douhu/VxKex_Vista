const vscode=require('vscode'),fs=require('fs'),net=require('net'),childProcess=require('child_process');
const dir='C:\\VxKexProbe\\VistaPty\\';
function log(s){fs.appendFileSync(dir+'extension-test.txt',s+'\r\n');}
exports.activate=async function(context){
 fs.writeFileSync(dir+'extension-test.txt','');try{fs.unlinkSync(dir+'powershell-default-result.txt');}catch(_){}log('VERSIONS '+JSON.stringify(process.versions));
 try {
 const addon=require(dir+'conpty.node');log('BACKEND '+addon.backend);
 const t=addon.startProcess('cmd.exe',80,25,false,'test',false,true);
 const input=new net.Socket({fd:fs.openSync(t.conin,'w'),readable:false,writable:true});
 const output=net.connect(t.conout);let data='';output.on('data',b=>data+=b.toString('utf8'));
 output.on('connect',()=>{const r=addon.connect(t.pty,'cmd.exe /d /q','C:\\VxKexProbe',Object.entries(process.env).map(([k,v])=>k+'='+v),true,c=>{log('NATIVE exit='+c+' data='+JSON.stringify(data));input.destroy();output.destroy();});log('NATIVE pid='+r.pid);addon.resize(t.pty,100,30,true);input.write('echo VISTAPTY_NATIVE_OK\r\nexit /b 7\r\n');});
 const terminal=vscode.window.createTerminal({name:'VxKex integrated terminal test',shellPath:'C:\\Windows\\System32\\cmd.exe',shellArgs:['/d','/q'],cwd:'C:\\VxKexProbe'});
 context.subscriptions.push(vscode.window.onDidCloseTerminal(t=>{if(t===terminal)log('IDE CLOSED '+JSON.stringify(t.exitStatus));}));
 terminal.show();terminal.sendText('echo VISTAPTY_IDE_OK > C:\\VxKexProbe\\VistaPty\\ide-result.txt');terminal.sendText('echo VISTAPTY_IDE_OK');
 setTimeout(async()=>{
 log('IDE result='+(fs.existsSync(dir+'ide-result.txt')?fs.readFileSync(dir+'ide-result.txt','utf8'):'MISSING'));
 terminal.sendText('exit /b 7');
 await Promise.all(Array.from({length:6},(_,i)=>nativeTest(addon,'parallel-'+i,'echo PARALLEL_OK\r\nexit /b 9\r\n',9)));
 await nativeTest(addon,'ctrlc','ping -n 30 127.0.0.1\r\n',19,(input)=>{setTimeout(()=>input.write('\x03'),700);setTimeout(()=>input.write('echo CTRL_C_OK\r\nexit /b 19\r\n'),1500);});
 await nativeTest(addon,'unicode','echo 日本語\r\nexit /b 9\r\n',9);
 await nativeTest(addon,'clear','echo BEFORE_CLEAR\r\n',9,(input,id)=>{setTimeout(()=>{try{addon.clear(id,true);log('CLEAR OK');input.write('echo AFTER_CLEAR\r\nexit /b 9\r\n');}catch(e){log('CLEAR ERROR '+e);addon.kill(id,true);}},500);});
 await nativeTest(addon,'kill','ping -n 30 127.0.0.1\r\n',null,(_,id)=>setTimeout(()=>addon.kill(id,true),700));
 const task=new vscode.Task({type:'vistapty-test'},vscode.TaskScope.Workspace,'VistaPty shell task','VxKex',new vscode.ShellExecution('echo VISTAPTY_TASK_OK > C:\\VxKexProbe\\VistaPty\\task-result.txt',{executable:'C:\\Windows\\System32\\cmd.exe',shellArgs:['/d','/c']}));
 context.subscriptions.push(vscode.tasks.onDidEndTaskProcess(e=>{if(e.execution.task.name==='VistaPty shell task')log('TASK exit='+e.exitCode+' result='+(fs.existsSync(dir+'task-result.txt')?fs.readFileSync(dir+'task-result.txt','utf8'):'MISSING'));}));
 try{await vscode.tasks.executeTask(task);}catch(e){log('TASK ERROR '+e);}
 const ready=vscode.window.createTerminal({name:'VxKex WinPTY',shellPath:'C:\\Windows\\System32\\cmd.exe',shellArgs:['/d'],cwd:'C:\\VxKexProbe'});ready.show();ready.sendText('echo VxKex WinPTY integrated terminal is ready.');
 await nativeTest(addon,'powershell-native','Write-Output VISTAPTY_POWERSHELL_NATIVE_OK; exit 23\r\n',23,null,'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe');
 await nativeTest(addon,'powershell-via-cmd','powershell.exe -NoLogo -NoProfile -Command "Write-Output VISTAPTY_POWERSHELL_CMD_OK"\r\nexit /b 42\r\n',42);
 childProcess.execFile('C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-Command','Write-Output VISTAPTY_POWERSHELL_DIRECT_OK'],{timeout:5000},(error,stdout,stderr)=>log('powershell-direct '+JSON.stringify({exit:error&&error.code,stdout,stderr})));
 const psTerminal=vscode.window.createTerminal({name:'VxKex PowerShell test',shellPath:'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe',shellArgs:['-NoLogo','-NoProfile'],cwd:'C:\\VxKexProbe'});
 context.subscriptions.push(vscode.window.onDidCloseTerminal(t=>{if(t===psTerminal)log('POWERSHELL IDE CLOSED '+JSON.stringify(t.exitStatus));}));
 psTerminal.show();psTerminal.sendText("'VISTAPTY_POWERSHELL_IDE_OK' | Out-File C:\\VxKexProbe\\VistaPty\\powershell-ide-result.txt -Encoding ASCII");
 setTimeout(()=>{log('POWERSHELL IDE result='+(fs.existsSync(dir+'powershell-ide-result.txt')?fs.readFileSync(dir+'powershell-ide-result.txt','utf8'):'MISSING'));psTerminal.sendText('exit 29');
 const defaultPs=vscode.window.createTerminal({name:'VxKex default PowerShell test',shellPath:'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe',cwd:'C:\\VxKexProbe'});
 context.subscriptions.push(vscode.window.onDidCloseTerminal(t=>{if(t===defaultPs)log('POWERSHELL DEFAULT CLOSED '+JSON.stringify(t.exitStatus));}));
 defaultPs.show();defaultPs.sendText("'VISTAPTY_POWERSHELL_DEFAULT_OK' | Out-File C:\\VxKexProbe\\VistaPty\\powershell-default-result.txt -Encoding ASCII");
 setTimeout(()=>{log('POWERSHELL DEFAULT result='+(fs.existsSync(dir+'powershell-default-result.txt')?fs.readFileSync(dir+'powershell-default-result.txt','utf8'):'MISSING'));defaultPs.sendText('exit 31');},5000);
 },5000);
 },8000);
 }catch(e){log('ERROR '+e.stack);}
};
function nativeTest(addon,name,command,expected,extra,shell='C:\\Windows\\System32\\cmd.exe'){return new Promise(resolve=>{
 let t,input,output,data='',timer;try{
 t=addon.startProcess(shell,80,25,false,'test',false,true);
 input=new net.Socket({fd:fs.openSync(t.conin,'w'),readable:false,writable:true});output=net.connect(t.conout);output.on('data',b=>data+=b.toString('utf8'));
 timer=setTimeout(()=>{log(name+' FAIL TIMEOUT');addon.kill(t.pty,true);resolve();},10000);
 output.on('connect',()=>{const commandLine=shell.toLowerCase().includes('powershell')?'"'+shell+'" -NoLogo -NoProfile':shell+' /d /q';addon.connect(t.pty,commandLine,'C:\\VxKexProbe',Object.entries(process.env).map(([k,v])=>k+'='+v),true,c=>{clearTimeout(timer);log(name+' exit='+c+' pass='+(expected===null||c===expected)+' data='+JSON.stringify(data));input.destroy();output.destroy();resolve();});addon.resize(t.pty,100,30,true);input.write(command);if(extra)extra(input,t.pty);});
 }catch(e){log(name+' ERROR '+e.stack);clearTimeout(timer);if(t)addon.kill(t.pty,true);resolve();}
});}
