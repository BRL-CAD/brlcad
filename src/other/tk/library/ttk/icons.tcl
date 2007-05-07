#
# $Id$
#
# Ttk package -- stock icons.
# 
# Usage:
#	$w configure -image [ttk::stockIcon $context/$icon]
#
# At present, only includes icons for dialog boxes,
# dialog/info, dialog/warning, dialog/error, etc.
#
# This list should be expanded.
#
# See the Icon Naming Specification from the Tango project:
# http://standards.freedesktop.org/icon-naming-spec/
# They've finally gotten around to publishing something.
#

namespace eval ttk {
    variable Icons		;# Map: icon name -> image
    namespace eval icons {}	;# container namespace for images
}

# stockIcon $name --
#	Returns a Tk image for built-in icon $name. 
#
proc ttk::stockIcon {name} {
    variable Icons
    return $Icons($name)
}

# defineImage --
#	Define a new stock icon.
# 
proc ttk::defineImage {name args} {
    variable Icons
    set iconName ::ttk::icons::$name
    eval [linsert $args 0 image create photo $iconName]
    set Icons($name) $iconName
}

#
# Stock icons for dialogs 
#
# SOURCE: dialog icons taken from BWidget toolkit.
#
ttk::defineImage dialog/error -data {
    R0lGODlhIAAgALMAAIQAAISEhPf/Mf8AAP//////////////////////////
    /////////////////////yH5BAEAAAIALAAAAAAgACAAAASwUMhJBbj41s0n
    HmAIYl0JiCgKlNWVvqHGnnA9mnY+rBytw4DAxhci2IwqoSdFaMKaSBFPQhxA
    nahrdKS0MK8ibSoorBbBVvS4XNOKgey2e7sOmLPvGvkezsPtR3M2e3JzdFIB
    gC9vfohxfVCQWI6PII1pkZReeIeWkzGJS1lHdV2bPy9koaKopUOtSatDfECq
    phWKOra3G3YuqReJwiwUiRkZwsPEuMnNycslzrIdEQAAOw==
}

ttk::defineImage dialog/info -data {
    R0lGODlhIAAgALMAAAAAAAAA/4SEhMbGxvf/Mf//////////////////////
    /////////////////////yH5BAEAAAQALAAAAAAgACAAAAStkMhJibj41s0n
    HkUoDljXXaCoqqRgUkK6zqP7CvQQ7IGsAiYcjcejFYAb4ZAYMB4rMaeO51sN
    kBKlc/uzRbng0NWlnTF3XAAZzExj2ET3BV7cqufctv2Tj0vvFn11RndkVSt6
    OYVZRmeDXRoTAGFOhTaSlDOWHACHW2MlHQCdYFebN6OkVqkZlzcXqTKWoS8w
    GJMhs7WoIoC7v7i+v7uTwsO1o5HHu7TLtcodEQAAOw==
}

ttk::defineImage dialog/question -data {
    R0lGODlhIAAgALMAAAAAAAAA/4SEhMbGxvf/Mf//////////////////////
    /////////////////////yH5BAEAAAQALAAAAAAgACAAAAS2kMhJibj41s0n
    HkUoDljXXaCoqqRgUkK6zqP7CnS+AiY+D4GgUKbibXwrYEoYIIqMHmcoqGLS
    BlBLzlrgzgC22FZYAJKvYG3ODPLS0khd+awDX+Qieh2Dnzb7dnE6VIAffYdl
    dmo6bHiBFlJVej+PizRuXyUTAIxBkSGBNpuImZoVAJ9roSYAqH1Yqzetrkmz
    GaI3F7MyoaYvHhicoLe/sk8axcnCisnKBczNxa3I0cW+1bm/EQAAOw==
}

ttk::defineImage dialog/warning -data {
    R0lGODlhIAAgALMAAAAAAISEAISEhMbGxv//AP//////////////////////
    /////////////////////yH5BAEAAAUALAAAAAAgACAAAASrsMhJZ7g16y0D
    IQPAjZr3gYBAroV5piq7uWcoxHJFv3eun0BUz9cJAmHElhFow8lcIQBgwHOu
    aNJsDfk8ZgHH4TX4BW/Fo12ZjJ4Z10wuZ0cIZOny0jI6NTbnSwRaS3kUdCd2
    h0JWRYEhVIGFSoEfZo6FipRvaJkfUZB7cp2Cg5FDo6RSmn+on5qCPaivYTey
    s4sqtqswp2W+v743whTCxcbHyG0FyczJEhEAADs=
}

ttk::defineImage dialog/auth -data {
    R0lGODlhIAAgAIQAAAAA/wAAAICAgICAAP///7CwsMDAwMjIAPjIAOjo6Pj4
    AODg4HBwcMj4ANjY2JiYANDQ0MjIyPj4yKCgoMiYAMjImDAwAMjIMJiYmJCQ
    kAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAgACAAAAX+ICCOYmCa
    ZKquZCCMQsDOqWC7NiAMvEyvAoLQVdgZCAfEAPWDERIJk8AwIJwUil5T91y4
    GC6ry4RoKH2zYGLhnS5tMUNAcaAvaUF2m1A9GeQIAQeDaEAECw6IJlVYAmAK
    AWZJD3gEDpeXOwRYnHOCCgcPhTWWDhAQQYydkGYIoaOkp6h8m1ieSYOvP0ER
    EQwEEap0dWagok1BswmMdbiursfIBHnBQs10oKF30tQ8QkISuAcB25UGQQ4R
    EzzsA4MU4+WGBkXo6hMTMQADFQfwFtHmFSlCAEKEU2jc+YsHy8nAML4iJKzQ
    Dx65hiWKTIA4pRC7CxblORRA8E/HFfxfQo4KUiBfPgL0SDbkV0ElKZcmEjwE
    wqPCgwMiAQTASQDDzhkD4IkMkg+DiwU4aSTVQiIIBgFXE+ATsPHHCRVWM8QI
    oJUrxi04TCzA0PQsWh9kMVx1u6UFA3116zLJGwIAOw==
}

ttk::defineImage dialog/busy -data {
    R0lGODlhIAAgALMAAAAAAIAAAACAAICAAAAAgIAAgACAgMDAwICAgP8AAAD/
    AP//AAAA//8A/wD//////yH5BAEAAAsALAAAAAAgACAAAASAcMlJq7046827
    /2AYBmRpkoC4BMlzvEkspypg3zitIsfjvgcEQifi+X7BoUpi9AGFxFATCV0u
    eMEDQFu1GrdbpZXZC0e9LvF4gkifl8aX2tt7bIPvz/Q5l9btcn0gTWBJeR1G
    bWBdO0EPPIuHHDmUSyxIMjM1lJVrnp+goaIfEQAAOw==
}

#*EOF*
