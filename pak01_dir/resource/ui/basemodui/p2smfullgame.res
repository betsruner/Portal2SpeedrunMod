"Resource/UI/BaseModUI/P2SMFullGame.res"
{
	"P2SMFullGame"
	{
		"ControlName"			"Frame"
		"fieldName"				"P2SMFullGame"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"10"
		"tall"					"5"
		"autoResize"			"0"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"dialogstyle"			"1"
	}

	// Filled by SMSM with P2SMCategoryTile entries, three per row; two rows show at a time and the
	// rest scroll. 480 for the tiles plus room for the scroll bar.
	"CategoryGrid"
	{
		"ControlName"			"GenericPanelList"
		"fieldName"				"CategoryGrid"
		"xpos"					"0"
		"ypos"					"5"
		"zpos"					"1"
		"wide"					"495"
		"tall"					"244"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"1"
		"NoWrap"				"1"
		"panelBorder"			"0"
		"bgcolor_override"		"0 0 0 0"
	}
}
