"""Disposable read-only TLS/WebSocket fixture. Never connects to Home Assistant."""
import contextlib,http.server,ipaddress,json,ssl,subprocess,tempfile,threading,hashlib,base64,socket,struct,time,secrets
from pathlib import Path
@contextlib.contextmanager
def endpoint(camera_image=None):
    host=subprocess.check_output(['/usr/sbin/ipconfig','getifaddr','en0'],text=True).strip()
    host=str(ipaddress.IPv4Address(host));token='amihass_fixture_'+secrets.token_hex(16);counts={'actions':0,'subscriptions':[],'rest':0,'errors':[],'camera_images':0,'camera_sizes':{}}
    with tempfile.TemporaryDirectory(prefix='amihass-ws-') as directory:
        root=Path(directory);cert=root/'ca.pem';key=root/'key.pem'
        subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-subj','/CN=AmiHASS fixture','-addext','subjectAltName=IP:'+host,'-keyout',str(key),'-out',str(cert)],check=True,capture_output=True)
        def send(w,body):
            payload=json.dumps(body,separators=(',',':')).encode();w.sendall(bytes([0x81,len(payload)])+payload if len(payload)<126 else b'\x81\x7e'+struct.pack('!H',len(payload))+payload)
        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_POST(self):counts['actions']+=1;self.send_error(405)
            def do_GET(self):
                if self.path.startswith('/api/camera_proxy/'):
                    sizes={'/api/camera_proxy/camera.fixture?width=320&height=200':'320x200','/api/camera_proxy/camera.fixture?width=96&height=60':'96x60'}
                    size=sizes.get(self.path);image=camera_image.get(size) if isinstance(camera_image,dict) else camera_image
                    allowed=size and self.headers.get('Authorization')=='Bearer '+token and image is not None
                    if not allowed:self.send_error(403);return
                    counts['camera_images']+=1;counts['camera_sizes'][size]=counts['camera_sizes'].get(size,0)+1;self.send_response(200);self.send_header('Content-Type','image/png');self.send_header('Content-Length',str(len(image)));self.end_headers();self.wfile.write(image);return
                if self.path=='/api/websocket':
                    try:
                        accept=base64.b64encode(hashlib.sha1((self.headers['Sec-WebSocket-Key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
                        self.send_response(101);self.send_header('Upgrade','websocket');self.send_header('Connection','Upgrade');self.send_header('Sec-WebSocket-Accept',accept);self.end_headers();self.wfile.flush();self.connection.settimeout(10)
                        def receive():
                            first=self.rfile.read(2)
                            if len(first)!=2:raise EOFError()
                            size=first[1]&127
                            if size==126:size=struct.unpack('!H',self.rfile.read(2))[0]
                            if size>8192 or not first[1]&128:raise ValueError('bad client frame')
                            mask=self.rfile.read(4);payload=self.rfile.read(size)
                            return json.loads(bytes(c^mask[i%4] for i,c in enumerate(payload)))
                        send(self.connection,{'type':'auth_required','ha_version':'fixture'})
                        auth=receive();assert auth=={'type':'auth','access_token':token};send(self.connection,{'type':'auth_ok','ha_version':'fixture'})
                        selection=receive();assert selection['type']=='subscribe_entities' and selection['entity_ids']==['light.fixture'];counts['subscriptions'].append(selection['entity_ids']);send(self.connection,{'id':1,'type':'result','success':True,'result':None})
                        send(self.connection,{'id':1,'type':'event','event':{'a':{'light.fixture':{'s':'on','a':{'friendly_name':'Fixture Åäö','brightness':128,'supported_color_modes':['brightness']}}}}})
                        time.sleep(3);send(self.connection,{'id':1,'type':'event','event':{'c':{'light.fixture':{'+':{'a':{'brightness':255}}}}}});time.sleep(4)
                    except (EOFError,ConnectionError,socket.timeout):pass
                    except Exception as error:counts['errors'].append(type(error).__name__+': '+str(error))
                    self.close_connection=True;return
                counts['rest']+=1
                body={'message':'API running.'} if self.path=='/api/' else {'unit_system':{'temperature':'C'}} if self.path=='/api/config' else {'entity_id':'light.fixture','state':'off','attributes':{'friendly_name':'REST fallback','brightness':0,'supported_color_modes':['brightness']}}
                payload=json.dumps(body).encode();self.send_response(200 if self.headers.get('Authorization')=='Bearer '+token else 401);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
        server=http.server.ThreadingHTTPServer((host,0),Handler);server.daemon_threads=True;ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(cert,key);server.socket=ctx.wrap_socket(server.socket,server_side=True);worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start();(root/'credentials').write_text('https://'+host+':'+str(server.server_port)+'\n'+token+'\n')
        try:yield root,counts
        finally:server.shutdown();server.server_close();worker.join(5)
