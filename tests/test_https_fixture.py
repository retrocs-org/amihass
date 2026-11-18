"""Verify the disposable TLS endpoint without any real server credentials."""
import http.client,json,shutil,ssl,sys,unittest,urllib.parse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from https_fixture import endpoint
class FixtureTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('openssl'),'Optional TLS fixture needs openssl')
    def test_verified_tls_and_private_key_isolation(self):
        with endpoint('127.0.0.1') as (credentials,public):
            url,token=credentials.read_text().splitlines();target=urllib.parse.urlsplit(url)
            self.assertEqual([p.name for p in public.iterdir()],['ca.pem'])
            self.assertFalse((public/'key.pem').exists())
            context=ssl.create_default_context(cafile=str(public/'ca.pem'))
            c=http.client.HTTPSConnection(target.hostname,target.port,context=context,timeout=5)
            c.request('GET','/api/',headers={'Authorization':'Bearer '+token});reply=c.getresponse();self.assertEqual(reply.status,200);self.assertEqual(json.loads(reply.read()),{'message':'API running.'});c.close()
            c=http.client.HTTPSConnection(target.hostname,target.port,context=context,timeout=5);c.request('GET','/api/');reply=c.getresponse();self.assertEqual(reply.status,401);reply.read();c.close()
        self.assertFalse(credentials.exists());self.assertFalse(public.exists())
if __name__=='__main__':unittest.main()
