##
# Portions Copyright (c) 2002 SURVICE Engineering Company. All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code as
# defined in and that are subject to the SURVICE Public Source License
# (Version 1.3, dated March 12, 2002).
#
#                        W I Z A R D . T C L

::itcl::class Wizard {
    inherit itk::Widget

    constructor {args} {}
    destructor {}

    public {
	method getWizardTop {}
	method setWizardTop {_wizardTop}
	method getWizardState {}
	method setWizardState {_wizardState}
	method getWizardOrigin {}
	method setWizardOrigin {_wizardOrigin}
	method getWizardAction {}
	method getWizardXmlAction {}
	method getWizardUnits {}

	common wizardMajorType ""
	common wizardMinorType ""
	common wizardName ""
	common wizardVersion ""
	common wizardClass "Wizard"
    }

    protected {
	variable wizardOrigin ""
	variable wizardState ""
	variable wizardTop ""
	variable wizardAction ""
	variable wizardXmlAction ""
	variable wizardUnits ""
    }
}

::itcl::body Wizard::getWizardTop {} {
    return $wizardTop
}

::itcl::body Wizard::setWizardTop {_wizardTop} {
    set wizardTop $_wizardTop
}

::itcl::body Wizard::getWizardState {} {
    return $wizardState
}

::itcl::body Wizard::setWizardState {_wizardState} {
    set wizardState $_wizardState
}

::itcl::body Wizard::getWizardOrigin {} {
    return $wizardOrigin
}

::itcl::body Wizard::setWizardOrigin {_wizardOrigin} {
    set wizardOrigin $_wizardOrigin
}

::itcl::body Wizard::getWizardAction {} {
    return $wizardAction
}

::itcl::body Wizard::getWizardXmlAction {} {
    return $wizardXmlAction
}

::itcl::body Wizard::getWizardUnits {} {
    return $wizardUnits
}
