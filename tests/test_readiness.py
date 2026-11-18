"""Readiness must reject missing, failed, modified and stale native QA evidence."""
import json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from release_readiness import SUITES,REQUIRED,binding,record_qa,report,junit_result,require_ready
from release_support import ReleaseError,atomic_json
from memory_report import measurements
class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);root=Path(self.temp.name)
        self.tool=SimpleNamespace(root=root,state=root/'release',record=root/'release/build.json',version='0.1',check=lambda:None)
        self.tool.state.mkdir();self.record={'source_sha256':'source','native_record_sha256':'native','artifacts':{'archive':'hash'},'checks':{'portable':'passed','native_build':'passed','sdk':'passed'}}
        atomic_json(self.tool.record,self.record)
        for name in SUITES.values():
            path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('{}')
        self.result=root/'result';self.result.mkdir();(self.result/'junit.xml').write_text('<testsuites><testsuite tests="1" failures="0"><testcase name="native"/></testsuite></testsuites>')
        atomic_json(self.result/'resource-usage.json',{'passed':True})
    def passed(self):
        for suite in REQUIRED:record_qa(self.tool,suite,self.result,binding(self.tool),0)
    def test_missing_and_complete_evidence(self):
        self.assertFalse(report(self.tool)['ready'])
        with self.assertRaises(ReleaseError):require_ready(self.tool)
        self.passed();self.assertTrue(report(self.tool)['ready'])
    def test_changed_source_and_tampered_evidence(self):
        self.passed();self.record['source_sha256']='changed';atomic_json(self.tool.record,self.record)
        value=report(self.tool);self.assertFalse(value['ready']);self.assertTrue(any(c['status']=='stale' for c in value['checks']))
        self.passed();(self.tool.state/'qa/installer.junit.xml').write_text('changed')
        self.assertFalse(report(self.tool)['ready'])
    def test_report_failure_and_zero_test_reports(self):
        for xml in ('<testsuite tests="0"/>','<testsuite failures="1"><testcase name="bad"/></testsuite>','<testsuite><testcase><skipped/></testcase></testsuite>'):
            path=self.result/'junit.xml';path.write_text(xml);self.assertEqual(junit_result(path),'failed')
    def test_changed_build_during_run_rejected(self):
        previous=binding(self.tool);self.record['native_record_sha256']='changed';atomic_json(self.tool.record,self.record)
        with self.assertRaises(ReleaseError):record_qa(self.tool,'installer',self.result,previous,0)
    def test_numeric_memory_reports_and_growth(self):
        sample=measurements('MEMORY sample warm: 1000000 Fast bytes free.\nMEMORY sample ending: 999000; retained change 1000 bytes.','sample-memory');self.assertTrue(sample['passed'])
        self.assertFalse(measurements('MEMORY sample warm: 1000000 Fast bytes free.\nMEMORY sample ending: 800000; retained change 200000 bytes.','sample-memory')['passed'])
        text='\n'.join(f'MEMORY HTTPS cycle {i}: before 16000000, minimum 11000000, after request 14000000, after reclaim 15500000 bytes.' for i in range(1,9))
        result=measurements(text,'https-memory');self.assertTrue(result['passed']);self.assertEqual(result['observed_peak'],5000000);self.assertEqual(result['cycles'][0]['reclaimed'],1500000)
        with self.assertRaises(ReleaseError):measurements('no readings','https-memory')
if __name__=='__main__':unittest.main()
