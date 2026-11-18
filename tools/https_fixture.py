"""Disposable authenticated TLS endpoint for read-only native memory measurements."""
import contextlib,http.server,ipaddress,json,secrets,shutil,ssl,subprocess,tempfile,threading
from pathlib import Path
from release_support import ReleaseError
@contextlib.contextmanager
def endpoint(host=None):
    if host is None:
        address=subprocess.run(['/usr/sbin/ipconfig','getifaddr','en0'],capture_output=True,text=True)
        if address.returncode:raise ReleaseError('TLS fixture needs the en0 bridge interface used by Workbench')
        host=address.stdout.strip()
    host=str(ipaddress.IPv4Address(host))
    openssl=shutil.which('openssl')
    if not openssl:raise ReleaseError('TLS fixture needs openssl to generate its disposable certificate')
    with tempfile.TemporaryDirectory(prefix='amihass-tls-fixture-') as temp:
        root=Path(temp);public=root/'public';public.mkdir();cert=public/'ca.pem';key=root/'key.pem'
        command=[openssl,'req','-x509','-newkey','rsa:2048','-sha256','-nodes','-days','1','-subj','/CN=AmiHASS QA fixture','-addext','subjectAltName=IP:'+host,'-keyout',str(key),'-out',str(cert)]
        result=subprocess.run(command,capture_output=True)
        if result.returncode:raise ReleaseError('Could not generate the disposable TLS fixture certificate')
        key.chmod(0o600);token='amihass_fixture_'+secrets.token_hex(24)
        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_GET(self):
                accepted=self.path=='/api/' and self.headers.get('Authorization')=='Bearer '+token
                body=json.dumps({'message':'API running.' if accepted else 'Unauthorized'}).encode()
                self.send_response(200 if accepted else 401);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        server=http.server.HTTPServer((host,0),Handler);server.timeout=2
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key);server.socket=context.wrap_socket(server.socket,server_side=True)
        worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
        credentials=root/'credentials';credentials.write_text('https://'+host+':'+str(server.server_port)+'\n'+token+'\n');credentials.chmod(0o600)
        try:yield credentials,public
        finally:server.shutdown();server.server_close();worker.join(timeout=5)

@contextlib.contextmanager
def transport_endpoint():
    """Public reserved-domain TLS handshake probe; HTTP 404 is expected, not HA success."""
    with tempfile.TemporaryDirectory(prefix='amihass-transport-fixture-') as directory:
        path=Path(directory)/'credentials'
        path.write_text('https://example.com\namihass_fixture_'+secrets.token_hex(24)+'\n');path.chmod(0o600)
        yield path,None
