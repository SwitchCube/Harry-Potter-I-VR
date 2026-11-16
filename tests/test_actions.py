import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from project_paths import inside
from validate_actions import load_contract,validate,main


class ActionContractTest(unittest.TestCase):
    def setUp(self):
        self.contract=load_contract()
        self.manifest=json.loads(inside('config/vr/actions.json').read_text())
        self.bindings={row['binding_url']:json.loads(inside('config/vr/'+row['binding_url']).read_text()) for row in self.manifest['default_bindings']}

    def test_shipped_files_and_native_references(self):
        main()

    def test_original_crash_name_rejected_before_launch(self):
        self.contract['right_aim']=self.contract.pop('right_hand')
        with self.assertRaisesRegex(ValueError,'missing.*right_aim'):
            validate(self.contract,self.manifest,self.bindings)

    def test_pose_type_mismatch_rejected(self):
        next(a for a in self.manifest['actions'] if a['name'].endswith('/right_hand'))['type']='boolean'
        with self.assertRaisesRegex(ValueError,'Wrong.*right_hand'):
            validate(self.contract,self.manifest,self.bindings)

    def test_missing_controller_pointer_rejected(self):
        self.bindings['bindings_touch.json']['bindings']['/actions/game']['poses']=[p for p in self.bindings['bindings_touch.json']['bindings']['/actions/game']['poses'] if not p['output'].endswith('/right_hand')]
        with self.assertRaisesRegex(ValueError,'missing=.*right_hand'):
            validate(self.contract,self.manifest,self.bindings)

    def test_unknown_binding_output_rejected(self):
        self.bindings['bindings_touch.json']['bindings']['/actions/game']['poses'][0]['output']='/actions/game/in/missing_pose'
        with self.assertRaisesRegex(ValueError,'unknown=.*missing_pose'):
            validate(self.contract,self.manifest,self.bindings)


if __name__=='__main__':
    unittest.main()
