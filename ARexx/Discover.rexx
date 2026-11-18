/* Discover bridge devices and the first page of lights. */
options results
address AMIHASS 'DEVICES "" 0'
if rc = 0 then say result
else say 'Device catalogue is updating or unavailable; RC='rc
address AMIHASS 'LIST light "" 0'
if rc = 0 then say result
else say 'Entity catalogue is updating or unavailable; RC='rc
