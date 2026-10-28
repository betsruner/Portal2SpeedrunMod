"Resource/UI/BaseModUI/P2SMChallengeMode.res"
{
	"P2SMChallengeMode"
	{
		"ControlName"		"Frame"
		"fieldName"			"P2SMChallengeMode"
		"xpos"				"0"
		"ypos"				"0"
		"wide"				"10"
		"tall"				"5"
		"autoResize"		"0"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"dialogstyle"		"1"
	}

	"ListBtnChapters"
	{
		"ControlName"		"CDialogListButton"
		"fieldName"			"ListBtnChapters"
		"xpos"				"0"
		"ypos"				"10"
		"zpos"				"1"
		"wide"				"250"
		"tall"				"20"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"navDown"			"MapList"

		"list"
		{
			"The Courtesy Call"		"SelectedChapter:1"
			"The Cold Boot"		"SelectedChapter:2"
			"The Return"		"SelectedChapter:3"
			"The Surprise"		"SelectedChapter:4"
			"The Escape"		"SelectedChapter:5"
			"The Fall"		"SelectedChapter:6"
			"The Reunion"		"SelectedChapter:7"
			"The Itch"		"SelectedChapter:8"
			"The Part Where He Kills You"		"SelectedChapter:9"
		}
	}

	// Filled by SMSM with P2SMChallengeMapItem entries.
	"MapList"
	{
		"ControlName"		"GenericPanelList"
		"fieldName"			"MapList"
		"xpos"				"0"
		"ypos"				"40"
		"zpos"				"1"
		"wide"				"250"
		"tall"				"210"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"1"
		"NoWrap"			"1"
		"panelBorder"		"0"
		"bgcolor_override"	"0 0 0 0"
		"navUp"				"ListBtnChapters"
		"navRight"			"LeaderboardList"
	}

	// Category picker over the leaderboard; drawn by SMSM like the stock PORTALS / TIME toggle.
	"CategorySelector"
	{
		"ControlName"		"Panel"
		"fieldName"			"CategorySelector"
		"xpos"				"255"
		"ypos"				"10"
		"zpos"				"1"
		"wide"				"240"
		"tall"				"22"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"bgcolor_override"	"0 0 0 190"
	}

	// Filled by SMSM with P2SMLeaderboardEntry entries.
	"LeaderboardList"
	{
		"ControlName"		"GenericPanelList"
		"fieldName"			"LeaderboardList"
		"xpos"				"255"
		"ypos"				"36"
		"zpos"				"1"
		"wide"				"240"
		"tall"				"214"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"NoWrap"			"1"
		"panelBorder"		"2"
		"bgcolor_override"	"0 0 0 0"
		"navLeft"			"MapList"
	}

	// Loading / error text shown over LeaderboardList.
	"LblLeaderboardStatus"
	{
		"ControlName"		"Label"
		"fieldName"			"LblLeaderboardStatus"
		"xpos"				"255"
		"ypos"				"100"
		"zpos"				"2"
		"wide"				"240"
		"tall"				"40"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"wrap"				"1"
		"labelText"			""
		"textAlignment"		"center"
		"Font"				"DialogMenuItem"
		"fgcolor_override"	"0 0 0 255"
		"noshortcutsyntax"	"1"
	}
}
