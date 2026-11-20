"""Native C ABI, compact resources, privacy and Conversation regressions. Local fixtures only."""
import ctypes as c,json,time,threading,urllib.request,subprocess
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from LiveCapture import ROOT,WORK,cli

class Host:
    def __init__(self,**options):
        self.folder=WORK/options.pop('fixture','host');self.folder.mkdir(exist_ok=True)
        self.dll=c.CDLL(str(ROOT/'bin/Release/CyGameCaptureMCP.dll'))
        for name,args,result in [('create',[c.c_char_p],c.c_void_p),('add',[c.c_void_p,c.c_char_p,c.c_void_p,c.c_void_p],c.c_int),('connection',[c.c_void_p],c.c_char_p),('pump',[c.c_void_p],None),('destroy',[c.c_void_p],None),('chat_send',[c.c_void_p,c.c_char_p],c.c_int),('chat_events',[c.c_void_p],c.c_char_p),('chat_cancel',[c.c_void_p],None),('chat_clear',[c.c_void_p],None),('chat_history',[c.c_void_p],c.c_char_p)]:
            fn=getattr(self.dll,'cygc_automation_'+name);fn.argtypes=args;fn.restype=result;setattr(self,name,fn)
        self.host=self.create(json.dumps(dict(component='fixture',directory=str(self.folder),**options)).encode());assert self.host
        self.path=self.connection(self.host).decode();self.descriptor=json.loads(open(self.path).read());self.count=0;self.reply=c.create_string_buffer(200000)
        @c.CFUNCTYPE(c.c_void_p,c.c_void_p,c.c_char_p,c.c_char_p)
        def callback(_,name,args):
            name=name.decode();arguments=json.loads(args)
            if name=='app.status':result={'count':self.count}
            elif name=='fixture.large':self.count+=1;result={'secret_content_marker':'sensitive-result-never-measure','blob':'0123456789'*9000}
            elif name=='fixture.mutate':self.count+=1;result={'changed':True}
            elif name=='fixture.fail':result={'error':{'code':'FixtureRemoteError','message':'Expected remote business error'}}
            else:result={'selected':arguments.get('selected',False)};self.count+=1
            self.reply.value=json.dumps(result).encode();return c.addressof(self.reply)
        self.callback=callback
        for name,read,properties in [('app.status',True,{}),('fixture.large',False,{}),('fixture.mutate',False,{}),('fixture.fail',True,{}),('view.set',False,{'selected':{'type':'boolean'}})]:
            spec={'name':name,'description':'Local boundary test '+name,'readOnly':read,'inputSchema':{'type':'object','properties':properties,'additionalProperties':False}}
            assert self.add(self.host,json.dumps(spec).encode(),callback,None)
        self.running=True
        self.thread=threading.Thread(target=self.loop);self.thread.start()
    def loop(self):
        while self.running:self.pump(self.host);time.sleep(.01)
    def rpc(self,method,params):
        params=dict(params);params['_meta']={'io.modelcontextprotocol/protocolVersion':'2026-07-28','io.modelcontextprotocol/clientCapabilities':{}};body=json.dumps({'jsonrpc':'2.0','id':1,'method':method,'params':params}).encode();headers={'Authorization':'Bearer '+self.descriptor.get('token',''),'Content-Type':'application/json','MCP-Protocol-Version':'2026-07-28','Mcp-Method':method};headers.pop('Authorization',None) if not self.descriptor.get('token') else None;headers.update({'Mcp-Name':params['name']} if 'name' in params else {});req=urllib.request.Request(self.descriptor['url'],data=body,headers=headers)
        return json.load(urllib.request.urlopen(req,timeout=20))
    def tool(self,name,args={}):
        value=self.rpc('tools/call',{'name':name,'arguments':args})['result'];assert not value.get('isError'),value;return value['structuredContent']
    def close(self):
        self.running=False;self.thread.join();self.destroy(self.host);assert not __import__('pathlib').Path(self.path).exists()

