"""Load the delivered plugin in real libobs with an isolated live source and D3D11 video thread."""
import ctypes as c,os,json,time,shutil
from pathlib import Path
from LiveCapture import ROOT,WORK,cli,app

def main():
    folder=WORK/'obs';folder.mkdir();os.environ['LOCALAPPDATA']=str(folder/'UserData')
    installed=Path('C:/Program Files/obs-studio');bin=installed/'bin/64bit'
    os.chdir(bin);directory=os.add_dll_directory(str(bin));obs=c.CDLL(str(bin/'obs.dll'))
    def fn(name,args,result):
        value=getattr(obs,name);value.argtypes=args;value.restype=result;return value
    ptr=c.c_void_p;string=c.c_char_p
    startup=fn('obs_startup',[string,string,ptr],c.c_bool);shutdown=fn('obs_shutdown',[],None)
    assert startup(b'en-US',str(folder).encode(),None)
    source=None;settings=None;process=None
    release=fn('obs_source_release',[ptr],None);data_release=fn('obs_data_release',[ptr],None)
    try:
        fn('obs_add_data_path',[string],None)(str(installed/'data/libobs').encode())
        class Video(c.Structure):
            _fields_=[('module',string),('fps_num',c.c_uint32),('fps_den',c.c_uint32),('width',c.c_uint32),('height',c.c_uint32),('out_width',c.c_uint32),('out_height',c.c_uint32),('format',c.c_int),('adapter',c.c_uint32),('gpu',c.c_bool),('colorspace',c.c_int),('range',c.c_int),('scale',c.c_int)]
        video=Video(str(bin/'libobs-d3d11.dll').encode(),30,1,320,180,320,180,6,0,False,2,1,0)
        assert fn('obs_reset_video',[c.POINTER(Video)],c.c_int)(c.byref(video))==0
        module=ptr();plugin=ROOT/'bin/Release/obs-plugins/64bit/CyGameCaptureOBS.dll'
        assert fn('obs_open_module',[c.POINTER(ptr),string,string],c.c_int)(c.byref(module),str(plugin).encode(),str(ROOT/'bin/Release/data/obs-plugins/CyGameCaptureOBS').encode())==0
        assert fn('obs_init_module',[ptr],c.c_bool)(module)
        settings=fn('obs_data_create',[],ptr)();set_bool=fn('obs_data_set_bool',[ptr,string,c.c_bool],None)
        set_bool(settings,b'auto_connect',False)
        source=fn('obs_source_create_private',[string,string,ptr],ptr)(b'cygamecapture_source',b'SDK isolated live source',settings);assert source
        fn('obs_source_inc_active',[ptr],None)(source)
        connection=next((folder/'UserData/CyGameCapture/Automation').glob('obs-source-*.json'))
        assert len(cli(connection,'list')['operations'])==5
        assert cli(connection,'call','source.status')['name']=='SDK isolated live source'
        cli(connection,'call','source.sender','--args','{"sender":"SDK-fixture"}',expect=3)
        set_bool(settings,b'automation_allow_control',True);fn('obs_source_update',[ptr,ptr],None)(source,settings)
        assert cli(connection,'call','source.sender','--args','{"sender":"SDK-fixture"}')['sender']=='SDK-fixture'
        assert cli(connection,'call','source.status')['requested_sender']=='SDK-fixture'
        assert cli(connection,'call','recording.stop')['stopped_sources']==0
        # Real D3D12 producer, GPU copy, Spout discovery and live OBS receiver.
        process,game=app(True,backend='D3D12');state=cli(game,'call','app.status');assert state['renderer']=='D3D12'
        buffers=cli(game,'call','buffers.list','--args','{"limit":128}')['buffers'];target=next(b for b in buffers if b['format']=='R8G8B8A8_UNORM' and not b['backbuffer'])
        cli(game,'call','captures.start','--args',json.dumps({'id':target['id']}));sender=cli(game,'call','captures.list')['streams'][0]['sender']
        cli(connection,'call','source.sender','--args',json.dumps({'sender':sender}))
        for _ in range(70):
            state=cli(connection,'call','source.status')
            if state['connected'] and state['width']>0:break
            time.sleep(.1)
        assert state['connected'] and state['width']==target['width'],state
        cli(game,'call','captures.stop','--args','{"all":true}')
        print('PASS real libobs source: default refusal, authorized source update, shared CLI/MCP schemas, real D3D12 producer -> Spout -> OBS reception and stop.',flush=True)
    finally:
        if process and process.poll() is None:process.terminate();process.wait(timeout=60)
        if source:fn('obs_source_dec_active',[ptr],None)(source);release(source)
        if settings:data_release(settings)
        shutdown();directory.close()

if __name__=='__main__':main()
