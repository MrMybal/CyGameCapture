"""Real helper chat -> explicitly allowlisted MCP peer -> typed results/errors/permission refusal."""
import json,threading,subprocess,time
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from LiveCapture import ROOT,WORK,HELPER,app
from NativeRuntime import Host

def main():
    peer=Host(allow_mutations=True,fixture='peer');process,connection=app(False)
    requests=[];choice='app.status'
    class API(BaseHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_GET(self):self.send_response(200);self.end_headers();self.wfile.write(b'{"data":[{"id":"fixture"}]}')
        def do_POST(self):
            data=json.loads(self.rfile.read(int(self.headers['Content-Length'])));requests.append(data)
            self.send_response(200);self.send_header('Content-Type','text/event-stream');self.end_headers()
            if data['messages'][-1]['role']!='tool':
                tool=next(t['function']['name'] for t in data['tools'] if t['function']['description'].endswith('Local boundary test '+choice) and t['function']['description'].startswith('Authorized tool'))
                delta={'tool_calls':[{'index':0,'id':'peer-call','type':'function','function':{'name':tool,'arguments':'{}'}}]};finish='tool_calls'
            else:delta={'content':'Remote result received.'};finish='stop'
            self.wfile.write(('data: '+json.dumps({'choices':[{'index':0,'delta':delta,'finish_reason':finish}]})+'\n\ndata: [DONE]\n\n').encode());self.wfile.flush()
    server=ThreadingHTTPServer(('127.0.0.1',0),API);threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        allowlist=WORK/'peer-tools.json';allowlist.write_text(json.dumps([{'connection':peer.path,'prefix':'peer','tools':['app.status','fixture.fail','fixture.mutate'],'allow_mutations':True}]))
        for index,operation in enumerate(['app.status','fixture.fail','fixture.mutate']):
            choice=operation;requests.clear();folder=WORK/('peer-turn-'+str(index));folder.mkdir()
            request={'run_id':index+1,'pid':process.pid,'game':'peer-fixture','client':'openai-compatible','endpoint':f'http://127.0.0.1:{server.server_port}/v1','model':'fixture','language':'en','mode':'chat','message':'Call the explicitly authorized peer tool and explain its result.','automation_connection':str(connection),'external_tools_file':str(allowlist),'allow_peer_mutations':True,'buffers':[],'tiles':[]}
            path=folder/'request.json';path.write_text(json.dumps(request));turn=subprocess.run([str(HELPER),str(path)],capture_output=True,timeout=30);assert turn.returncode==0,(turn.stdout,turn.stderr)
            replies=[m for r in requests for m in r['messages'] if m['role']=='tool'];assert replies
            result=json.dumps(replies)
            if operation=='fixture.fail':assert 'FixtureRemoteError' in result,result
            elif operation=='fixture.mutate':assert 'AccessDenied' in result and peer.count==0,result
            else:assert 'count' in result,result
        print('PASS real helper peer tools: explicit allowlist success, typed remote business error in chat, primary live read-only permission prevents peer mutation.',flush=True)
    finally:
        process.terminate();process.wait(timeout=60);peer.close();server.shutdown()

if __name__=='__main__':main()
