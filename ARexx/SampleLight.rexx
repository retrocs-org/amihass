/* Enable Sample data and start the service in AmiHASSPrefs first. */
options results
address AMIHASS 'QUERY light.amihass_sample_living_room'
if rc ~= 0 then do
  say 'Could not read sample light; RC='rc
  exit rc
end
say result
address AMIHASS 'ACTION light.amihass_sample_living_room Toggle'
if rc ~= 0 then do
  say 'Sample action rejected; RC='rc
  exit rc
end
address AMIHASS 'QUERY light.amihass_sample_living_room'
if rc = 0 then say result
else say 'Readback pending; RC='rc
