from pathlib import Path
import os,subprocess,tempfile,sys,re
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as t:
 exe=Path(t)/'core';command=[os.environ.get('CC','cc'),'-std=c99','-Wno-deprecated-declarations','-I'+str(ROOT/'SDK'),str(ROOT/'tests/test_core.c'),str(ROOT/'src/core.c'),'-o',str(exe)]
 if os.environ.get('SANITIZE')=='1':command+=['-fsanitize=address,undefined']
 subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
for name in ('test_dashboard_extras.py','test_room_dashboard_ui.py','test_dashboard_favourites.py','test_label_picker.py','test_buttons_delivery.py','test_camera.py','test_camera_backend.py','test_detail_backend.py','test_dashboard_spark.py','test_dashboard_profile.py','test_server_profiles.py','test_catalogs.py','test_http_native.py','test_lh5.py','test_guide.py','test_thumbnail_cleanup.py','test_push_state.py','test_push_backend.py','test_media.py','test_dashboard_media.py','test_room_picker.py','test_https_fixture.py','test_subscription_client.py','test_readiness.py','test_resource_progress.py','test_release_tool.py','test_controls.py','test_controls_extended.py','test_capabilities.py','test_push_design.py','test_report.py','test_connection_errors.py','test_confirmation.py','test_dashboard_close.py','test_dashboard_undo.py','test_dashboard_backup_ui.py','test_dashboard_offline.py','test_browser.py','test_dashboard.py','test_dashboard_options.py','test_dashboard_templates.py','test_dashboard_readonly.py','test_dashboard_basic_controls.py','test_dashboard_editor.py','test_dashboard_confirmation.py','test_rexx.py','test_browser_live.py','test_stop.py','test_client.py','test_backend.py','test_low_memory.py','test_credentials.py','test_save_recovery.py','test_recovery_path.py','test_transport.py','test_installer.py','test_preferences.py','test_panel_lifetime.py','test_dependencies.py','test_health.py','test_connectivity.py','test_network.py','test_logs.py'):
 subprocess.run([sys.executable,str(ROOT/'tests'/name)],check=True)
for name in ('AmiHASSBrowser.info','AmiHASSPrefs.info','Install.info','AmiHASS.drawer.info'):
 assert (ROOT/name).read_bytes()[:4]==b'\xe3\x10\0\1',name
assert (ROOT/'Icons/AmiHASS.iff').read_bytes()[8:12]==b'ILBM'
print('AmiHASS portable/core, broker, credentials, transport, installer-flow and icon checks passed.')
