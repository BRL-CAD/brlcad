<?php
/*                         D B . P H P
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 **//** @file db.php
 *
 * Really Simple Syndication of BRL-CAD Geometry Databases
 *
 * Description -
 *   This script will publish an rss feed of BRL-CAD geometry
 *   databases.  For convenience, a path alias should be set up for
 *   the * script with a web server.  For Apache, adding something
 *   such as the * following will allow the user to go to a simplified
 *   url without * needing to know that the script is named db.php:
 *
 *   Alias /geometry "/path/to/geometry/db.php"
 *
 * Author -
 *   Sean Morrison
 *
 * Source -
 *   The U.S. Army Research Laboratory
 *   Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

/* --SETTINGS-- */

/* location of the database */
$BASE_URL = ""; /* example: "http://brlcad.org/geometry" */
$PATH_TO_DB = ""; /* "/path/to/geometry" */

/* where to send debug printing */
$ENABLE_DEBUG = 0;
$DEBUG_FILE = "/tmp/db.log";


/* --FUNCTIONS-- */

require('include/debug.inc');
require('include/ctype.inc');


/* --PARSE_OPTIONS-- */

debug("BEGIN http request");

/* initialize defaults */

$extensions = array(
  "rss"  => array(
		  "mime" => array(
				  'text/plain',
				  'text/xml+rss',
				  'text/rss',
  				  'text/xml'
				 )
		 ),
  "xml"	 => array(
		  "mime" => array(
				  'text/plain',
				  'text/xml'
				 )
		 ),
  "html" => array(
		  "mime" => 'text/html'
		 ),
  "txt"  => array(
		  "mime" => 'text/plain'
		 ),
  "g"    => array(
		  "mime" => array(
				  'application/vnd.brlcad.geometry',
				  'application/force-download',
				  'application/octet-stream',
				  'application/download'
				 )
		  ),
  "dir"  => array(
		  "mime" => 'text/plain'
		 )
);
/* extension of response type */
$fileType = "";

/* if the url is an actual file */
$physicalFile = false;

/* place to stash textual data for a proper content-length response */
$textData = "";


/* sanity check */
if (!is_dir($PATH_TO_DB)) {
  debug("ERROR: $PATH_TO_DB is not a directory\n");
  die("ERROR: $PATH_TO_DB is not a directory");
}

/* if path info is given, see if we match a known extension */
$fileWithPath = $PATH_TO_DB . "/";
if (isset($_SERVER['PATH_INFO'])) {
  $fileWithPath .= $_SERVER['PATH_INFO'];

  if (file_exists($fileWithPath)) {
    if (is_dir($fileWithPath)) {
      debug("Requested directory exists");
      $fileType = "dir";
    } else {

      $fileInfo = pathinfo($fileWithPath);
      $fileType = $fileInfo["extension"];
      debug("File exists and has $fileType extension");
      $physicalFile = true;
    }
  }
}

if ($fileType == "") {
  /* no known file found, so try to determine manually */
  $splitString = $_SERVER['REQUEST_URI'];
  if ($_SERVER['PATH_INFO']) {
    $splitString = $_SERVER['PATH_INFO'];
  }
  $splitRequest = preg_split('//', $splitString, -1, PREG_SPLIT_NO_EMPTY);
  $splitLength = count($splitRequest);
  $dotPosition = 0;
  while ($dotPosition < $splitLength) {
    if ($splitRequest[$dotPosition] == ".") {
      break;
    }
    $dotPosition++;
  }

  while ($dotPosition+1 < $splitLength) {
    if (!isalpha($splitRequest[$dotPosition+1])) {
      break;
    }
    $fileType .= $splitRequest[$dotPosition+1];
    $dotPosition++;
  }

  debug("Suffix parsed is $fileType\n");
}

/* final check to see if a known type was found */
if ($fileType == "") {
  debug("WARNING: Unable to determine file type");
} else {
  debug("Suffix found as $fileType");
}


/* --REPLY-- */

header("Pragma: public");
header("Expires: 0");
header("Cache-Control: must-revalidate, post-check=0, pre-check=0");

/* reply with the proper mime type and data */

