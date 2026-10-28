"Resource/UI/BaseModUI/P2SMSettings.res"
{
	"P2SMSettings"
	{
		"ControlName"			"Frame"
		"fieldName"				"P2SMSettings"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"7"
		"tall"					"2"
		"autoResize"			"0"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"dialogstyle"			"1"
	}

	// The list commands are handled by SMSM's settings panel, which also sets the shown values.
	"DrpDialogue"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"DrpDialogue"
		"xpos"					"0"
		"ypos"					"0"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"1"
		"navUp"					"DrpFastStart"
		"navDown"				"DrpFastStart"
		"labelText"				"In-game Dialogue"
		"style"					"DialogListButton"
		"list"
		{
			"#L4D360UI_Disabled"	"DialogueDisabled"
			"#L4D360UI_Enabled"		"DialogueEnabled"
		}
	}

	"DrpFastStart"
	{
		"ControlName"			"BaseModHybridButton"
		"fieldName"				"DrpFastStart"
		"xpos"					"0"
		"ypos"					"25"
		"wide"					"0"
		"tall"					"20"
		"visible"				"1"
		"enabled"				"1"
		"tabPosition"			"0"
		"navUp"					"DrpDialogue"
		"navDown"				"DrpDialogue"
		"labelText"				"Fast-Start"
		"style"					"DialogListButton"
		"list"
		{
			"#L4D360UI_Disabled"	"FastStartDisabled"
			"#L4D360UI_Enabled"		"FastStartEnabled"
		}
	}
}