def main():
    host=Host(compact=True,performance=True,allow_mutations=True,result_ttl_ms=500,cache_ttl_ms=30000)
    try:
        assert len(cli(host.path,'list')['operations'])==5
        assert {x['name'] for x in cli(host.path,'list','--full')['operations']}=={'app.status','fixture.large','fixture.mutate','fixture.fail','view.set'}
        assert cli(host.path,'call','app.status','--full')['count']==0
        assert host.tool('cy_tools_search',{'query':'mutate'})['tools'][0]['name']=='fixture.mutate'
        assert 'inputSchema' in host.tool('cy_tools_describe',{'name':'fixture.mutate'})
        value=host.tool('cy_tools_call',{'name':'fixture.large'})['resource'];assert value['chunked'];count=host.count
        manifest=host.tool('cy_resources_read',{'uri':value['uri']});body=json.loads(manifest['contents'][0]['text']);uri=body['firstUri'];pieces=[]
        while uri:
            chunk=host.tool('cy_resources_read',{'uri':uri});pieces.append(chunk['contents'][0]['text']);uri=chunk.get('_meta',{}).get('com.cymcp/chunk',{}).get('nextUri')
        assert len(json.loads(''.join(pieces))['blob'])==90000
        time.sleep(.7);expired=host.rpc('tools/call',{'name':'cy_resources_read','arguments':{'uri':value['uri']}})['result'];assert expired.get('isError') and host.count==count
        assert host.tool('cy_resources_grep',{'text':'fixture.mutate'})['matches']
        probe=subprocess.run([str(ROOT/'build/Release/tests/CyGameCaptureTransportProbe.exe'),host.path],capture_output=True,text=True,timeout=20);assert probe.returncode==0,(probe.stdout,probe.stderr)
        measures='\n'.join(p.read_text() for p in host.folder.glob('performance-*.jsonl'));assert measures
        assert all(secret not in measures for secret in ['sensitive-result-never-measure','0123456789',host.descriptor.get('token','')] if secret)
        for line in measures.splitlines():
            record=json.loads(line);assert set(record)<={'stage','name','status','elapsed_ms','input_bytes','output_bytes','cache_hit','timestamp_ms'}
        print('PASS compact/full discovery, complete 90 KB chunk retrieval, expiry without mutation replay, native resource grep, cached discovery + live HTTP invalidation, metadata-only JSONL.',flush=True)
    finally:host.close()
    requests=[]
    class API(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_GET(self):
            self.send_response(200);self.end_headers();self.wfile.write(b'{"data":[{"id":"fixture"}]}')
        def do_POST(self):
            requests.append(json.loads(self.rfile.read(int(self.headers['Content-Length']))));self.send_response(200);self.send_header('Content-Type','text/event-stream');self.end_headers()
            self.wfile.write(b'data: {"choices":[{"index":0,"delta":{"content":"Native reply"},"finish_reason":"stop"}]}\n\ndata: [DONE]\n\n');self.wfile.flush()
    server=ThreadingHTTPServer(('127.0.0.1',0),API);threading.Thread(target=server.serve_forever,daemon=True).start()
    host=Host(fixture='chat')
    try:
        def send(text,budget=12000):
            attachment=host.folder/'attachment.txt';attachment.write_text('Native attachment marker')
            request={'attachments':[str(attachment)],'provider':'openai-compatible','endpoint':f'http://127.0.0.1:{server.server_port}/v1','model':'fixture','context_bytes':budget,'text':text}
            assert host.chat_send(host.host,json.dumps(request).encode());events=[];deadline=time.monotonic()+25
            while time.monotonic()<deadline:
                state=json.loads(host.chat_events(host.host));events+=state['events']
                if not state['busy']:break
                time.sleep(.02)
            assert any(e['type']=='TextDelta' for e in events) and any(e['type']=='HostReply' for e in events),events
        send('full-history-marker:'+'A'*10000,20000);send('Second active turn:'+'B'*4000)
        assert 'full-history-marker' not in json.dumps(requests[-1]['messages'])
        assert 'full-history-marker' in host.chat_history(host.host).decode()
        assert len(json.loads(host.chat_history(host.host))['messages'])==4
        assert json.loads(host.chat_history(host.host))['settings']['provider']=='openai-compatible'
        host.close();host=Host(fixture='chat');assert len(json.loads(host.chat_history(host.host))['messages'])==4
        send('Third turn after restarting the native host.')
        assert len(json.loads(host.chat_history(host.host))['messages'])==6
        print('PASS native panel backend: streaming events, attachment, complete history export and restored provider/session after host restart, API context reduction preserves stored history.',flush=True)
    finally:host.close();server.shutdown()

if __name__=='__main__':main()
