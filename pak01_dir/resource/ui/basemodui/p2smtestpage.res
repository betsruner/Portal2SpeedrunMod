"Resource/UI/BaseModUI/P2SMTestPage.res"
{
	"P2SMTestPage"
	{
		"ControlName"			"Frame"
		"fieldName"				"P2SMTestPage"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"5"
		"tall"					"3"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"dialogstyle"			"1"
	}

	"LblHeading"
	{
		"ControlName"			"Label"
		"fieldName"				"LblHeading"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"labelText"				"THIRD PAGE (test)"
		"textAlignment"			"west"
		"Font"					"FriendsListStatusLine"
		"fgcolor_override"		"0 0 0 255"
	}

	"BtnTestPopup"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"BtnTestPopup"
		"command"				"menus_open p2sm_test_popup"
		"xpos"					"0"
		"ypos"					"25"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"1"
		"navUp"					"BtnBack"
		"navDown"				"BtnBack"
		"labelText"				"Open a popup"
		"style"					"DefaultButton"
	}

	"BtnBack"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"BtnBack"
		"command"				"menus_back"
		"xpos"					"0"
		"ypos"					"50"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"navUp"					"BtnTestPopup"
		"navDown"				"BtnTestPopup"
		"labelText"				"Back"
		"style"					"DefaultButton"
	}
}
