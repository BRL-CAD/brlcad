# FIXME - UGLY UGLY hack for Windows - has something to do with
# Itk or something Itk requries not loading quite right.  Should
# be doing this delete for ALL platforms, to set up the redefine
# below.
if {$tcl_platform(platform) != "windows"} {
itcl::delete class itk::Toplevel
}

# Define our own Toplevel to ensure older Itcl/Itk versions can
# still support Archer - we need to keep -menu
itcl::class itk::Toplevel {
    inherit itk::Archetype

    constructor {args} {
        #
        #  Create a toplevel window with the same name as this object
        #
        set itk_hull [namespace tail $this]
        set itk_interior $itk_hull

        itk_component add hull {
            toplevel $itk_hull -class [namespace tail [info class]]
        } {
            keep -menu -background -cursor -takefocus
        }
        bind itk-delete-$itk_hull <Destroy> [list itcl::delete object $this]

        set tags [bindtags $itk_hull]
        bindtags $itk_hull [linsert $tags 0 itk-delete-$itk_hull]

        eval itk_initialize $args
    }

    destructor {
        if {[winfo exists $itk_hull]} {
            set tags [bindtags $itk_hull]
            set i [lsearch $tags itk-delete-$itk_hull]
            if {$i >= 0} {
                bindtags $itk_hull [lreplace $tags $i $i]
            }
            destroy $itk_hull
        }
        itk_component delete hull

        set components [component]
        foreach component $components {
            set path($component) [component $component]
        }
        foreach component $components {
            if {[winfo exists $path($component)]} {
                destroy $path($component)
            }
        }
    }

    itk_option define -title title Title "" {
        wm title $itk_hull $itk_option(-title)
    }

    private variable itk_hull ""
}
