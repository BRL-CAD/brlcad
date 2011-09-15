/*
* Copyright 2002-2004 The Apache Software Foundation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
// Write the Netscape 4-specific stylesheet.
if (document.layers) {
  document.writeln('<link rel="stylesheet" type="text/css" href="http://style.tigris.org/nonav/css/ns4_only.css" media="screen" />')
}


// Focus on user name input (the "loginform.loginID" field).
function focus() {
  if (document.loginform) {
    document.loginform.loginID.focus();
  }
}

/* Open popup widows of (mostly) predetermined types.

   windowURL -- The URL to load in the new browser window.
   type -- The (predetermined) type of window to launch.
           acceptable values for type:
           1: a help window
           2: a 400x400 window
           3: Issuezilla assignable users popup window
           ... and you can hard code others yourself inside the function.
   atts -- (optional) If the window you wish to create is unique and you do
           not want to set up a "type" for it, or if you want to pass
           additional attributes for a certain "type", you can pass its
           attributes directly to the function via this parameter.
*/

var tigrisPopupCounter = 0;

function launch(windowURL, type, atts) {
  tigrisPopupCounter += 1;

  var windowName = 'SourceCast' + type;
  if (atts) {
    windowName += tigrisPopupCounter;
  }

  var windowAttributes;
  if (type == 1) {
    windowAttributes = 'resizable=yes,left=10,top=10,screenX=12,screenY=12,height=485,width=724,status=yes,scrollbars=yes,toolbar=yes,menubar=yes,location=yes'
  }
  else if (type == 2) {
    windowAttributes = 'resizable=yes,left=10,top=10,screenX=12,screenY=12,height=400,width=400';
  }
  else if (type == 3) {
   windowAttributes = 'resizable=yes,left=10,top=10,screenX=12,screenY=12,height=440,width=600,scrollbars=yes'; 
  }
  if (atts) {
    windowAttributes += ',' + atts;
  }

  var windowObj = window.open(windowURL, windowName, windowAttributes);

  if (windowObj) {
    return false;
  }
  else {
    return true;
  }
}
