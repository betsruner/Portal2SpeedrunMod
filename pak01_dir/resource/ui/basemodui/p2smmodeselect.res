"Resource/UI/BaseModUI/P2SMModeSelect.res"
{
	"P2SMModeSelect"
	{
		"ControlName"			"Frame"
		"fieldName"				"P2SMModeSelect"
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
		"labelText"				"SPEEDRUN MOD"
		"textAlignment"			"west"
		"Font"					"FriendsListStatusLine"
		"fgcolor_override"		"0 0 0 255"
	}

	"BtnFullGame"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"BtnFullGame"
		"command"				"menus_open p2sm_full_game"
		"xpos"					"0"
		"ypos"					"25"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"1"
		"navUp"					"BtnChallengeMode"
		"navDown"				"BtnChallengeMode"
		"labelText"				"Full Game"
		"style"					"DefaultButton"
	}

	"BtnChallengeMode"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"BtnChallengeMode"
		"command"				"menus_open p2sm_challenge_mode"
		"xpos"					"0"
		"ypos"					"50"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"navUp"					"BtnFullGame"
		"navDown"				"BtnFullGame"
		"labelText"				"Challenge Mode"
		"style"					"DefaultButton"
	}
}