$mimeType = "";
if (array_key_exists($fileType, $extensions)) {
  $mimeType = $extensions[$fileType]['mime'];
}

if ($mimeType) {
  if (is_array($mimeType)) {
    foreach ($mimeType as $type) {
      debug($type . " content");
      header("Content-type: " . $type);
    }
  } else {
    debug($mimeType . " content");
    header("Content-type: " . $mimeType);
  }
} else {
  /* unknown type */
  debug("WARNING: unknown type of " . $fileType);
}

/* just return the file if it exists  */
if ($physicalFile) {

  /* sanity check */
  if (isset($_SERVER['PATH_INFO'])) {
    $fileWithPath = $PATH_TO_DB . "/" . $_SERVER['PATH_INFO'];

    if (file_exists($fileWithPath)) {
      debug("Returning non rss file $fileWithPath\n");

      header("Content-Disposition: attachment; filename=" . basename($fileWithPath) . ";");
      if ($fileType == "g") {
	header("Content-Transfer-Encoding: binary");
      }
      header("Content-Length: " . filesize($fileWithPath));

      readfile($fileWithPath);

    } else {
      debug("ERROR: $fileWithPath does not exist\n");
      die("ERROR: File $fileWithPath not found.");
    }

  } else {
    debug("ERROR: PATH_INFO was not set\n");
    die("ERROR: PATH_INFO was not set\n");
  }

} else {

  /* return text data -- aggregate into a variable so that a content
   * length can be set.
   */
  if (($fileType == "rss") || ($fileType == "xml")) {

    /* news feed response */
    $textData .= "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    $textData .= "<rss version=\"2.0\" xmlns:content=\"http://purl.org/rss/1.0/modules/content/\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n";
    $textData .= "  <channel>\n";
    $textData .= "    <title>BRL-CAD Geometry Server</title>\n";
    $textData .= "    <link>http://brlcad.org/geometry/</link>\n";
    $textData .= "    <description>Geometry served daily.</description>\n";
    $textData .= "    <language>en-us</language>\n";
    $textData .= "    <copyright>Copyright 2004, brlcad.org</copyright>\n";
    $textData .= "    <webMaster>webmaster@brlcad.org (Christopher Sean Morrison)</webMaster>\n";
    $textData .= "    <pubDate>...</pubDate>\n";
    $textData .= "    <category>BRL-CAD</category>\n";
    $textData .= "    <generator>BRL-CAD db.php Geometry Server</generator>\n";
    $textData .= "    <ttl>60</ttl>\n";

    $dh = opendir($PATH_TO_DB);
    while ($filename = readdir($dh)) {
      $fileEntry = $PATH_TO_DB . "/" . "$filename";
      if (!is_dir($fileEntry)) {
	$fileInfo = pathinfo($fileEntry);
	$fileType = $fileInfo["extension"];

	if ($fileType == "g") {
	  $textData .= "    <item>\n";
	  $textData .= "      <title>$filename</title>\n";
	  $textData .= "      <link>$BASE_URL" . "/$filename</link>\n";
	  $textData .= "      <description>$filename</description>\n";
	  $textData .= "      <author>...</author>\n";
	  $textData .= "      <pubDate>...</pubDate>\n";
	  $textData .= "      <source>$BASE_URL</source>\n";
	  $textData .= "    </item>\n";
	}
      }
    }

    $textData .= "  </channel>\n";
    $textData .= "</rss>\n";

  } else if ($fileType == "dir") {
    debug("WARNING: directories are not handled yet\n");
    $textData .= "WARNING: directories are not handled yet\n";
  } else {
    debug("WARNING: file type [$fileType] without existing file is unsupported\n");
    $textData .= "WARNING: file type [$fileType] without existing file is unsupported\n";
    header("Content-type: text/plain");
  }

  header("Content-Length: " . strlen($textData));
  echo $textData;
}

debug("END http request");


# Local Variables: ***
# mode: C ***
# tab-width: 8 ***
# c-basic-offset: 2 ***
# indent-tabs-mode: t ***
# End: ***
# ex: shiftwidth=2 tabstop=8
?>
