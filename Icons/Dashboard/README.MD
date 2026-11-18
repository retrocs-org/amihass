# Dashboard device artwork

Generated native 24x24, two-plane artwork in the shared four-pen Workbench style.
Run `python3 tools/make_dashboard_art.py` to regenerate the compiled image data
in src/dashboard_art.c and the matching PNG previews. The PNGs are previews;
the dashboard draws the built-in planar images directly, without loading files.
The palette is background, detail, shine and blue accent. There are domain icons
for light, switch, sensor, binary_sensor, fan, climate, media_player, cover, lock,
scene, script, input_boolean, number, select, weather, camera and a custom fallback,
plus temperature and humidity variants. State and action labels remain textual.
