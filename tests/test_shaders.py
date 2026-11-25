import importlib.util, pathlib, struct, tempfile, unittest
spec=importlib.util.spec_from_file_location('shaders',pathlib.Path(__file__).parents[1]/'tools/shaders.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
def make(): return b'DXBC'+bytes(16)+struct.pack('<IIII',1,48,1,36)+b'DXIL'+struct.pack('<I',4)+b'test'
class Test(unittest.TestCase):
 def test_container(self):
  x=make();self.assertEqual(s.container(x,0),(48,['DXIL']));self.assertIsNone(s.container(x[:-1],0));self.assertIsNone(s.container(b'DXBC'+bytes(50),0))
 def test_scan(self):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t);(p/'a').write_bytes(b'abc'+make());(p/'b').write_bytes(make());r=s.scan(p);self.assertEqual(r['unique_containers'],1);self.assertEqual(len(r['containers'][0]['locations']),2)
if __name__=='__main__': unittest.main()
