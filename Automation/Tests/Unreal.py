"""Run inside an isolated Unreal Editor, using its real game ticker and public component API."""
import unreal,threading,subprocess,json,os,time,traceback
from pathlib import Path
root=Path(os.environ['CYGC_TEST_ROOT']);output=Path(os.environ['CYGC_TEST_OUTPUT']);output.mkdir(parents=True,exist_ok=True)
actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.Actor,unreal.Vector())
component=unreal.CyGameCaptureSpoutSenderComponent(outer=actor,name='SDKFixtureSender')
component_path=component.get_path_name();need_write=threading.Event();allowed=threading.Event();done=threading.Event();failure=[]
def cli(connection,*args,expect=0):
    result=subprocess.run([os.environ.get('CYGC_TEST_CLI',str(root/'bin/Release/CyGameCaptureCLI.exe')),*args,'--instance',str(connection)],capture_output=True,text=True,encoding='utf-8',timeout=25)
    assert result.returncode==expect,(args,result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)
def worker():
    try:
        connection=next((Path(os.environ['LOCALAPPDATA'])/'CyGameCapture/Automation').glob('unreal-'+str(os.getpid())+'-*.json'))
        assert len(cli(connection,'list')['operations'])==11
        assert cli(connection,'call','app.status')['component']=='unreal'
        rows=cli(connection,'call','components.list','--args','{"limit":1}')
        assert rows['total']>=1 and len(rows['components'])<=1
        args=json.dumps({'component':component_path,'name':'SDK verified sender name'})
        cli(connection,'call','sender.name','--args',args,expect=3)
        need_write.set();assert allowed.wait(20)
        cli(connection,'call','sender.name','--args',args)
        cli(connection,'call','streams.stop_all')
        cli(connection,'call','sender.start','--args',json.dumps({'component':component_path}),expect=1) # NullRHI is intentionally unsupported.
    except Exception:failure.append(traceback.format_exc())
    finally:done.set()
def tick(_):
    if need_write.is_set() and not allowed.is_set():
        unreal.SystemLibrary.execute_console_command(None,'CyGameCapture.Automation.AllowMutations 1');allowed.set()
    if done.is_set():
        try:
            assert not failure,failure
            assert component.get_editor_property('sender_name')=='SDK verified sender name'
            output.joinpath('result.txt').write_text('PASS actual Unreal editor: real component discovery, paginated schema, read-only denial, authorized game-thread setter, stop and accurate NullRHI failure.\n')
        except Exception:output.joinpath('result.txt').write_text('FAIL '+traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
threading.Thread(target=worker,daemon=True).start()
