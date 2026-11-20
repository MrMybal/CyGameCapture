"""Exercise the actual ReShade test application through the shipped CLI and MCP transports.
All configuration, captures and connection files are isolated under build/. No provider account is used.
"""
from pathlib import Path
import subprocess,os,json,time,shutil,uuid,urllib.request
ROOT=Path(__file__).resolve().parents[2]
HELPER=ROOT/'bin/Release/CyGameCaptureAI.exe'
CLI=ROOT/'bin/Release/CyGameCaptureCLI.exe'
WORK=ROOT/'build'/('cymcp-live-'+uuid.uuid4().hex[:8]);WORK.mkdir(parents=True)

def cli(connection,*args,expect=0):
    result=subprocess.run([str(CLI),*args,'--instance',str(connection)],capture_output=True,text=True,encoding='utf-8',timeout=25)
    assert result.returncode==expect,(args,result.returncode,result.stdout,result.stderr)
    value=json.loads(result.stdout)
    if expect: assert 'error' in value and result.stderr.strip()
    return value

def app(writable,compact=False,gui=False,endpoint="",backend="D3D11",profile="protected_access",legacy=False):
    fixture=WORK/(('gui' if gui else 'compact' if compact else 'write' if writable else 'read')+'-'+backend+'-'+(profile or 'implicit'));fixture.mkdir()
    for file in [('CyGameCaptureTestAppD3D12.exe' if backend=='D3D12' else 'CyGameCaptureTestApp.exe'),'dxgi.dll','CyGameCaptureRS.addon64','CyGameCaptureAI.exe','CyGameCaptureMCP.dll']:
        source=ROOT/('bin/Release/TestAppD3D12' if backend=='D3D12' else 'bin/Release/TestApp')/file
        if not source.exists():source=ROOT/'bin/Release'/file
        shutil.copy2(source,fixture/file)
    ini=(ROOT/('Tools/PackageFiles/ini/ReShade-'+backend+'.ini')).read_text(encoding='utf-8-sig')
    ini+='\n[CYGAMECAPTURE]\n' if '[CYGAMECAPTURE]' not in ini else '\n'
    ini+='AutomationEnabled=1\nAutomationAllowMutations='+str(int(writable))+'\nAutomationCompact='+str(int(compact))+'\n'
    if profile is not None:ini+='AutomationAccessProfile='+profile+'\n'
    if legacy:ini+='SenderName=Existing-user-value\nDepthFarPlane=777\n'
    if gui:ini+='OpenOverlayOnStart=1\nAISearchOnStart=120\nAIClient=openai-compatible\nAIEndpoint='+endpoint+'\nAIModel=capture-fixture\n'
    (fixture/'ReShade.ini').write_text(ini,encoding='utf-8')
    env=dict(os.environ);env['LOCALAPPDATA']=str(fixture/'UserData')
    start=subprocess.STARTUPINFO();start.dwFlags|=subprocess.STARTF_USESHOWWINDOW;start.wShowWindow=4 if gui else 0
    process=subprocess.Popen([str(fixture/('CyGameCaptureTestAppD3D12.exe' if backend=='D3D12' else 'CyGameCaptureTestApp.exe')),'-seconds','180'],cwd=fixture,env=env,startupinfo=start)
    folder=Path(env['LOCALAPPDATA'])/'CyGameCapture/Automation'
    deadline=time.monotonic()+25
    while time.monotonic()<deadline:
        found=list(folder.glob('reshade-*.json'))
        if found:return process,found[0]
        if process.poll() is not None:break
        time.sleep(.1)
    log=(fixture/'ReShade.log').read_text(encoding='utf-8',errors='replace') if (fixture/'ReShade.log').exists() else 'No ReShade log'
    if process.poll() is None:process.terminate();process.wait()
    raise AssertionError('No live MCP connection: '+log[-5000:])

def main():
    assert subprocess.run([str(CLI),'--help'],capture_output=True).returncode==0
    assert json.loads(subprocess.check_output([str(CLI),'--version']))['sdk']=='0.7.1'
    assert json.loads(subprocess.check_output([str(CLI),'providers']))['providers']
    invalid=subprocess.run([str(CLI),'call','app.status'],capture_output=True);assert invalid.returncode==2 and invalid.stderr
    for writable in [False,True]:
        process,connection=app(writable)
        try:
            tools=cli(connection,'list')['operations'];assert any(x['name']=='captures.start' for x in tools)
            assert cli(connection,'describe','captures.start')['inputSchema']['required']==['id']
            state=cli(connection,'call','app.status')
            (WORK/'cli-example.json').write_text(json.dumps({'command':[str(CLI),'call','app.status','--instance',str(connection)],'result':state},indent=2))
            for _ in range(60):
                if state['frame']>=40:break
                time.sleep(.05);state=cli(connection,'call','app.status')
            assert state['renderer']=='D3D11' and state['frame']>=40
            buffers=cli(connection,'call','buffers.list','--args','{"limit":128}')['buffers']
            selected=next(x for x in buffers if x['width']==1280 and x['height']==720 and not x['backbuffer'] and x['format']=='R8G8B8A8_UNORM')
            selected_id=selected['id']
            if not writable:
                cli(connection,'call','buffers.select','--args',json.dumps({'id':selected_id}),expect=3)
            else:
                assert cli(connection,'call','buffers.select','--args',json.dumps({'id':selected_id}))['id']==selected_id
                assert cli(connection,'call','app.status')['selected_id']==selected_id
                assert cli(connection,'call','captures.start','--args',json.dumps({'id':selected_id}))['started']
                time.sleep(.3)
                streams=cli(connection,'call','captures.list')['streams'];assert any(s['source_id']==selected_id and s['frames_sent']>0 for s in streams)
                assert cli(connection,'call','captures.stop','--args','{"all":true}')['stopped']
            relay=subprocess.Popen([str(HELPER),'--mcp','--instance',str(connection)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
            def rpc(index,method,params):
                relay.stdin.write(json.dumps({'jsonrpc':'2.0','id':index,'method':method,'params':params})+'\n');relay.stdin.flush()
                reply=json.loads(relay.stdout.readline());assert reply['id']==index,reply;return reply
            assert 'result' in rpc(1,'initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'CyGameCapture local integration test','version':'1'}})
            assert rpc(2,'tools/list',{})['result']['tools']
            assert not rpc(3,'tools/call',{'name':'app.status','arguments':{}})['result'].get('isError',False)
            mutation=rpc(4,'tools/call',{'name':'view.set','arguments':{'selected':True}})['result'];assert mutation.get('isError',False)==(not writable)
            relay.stdin.close();relay.wait(timeout=15);assert relay.returncode==0,relay.stderr.read()
            # A bad bearer token must fail before the registry/queue is entered.
            descriptor=json.loads(connection.read_text());request=urllib.request.Request(descriptor['url'],data=b'{}',headers={'Authorization':'Bearer invalid','Content-Type':'application/json'})
            try:urllib.request.urlopen(request,timeout=5);raise AssertionError('Unauthenticated request accepted')
            except urllib.error.HTTPError as error:assert error.code==401,error.code
            print('PASS actual D3D11 instance:', 'authorized mutation' if writable else 'read-only denial','CLI discovery/schema/state, stdio MCP parity and authentication.',flush=True)
        finally:
            process.terminate();process.wait(timeout=60)
    print('PASS live application integration. Fixtures:',WORK)

if __name__=="__main__":main()
