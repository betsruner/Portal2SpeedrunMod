"Resource/UI/BaseModUI/P2SMLeaderboardEntry.res"
{
	// Same layout as the stock leaderboard's avatar rows (portal_leaderboard_avatar_panel.res).
	"P2SMLeaderboardEntry"
	{
		"ControlName"		"EditablePanel"
		"fieldName"			"P2SMLeaderboardEntry"
		"xpos"				"0"
		"ypos"				"0"
		"wide"				"225"
		"tall"				"36"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"bgcolor_override"	"0 0 0 20"
	}

	"ImgAvatar"
	{
		"ControlName"		"ImagePanel"
		"fieldName"			"ImgAvatar"
		"xpos"				"3"
		"ypos"				"2"
		"zpos"				"1"
		"wide"				"32"
		"tall"				"32"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"scaleImage"		"1"
		"image"				"icon_lobby"
	}

	"LblPlayerName"
	{
		"ControlName"		"Label"
		"fieldName"			"LblPlayerName"
		"xpos"				"38"
		"ypos"				"0"
		"wide"				"180"
		"tall"				"22"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"labelText"			""
		"textAlignment"		"west"
		"Font"				"FriendsList"
		"fgcolor_override"	"0 0 0 255"
		"noshortcutsyntax"	"1"
	}

	"LblTime"
	{
		"ControlName"		"Label"
		"fieldName"			"LblTime"
		"xpos"				"38"
		"ypos"				"22"
		"wide"				"55"
		"tall"				"13"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"labelText"			""
		"textAlignment"		"west"
		"Font"				"FriendsListSmall"
		"fgcolor_override"	"0 0 0 255"
		"noshortcutsyntax"	"1"
	}

	"LblRank"
	{
		"ControlName"		"Label"
		"fieldName"			"LblRank"
		"xpos"				"85"
		"ypos"				"22"
		"wide"				"135"
		"tall"				"14"
		"visible"			"1"
		"enabled"			"1"
		"tabPosition"		"0"
		"labelText"			""
		"textAlignment"		"east"
		"Font"				"FriendsListSmall"
		"fgcolor_override"	"0 0 0 255"
		"noshortcutsyntax"	"1"
	}
}
