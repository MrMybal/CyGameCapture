"""Local OpenAI-compatible protocol fixture; real SDK Conversation and real game tool handlers.
No vendor service, account, paid request or external workflow is used.
"""
from pathlib import Path
import json,threading,time,subprocess,os
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from LiveCapture import app,cli,HELPER,WORK

requests=[];mode='tool'
class API(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        body=json.dumps({'object':'list','data':[{'id':'capture-fixture','object':'model'}]}).encode()
        self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    def do_POST(self):
        data=json.loads(self.rfile.read(int(self.headers['Content-Length'])));requests.append(data)
        self.send_response(200);self.send_header('Content-Type','text/event-stream');self.end_headers()
        def event(delta,finish=None):
            self.wfile.write(('data: '+json.dumps({'id':'local-fixture','object':'chat.completion.chunk','choices':[{'index':0,'delta':delta,'finish_reason':finish}]})+'\n\n').encode());self.wfile.flush()
        try:
            if mode=='slow':
                event({'content':'Waiting for cancellation.'})
                for _ in range(120):time.sleep(.1);event({'content':' '})
            elif data['messages'][-1]['role']!='tool':
                event({'tool_calls':[{'index':0,'id':'capture-call-'+str(len(requests)),'type':'function','function':{'name':next(t['function']['name'] for t in data['tools'] if 'selected' in t['function'].get('parameters',{}).get('properties',{})),'arguments':'{"selected":true}'}}]},'tool_calls')
            else:
                event({'content':'Capture state '});time.sleep(.5);event({'content':'updated.'},'stop')
            self.wfile.write(b'data: [DONE]\n\n');self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError,ConnectionAbortedError):pass

def main():
    global mode
    server=ThreadingHTTPServer(('127.0.0.1',0),API);threading.Thread(target=server.serve_forever,daemon=True).start()
    profile=os.environ.get("CYGC_TEST_ACCESS_PROFILE","protected_access")
    process,connection=app(profile!="free",profile=None if profile=="free" else profile)
    try:
        for _ in range(30):
            state=cli(connection,'call','app.status')
            if state['frame']>40:break
            time.sleep(.1)
        buffers=cli(connection,'call','buffers.list','--args','{"limit":128}')['buffers'];buffer=next(b for b in buffers if b['format']=='R8G8B8A8_UNORM' and not b['backbuffer']);cli(connection,'call','buffers.select','--args',json.dumps({'id':buffer['id']}))
        folder=WORK/'conversation';folder.mkdir()
        secret='private-fixture-key-never-log';env=dict(os.environ);env['CYGC_TEST_KEY']=secret
        def send(run,text):
            request={'run_id':run,'pid':process.pid,'game':'isolated-fixture','client':'openai-compatible','endpoint':'http://127.0.0.1:'+str(server.server_port)+'/v1','model':'capture-fixture','key_env':'CYGC_TEST_KEY','context_bytes':12000,'language':'en','mode':'chat','message':text,'automation_connection':str(connection),'buffers':[],'tiles':[]}
            path=folder/'request.json';path.write_text(json.dumps(request),encoding='utf-8')
            return subprocess.Popen([str(HELPER),str(path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
        turn=send(101,'Switch the isolated test app to the selected-buffer view using the supplied tool.')
        partial=folder/'partial-101.txt';saw_partial=False;deadline=time.monotonic()+30
        while turn.poll() is None and time.monotonic()<deadline:
            if partial.exists() and partial.read_text():saw_partial=True
            time.sleep(.05)
        out,error=turn.communicate(timeout=10);assert turn.returncode==0,(out,error,(folder/'result.txt').read_text())
        assert saw_partial,'No text streaming observed before completion'
        instructions=json.dumps(requests[0]['messages'])
        assert 'Do not run commands or use tools' not in instructions
        assert 'Use the supplied application tools' in instructions
        assert cli(connection,'call','app.status')['selected_view'] is True
        checkpoint=folder/'sessions/current.json';before=json.loads(checkpoint.read_text());assert len(before['messages'])>=4
        assert any(message['role']=='tool' for message in before['messages'])
        turn=send(102,'Perform the same explicit view action, preserving this conversation history.')
        out,error=turn.communicate(timeout=30);assert turn.returncode==0,(out,error)
        after=json.loads(checkpoint.read_text());assert len(after['messages'])>len(before['messages'])
        assert any(req.get('messages',[]) and req['messages'][-1]['role']=='tool' for req in requests)
        assert all(secret not in file.read_text(encoding='utf-8',errors='ignore') for file in folder.rglob('*') if file.is_file()),'Provider credential leaked into application state'
        mode='slow';turn=send(103,'Start a streaming answer; this turn will be cancelled by the user.')
        partial=folder/'partial-103.txt';deadline=time.monotonic()+20
        while not partial.exists() and time.monotonic()<deadline:time.sleep(.05)
        assert partial.exists();(folder/'cancel.txt').write_text('103')
        out,error=turn.communicate(timeout=15);assert turn.returncode!=0
        assert 'Cancelled' in (folder/'result.txt').read_text() or 'Cancelled' in (folder/'status.txt').read_text()
        assert checkpoint.exists()
        print('PASS real helper/SDK chat: incremental text before completion, live game mutation, complete persisted tool history, restored second turn, no stored key and cancellation.',flush=True)
        print('Private fixture:',WORK)
    finally:
        process.terminate();process.wait(timeout=15);server.shutdown()

if __name__=="__main__":main()
