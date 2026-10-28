"Resource/UI/BaseModUI/P2SMTestPopup.res"
{
	"P2SMTestPopup"
	{
		"ControlName"			"Frame"
		"fieldName"				"P2SMTestPopup"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"5"
		"tall"					"2"
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
		"labelText"				"POPUP (test)"
		"textAlignment"			"west"
		"Font"					"FriendsListStatusLine"
		"fgcolor_override"		"0 0 0 255"
	}

	"BtnClose"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"BtnClose"
		"command"				"menus_back"
		"xpos"					"0"
		"ypos"					"25"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"1"
		"navUp"					"BtnClose"
		"navDown"				"BtnClose"
		"labelText"				"Close"
		"style"					"DefaultButton"
	}
}
