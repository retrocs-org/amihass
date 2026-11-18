/* AmiHASS: report the shared bridge's actual source and readiness. */
options results
address AMIHASS 'STATUS'
if rc = 0 then say result
else say 'AmiHASS status failed; RC='rc
