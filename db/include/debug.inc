<?php
/* log function */
if ($ENABLE_DEBUG) {
  function debug ($message) {
    global $DEBUG_FILE;
    $fp = fopen($DEBUG_FILE, "a");
    if ($fp) {
    /* output the message with a BSD-style timestamp */
      fwrite($fp, date("D M j G:i:s T Y") . " " . $_SERVER['REMOTE_ADDR'] . " " . $message . "\n");
      fclose($fp);
    } else {
      print("Unable to write to to log file [$filename]");
    }
  }
} else {
  function debug ($filename, $message) {
  }
}

# Local Variables: ***
# mode: C ***
# tab-width: 8 ***
# c-basic-offset: 2 ***
# indent-tabs-mode: t ***
# End: ***
# ex: shiftwidth=2 tabstop=8
?>
