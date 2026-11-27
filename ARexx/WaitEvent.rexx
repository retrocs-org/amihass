/* WaitEvent.rexx - react on the Amiga when Home Assistant changes an entity.
   Usage: rx WaitEvent.rexx binary_sensor.front_door
   In Home Assistant an automation can also set an input_text (for example
   input_text.amiga_message) to send text that this script prints. */
PARSE ARG entity .
IF entity = '' THEN entity = 'light.amihass_sample_desk'  /* toggle it from the Dashboard */
OPTIONS RESULTS
IF ~SHOW('P','AMIHASS') THEN DO; SAY 'Start the AmiHASS service first.'; EXIT 10; END
ADDRESS AMIHASS
SAY 'Waiting for' entity '(Ctrl-C to stop)...'
DO FOREVER
  'WAITEVENT' entity 600
  IF RC > 0 THEN DO; SAY 'WAITEVENT failed, RC' RC; EXIT RC; END
  PARSE VAR RESULT 'result=' kind '0a'x . 'state=' state '0a'x 'previous=' previous '0a'x .
  IF kind = 'changed' THEN SAY TIME() entity 'changed from' previous 'to' state
END
