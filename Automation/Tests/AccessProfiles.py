"""Exercise Free and every explicit profile on the real ReShade D3D11 application."""
import json,subprocess,time,urllib.request,urllib.error
from pathlib import Path
from LiveCapture import app,cli,CLI,WORK

def url_cli(url,*args,expect=0):
    result=subprocess.run([str(CLI),*args,'--url',url],capture_output=True,text=True,encoding='utf-8',timeout=25)
    assert result.returncode==expect,(args,result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)

def rpc(url,method,params,token=None,origin=None):
    params={**params,'_meta':{'io.modelcontextprotocol/protocolVersion':'2026-07-28','io.modelcontextprotocol/clientCapabilities':{}}}
    headers={'Content-Type':'application/json','MCP-Protocol-Version':'2026-07-28','Mcp-Method':method}
    if 'name' in params:headers['Mcp-Name']=params['name']
    if token is not None:headers['Authorization']='Bearer '+token
    if origin:headers['Origin']=origin
    request=urllib.request.Request(url,data=json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params}).encode(),headers=headers)
    try:
        with urllib.request.urlopen(request,timeout=20) as response:return response.status,json.load(response)
    except urllib.error.HTTPError as error:return error.code,None

def tool(url,name,args={},token=None):
    status,reply=rpc(url,'tools/call',{'name':name,'arguments':args},token)
    assert status==200,(status,name)
    return reply['result']

def main():
    # A pre-upgrade INI has a legacy permission flag but no explicit access profile.
    process,connection=app(False,profile=None,legacy=True)
    try:
        descriptor=json.loads(connection.read_text());url=descriptor['url']
        assert descriptor['access_profile']=='free' and not descriptor['require_token'] and 'token' not in descriptor
        assert url.startswith('http://127.0.0.1:')
        names=url_cli(url,'list')['operations'];assert len(names)==13
        assert url_cli(url.removesuffix('/mcp'),'call','app.status')['remote_control'] is True
        assert url_cli(url,'call','app.status','--full')['access_profile']=='free'
        for _ in range(50):
            buffers=url_cli(url,'call','buffers.list','--args','{"limit":128}')['buffers']
            selected=next((b for b in buffers if b['format']=='R8G8B8A8_UNORM' and not b['backbuffer']),None)
            if selected:break
            time.sleep(.1)
        assert selected
        assert tool(url,'buffers.select',{'id':selected['id']})['structuredContent']['id']==selected['id']
        assert tool(url,'captures.start',{'id':selected['id']})['structuredContent']['started']
        time.sleep(.2)
        assert any(s['frames_sent']>0 for s in url_cli(url,'call','captures.list')['streams'])
        assert tool(url,'captures.stop',{'all':True})['structuredContent']['stopped']
        assert tool(url,'buffers.select',{'id':'not-an-integer'}).get('isError') is True
        url_cli(url,'call','buffers.select','--args','{"id":"bad"}',expect=2)
        config=url_cli(url,'mcp-config')['mcpServers']['CyGameCapture'];assert config['args']==['--mcp','--url',url]
        relay=subprocess.Popen([str(CLI),'--mcp','--url',url],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
        for index,method,params in [(1,'initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'local profile test','version':'1'}}),(2,'tools/call',{'name':'view.set','arguments':{'selected':True}})]:
            relay.stdin.write(json.dumps({'jsonrpc':'2.0','id':index,'method':method,'params':params})+'\n');relay.stdin.flush()
            reply=json.loads(relay.stdout.readline());assert 'result' in reply and not reply['result'].get('isError'),reply
        relay.stdin.close();relay.wait(timeout=15);assert relay.returncode==0
        assert url_cli(url,'call','app.status')['selected_view'] is True
        assert rpc(url,'tools/list',{},origin='https://example.com')[0]==403
        ini=(connection.parents[3]/'ReShade.ini').read_text(encoding='utf-8',errors='replace')
        assert 'Existing-user-value' in ini and '777' in ini
        print('PASS migration without explicit profile -> Free: address-only CLI/HTTP/stdio reads, real buffer selection/capture/frames/stop, no permission grant, argument validation and preserved settings.',flush=True)
    finally:process.terminate();process.wait(timeout=60)
    for profile,token_required,controlled in [('free',False,False),('local_controlled',False,True),('token_only',True,False),('protected_access',True,True)]:
        process,connection=app(False,profile=profile)
        try:
            descriptor=json.loads(connection.read_text());url=descriptor['url'];token=descriptor.get('token')
            assert descriptor['access_profile']==profile and descriptor['require_token']==token_required
            assert cli(connection,'call','app.status')['access_profile']==profile
            if token_required:
                assert token and rpc(url,'tools/list',{})[0]==401
                assert rpc(url,'tools/list',{},'invalid')[0]==401
            else:assert token is None and rpc(url,'tools/list',{})[0]==200
            result=tool(url,'view.set',{'selected':True},token)
            assert result.get('isError',False)==controlled,(profile,result)
            assert cli(connection,'call','app.status')['remote_control']==(not controlled)
            print('PASS explicit profile preserved:',profile,'token required:',token_required,'business controls:',controlled,flush=True)
        finally:process.terminate();process.wait(timeout=60)
    print('PASS local access profiles on real application. Fixtures:',WORK,flush=True)

if __name__=='__main__':main()
