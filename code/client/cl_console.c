/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// console.c

#include "client.h"

#define  DEFAULT_CONSOLE_WIDTH 78
#define  MAX_CONSOLE_WIDTH 120

int bigchar_width;
int bigchar_height;
int smallchar_width;
int smallchar_height;

#define CONSOLE_ALL 0
#define CONSOLE_GENERAL 1
#define CONSOLE_KILLS 2
#define CONSOLE_HITS 3
#define CONSOLE_CHAT 4
#define CONSOLE_DEV 5

extern  qboolean    chat_team;
extern  int         chat_playerNum;

console_t consoles[6];

int currentConsoleNum = CONSOLE_ALL;
console_t *currentCon = &consoles[CONSOLE_ALL];

char *consoleNames[] = {
        "All",
        "General",
        "Kills",
        "Hits",
        "Chat",
        "Dev"
};
int numConsoles = 5;

cvar_t		*con_conspeed;
cvar_t		*con_notifytime;
cvar_t 		*con_timestamp;

vec4_t	    console_color = {1.0, 1.0, 1.0, 1.0};

int         g_console_field_width = DEFAULT_CONSOLE_WIDTH;

void		Con_Fixup( console_t* console );

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void ) {
	// Can't toggle the console when it's the only thing available
    if ( cls.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE ) {
		return;
	}

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_CONSOLE );
}


/*
================
Con_MessageMode_f
================
*/
static void Con_MessageMode_f( void ) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode2_f
================
*/
static void Con_MessageMode2_f( void ) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode3_f
================
*/
static void Con_MessageMode3_f( void ) {
	chat_playerNum = cgvm ? VM_Call( cgvm, 0, CG_CROSSHAIR_PLAYER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_MessageMode4_f
================
*/
static void Con_MessageMode4_f( void ) {
	chat_playerNum = cgvm ? VM_Call( cgvm, 0, CG_LAST_ATTACKER ) : -1;
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher() ^ KEYCATCH_MESSAGE );
}


/*
================
Con_Clear_f
================
*/
static void Con_Clear_f( void ) {
	int		i;

	for (i = 0; i < CON_TEXTSIZE; i++) {
		currentCon->text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
	}

	//con.x = 0;
	//con.current = 0;
	//con.newline = qtrue;

	Con_Bottom();		// go to end
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f( void )
{
	int		l, x, i, n;
	short	*line;
	fileHandle_t	f;
	int		bufferlen;
	char	*buffer;
	char	filename[ MAX_OSPATH ];
	const char *ext;

	if ( Cmd_Argc() != 2 )
	{
		Com_Printf( "usage: condump <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv( 1 ), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".txt" );

	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Printf( "%s: Invalid filename extension '%s'.\n", __func__, ext );
		return;
	}

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE )
	{
		Com_Printf( "ERROR: couldn't open %s.\n", filename );
		return;
	}

	Com_Printf( "Dumped console text to %s.\n", filename );

	if ( consoles[CONSOLE_ALL].current >= consoles[CONSOLE_ALL].totallines ) {
		n = consoles[CONSOLE_ALL].totallines;
		l = consoles[CONSOLE_ALL].current + 1;
	} else {
		n = consoles[CONSOLE_ALL].current + 1;
		l = 0;
	}

	bufferlen = consoles[CONSOLE_ALL].linewidth + ARRAY_LEN( Q_NEWLINE ) * sizeof( char );
	buffer = Hunk_AllocateTempMemory( bufferlen );

	// write the remaining lines
	buffer[ bufferlen - 1 ] = '\0';

	for ( i = 0; i < n ; i++, l++ ) 
	{
		line = consoles[CONSOLE_ALL].text + (l % consoles[CONSOLE_ALL].totallines) * consoles[CONSOLE_ALL].linewidth;
		// store line
		for( x = 0; x < consoles[CONSOLE_ALL].linewidth; x++ )
			buffer[ x ] = line[ x ] & 0xff;
		// terminate on ending space characters
		for ( x = consoles[CONSOLE_ALL].linewidth - 1 ; x >= 0 ; x-- ) {
			if ( buffer[ x ] == ' ' )
				buffer[ x ] = '\0';
			else
				break;
		}
		Q_strcat( buffer, bufferlen, Q_NEWLINE );
		FS_Write( buffer, strlen( buffer ), f );
	}

	Hunk_FreeTempMemory( buffer );
	FS_FCloseFile( f );
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	int		i;
	
	for ( i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		currentCon->times[i] = 0;
	}
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize(console_t* console)
{
	int		i, j, width, oldwidth, oldtotallines, oldcurrent, numlines, numchars;
	short	tbuf[CON_TEXTSIZE], *src, *dst;
	static int old_width, old_vispage;
	int		vispage;

	if ( console->viswidth == cls.glconfig.vidWidth )
		return;

	console->viswidth = cls.glconfig.vidWidth;

	if ( smallchar_width == 0 ) // might happen on early init
	{
		smallchar_width = SMALLCHAR_WIDTH;
		smallchar_height = SMALLCHAR_HEIGHT;
		bigchar_width = BIGCHAR_WIDTH;
		bigchar_height = BIGCHAR_HEIGHT;
	}

	if ( cls.glconfig.vidWidth == 0 ) // video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		console->linewidth = width;
		console->totallines = CON_TEXTSIZE / console->linewidth;
		console->vispage = 4;
		for (i = 0; i < CON_TEXTSIZE; i++)
			console->text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
	}
	else
	{
		width = (cls.glconfig.vidWidth / smallchar_width) - 2;

		if ( width > MAX_CONSOLE_WIDTH )
			width = MAX_CONSOLE_WIDTH;

		vispage = cls.glconfig.vidHeight / smallchar_height - 1;

		if ( old_vispage == vispage && old_width == width )
			return;

		oldwidth = console->linewidth;
		oldtotallines = console->totallines;
		oldcurrent = console->current;
		console->linewidth = width;
		console->totallines = CON_TEXTSIZE / console->linewidth;
		console->vispage = vispage;

		old_vispage = vispage;
		old_width = width;

		numchars = oldwidth;
		if ( numchars > console->linewidth )
			numchars = console->linewidth;

		if ( oldcurrent > oldtotallines )
			numlines = oldtotallines;	
		else
			numlines = oldcurrent + 1;	

		if ( numlines > console->totallines )
			numlines = console->totallines;

		Com_Memcpy( tbuf, console->text, CON_TEXTSIZE * sizeof( short ) );

		for ( i = 0; i < CON_TEXTSIZE; i++ ) 
			console->text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';

		for ( i = 0; i < numlines; i++ )
		{
			src = &tbuf[ ((oldcurrent - i + oldtotallines) % oldtotallines) * oldwidth ];
			dst = &console->text[ (numlines - 1 - i) * console->linewidth ];
			for ( j = 0; j < numchars; j++ )
				*dst++ = *src++;
		}

		Con_ClearNotify();

		console->current = numlines - 1;
	}

	console->display = console->current;
}

void Con_PrevTab() {
	currentConsoleNum--;
	if (currentConsoleNum < 0)
		currentConsoleNum = numConsoles - 1;
	currentCon = &consoles[currentConsoleNum];
}

void Con_NextTab() {
	currentConsoleNum++;
	if (currentConsoleNum == numConsoles)
		currentConsoleNum = CONSOLE_ALL;
	currentCon = &consoles[currentConsoleNum];
}


/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/*
================
Con_Init
================
*/
void Con_Init( void ) 
{
	con_notifytime = Cvar_Get( "con_notifytime", "3", 0 );
    Cvar_SetDescription( con_notifytime, "Defines how long messages (from players or the system) are on the screen\nDefault: 3 seconds" );

    con_conspeed = Cvar_Get( "scr_conspeed", "3", 0 );
    Cvar_SetDescription( con_conspeed, "Set how fast the console goes up and down\nDefault: 3 seconds" );

    con_timestamp = Cvar_Get( "con_timestamp", "0", CVAR_ARCHIVE_ND );

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Cmd_AddCommand( "clear", Con_Clear_f );
    Cmd_SetDescription("clear", "Clear all text from console\nUsage: clear");

    Cmd_AddCommand( "condump", Con_Dump_f );
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
    Cmd_SetDescription("condump", "Write the console text to a file\nUsage: condump <file>");

    Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
    Cmd_SetDescription("toggleconsole", "Usually bound to ~ the tilde key brings the console up and down\nUsage: bind <key> toggleconsole");

    Cmd_AddCommand( "messagemode", Con_MessageMode_f );
    Cmd_SetDescription("messagemode", "Send a message to everyone");

    Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
    Cmd_SetDescription("messagemode2", "Send a message to teammates");

    Cmd_AddCommand( "messagemode3", Con_MessageMode3_f );
    Cmd_SetDescription("messagemode3", "Send a message to targeted player");

    Cmd_AddCommand( "messagemode4", Con_MessageMode4_f );
    Cmd_SetDescription("messagemode4", "Send a message to last attacker");
}


/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	Cmd_RemoveCommand( "clear" );
	Cmd_RemoveCommand( "condump" );
	Cmd_RemoveCommand( "toggleconsole" );
	Cmd_RemoveCommand( "messagemode" );
	Cmd_RemoveCommand( "messagemode2" );
	Cmd_RemoveCommand( "messagemode3" );
	Cmd_RemoveCommand( "messagemode4" );
}


/*
===============
Con_Fixup
===============
*/
void Con_Fixup( console_t* console )
{
	int filled;

	if ( console->current >= console->totallines ) {
		filled = console->totallines;
	} else {
		filled = console->current + 1;
	}

	if ( filled <= console->vispage ) {
		console->display = console->current;
	} else if (console->current - console->display > filled - console->vispage ) {
		console->display = console->current - filled + console->vispage;
	} else if (console->display > console->current ) {
		console->display = console->current;
	}
}


/*
===============
Con_Linefeed

Move to newline only when we _really_ need this
===============
*/
void Con_NewLine( console_t* console, qboolean skipnotify )
{
	int		i;

	// mark time for transparent overlay
	if (console->current >= 0)
	{
		if (skipnotify)
			console->times[console->current % NUM_CON_TIMES] = 0;
		else
			console->times[console->current % NUM_CON_TIMES] = cls.realtime;
	}

	if (console->display == console->current)
		console->display++;
	console->current++;
	for (i = 0; i < console->linewidth; i++)
		console->text[(console->current % console->totallines) * console->linewidth + i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';

	console->x = 0;

//    short *s;
//    int i;
//
//    // follow last line
//    if ( console->display == console->current )
//        console->display++;
//    console->current++;
//
//    s = &console->text[ ( console->current % console->totallines ) * console->linewidth ];
//    for ( i = 0; i < console->linewidth ; i++ )
//        *s++ = (ColorIndex(COLOR_WHITE)<<8) | ' ';
//
//    console->x = 0;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( console_t* console, qboolean skipnotify )
{
	// mark time for transparent overlay
	if (console->current >= 0 )	{
		if ( skipnotify )
			console->times[ console->current % NUM_CON_TIMES ] = 0;
		else
			console->times[ console->current % NUM_CON_TIMES ] = cls.realtime;
	}

	if (console->newline ) {
		Con_NewLine(console, skipnotify);
	} else {
		console->newline = qtrue;
		console->x = 0;
	}

	Con_Fixup(console);
}

void writeTextToConsole(console_t* console, char* txt, qboolean skipnotify)
{
	int		y;
	int		c, l;
	int		colorIndex;
	int     prev;

	colorIndex = ColorIndex(COLOR_WHITE);

	while ((c = *txt) != 0) {
		if (Q_IsColorString(txt) && *(txt + 1) != '\n') {
			colorIndex = ColorIndexFromChar(*(txt + 1));
			txt += 2;
			continue;
		}

		// count word length
		for (l = 0; l < console->linewidth; l++) {
			if (txt[l] <= ' ') {
				break;
			}
		}

		// word wrap
		if (l != console->linewidth && (console->x + l >= console->linewidth)) {
			Con_Linefeed(console, skipnotify);
		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed(console, skipnotify);
			break;
		case '\r':
			console->x = 0;
			break;
		default:
			if (console->newline) {
				Con_NewLine(console, skipnotify);
				Con_Fixup(consoles);
				console->newline = qfalse;
			}
			// display character and advance
			y = console->current % console->totallines;
			console->text[y * console->linewidth + console->x] = (colorIndex << 8) | c;
			console->x++;
			if (console->x >= console->linewidth) {
				Con_Linefeed(console, skipnotify);
			}
			break;
		}
	}

	// mark time for transparent overlay
	if (console->current >= 0) {
		if (skipnotify) {
			prev = console->current % NUM_CON_TIMES - 1;
			if (prev < 0)
				prev = NUM_CON_TIMES - 1;
			console->times[prev] = 0;
		}
		else {
			console->times[console->current % NUM_CON_TIMES] = cls.realtime;
		}
	}
}


/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( char *txt ) {
	int     i;

	qboolean isKill = qfalse;
	qboolean isHit = qfalse;
	qboolean isChat = qfalse;

	qboolean skipnotify = qfalse;		// NERVE - SMF

    if (currentCon->x==0 && con_timestamp && con_timestamp->integer) {
        char txtt[MAXPRINTMSG];
        qtime_t	now;
		char* message;
		message = txt;

        Com_RealTime( &now );
		if (txt[0] == 17 || txt[0] == 18 || txt[0] == 19) {
			message++;
			Com_sprintf(txtt, sizeof(txtt), "%c^9%02d:%02d:%02d ^7%s", txt[0], now.tm_hour, now.tm_min, now.tm_sec, message);
		}
		else {
			Com_sprintf(txtt, sizeof(txtt), "^9%02d:%02d:%02d ^7%s", now.tm_hour, now.tm_min, now.tm_sec, txt);
		}
        
        strcpy(txt,txtt);
    }

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}

	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}

	for (i = 0; i < numConsoles + 1; i++) {
		if (!consoles[i].initialized) {
			consoles[i].color[0] =
				consoles[i].color[1] =
				consoles[i].color[2] =
				consoles[i].color[3] = 1.0f;
			consoles[i].viswidth = -9999;
			Con_CheckResize(&consoles[i]);
			consoles[i].initialized = qtrue;
		}
	}

	if (txt[0] == 17) {
		isKill = qtrue;
		txt++;
	} else if (txt[0] == 18) {
		isHit = qtrue;
		txt++;
	} else if (txt[0] == 19) {
		isChat = qtrue;
		txt++;
	}

	writeTextToConsole(&consoles[CONSOLE_ALL], txt, skipnotify);

	if (isKill) {
		writeTextToConsole(&consoles[CONSOLE_KILLS], txt, skipnotify);
		writeTextToConsole(&consoles[CONSOLE_HITS], txt, skipnotify);
	} else if (isHit) {
		writeTextToConsole(&consoles[CONSOLE_HITS], txt, skipnotify);
	} else if (isChat) {
		writeTextToConsole(&consoles[CONSOLE_CHAT], txt, skipnotify);
	} else {
		writeTextToConsole(&consoles[CONSOLE_GENERAL], txt, skipnotify);
	}
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput( void ) {
	int		y;

	if ( cls.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	y = currentCon->vislines - ( smallchar_height * 2 );

	re.SetColor(currentCon->color );

	SCR_DrawSmallChar(currentCon->xadjust + 1 * smallchar_width, y, ']' );

	Field_Draw( &g_consoleField, currentCon->xadjust + 2 * smallchar_width, y,
		SCREEN_WIDTH - 3 * smallchar_width, qtrue, qtrue );
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
	int		x, v;
	short	*text;
	int		i;
	int		time;
	int		skip;
	int		currentColorIndex;
	int		colorIndex;

	currentColorIndex = ColorIndex( COLOR_WHITE );
	re.SetColor( g_color_table[ currentColorIndex ] );

	v = 0;
	for (i= currentCon->current-NUM_CON_TIMES+1 ; i<= currentCon->current ; i++)
	{
		if (i < 0)
			continue;
		time = currentCon->times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if ( time >= con_notifytime->value*1000 )
			continue;
		text = currentCon->text + (i % currentCon->totallines)* currentCon->linewidth;

		if (cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			continue;
		}

		for (x = 0 ; x < currentCon->linewidth ; x++) {
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}
			colorIndex = ( text[x] >> 8 ) & 63;
			if ( currentColorIndex != colorIndex ) {
				currentColorIndex = colorIndex;
				re.SetColor( g_color_table[ colorIndex ] );
			}
			SCR_DrawSmallChar( cl_conXOffset->integer + currentCon->xadjust + (x+1)*smallchar_width, v, text[x] & 0xff );
		}

		v += smallchar_height;
	}

	re.SetColor( NULL );

	if ( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
		return;
	}

	// draw the chat line
	if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
	{
		// rescale to virtual 640x480 space
		v /= cls.glconfig.vidHeight / 480.0;

		if (chat_team)
		{
			SCR_DrawBigString( SMALLCHAR_WIDTH, v, "say_team:", 1.0f, qfalse );
			skip = 10;
		}
		else
		{
			SCR_DrawBigString( SMALLCHAR_WIDTH, v, "say:", 1.0f, qfalse );
			skip = 5;
		}

		Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, v,
			SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qtrue );
	}
}


/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac ) {

	int				i, x, y;
	int				rows;
	short* text;
	int				row;
	int				lines;
	int				currentColor;
	vec4_t			color = { 1, 0, 0, 1 };
	vec4_t			color2 = { 1, 1, 1, 1 };
	vec4_t          bgColor = { 0.0f, 0.0f, 0.0f, 0.85f };
	int totalOffset = 0;

	lines = cls.glconfig.vidHeight * frac;
	if (lines <= 0)
		return;

	if (lines > cls.glconfig.vidHeight)
		lines = cls.glconfig.vidHeight;

	// on wide screens, we will center the text
	currentCon->xadjust = 0;
	SCR_AdjustFrom640(&currentCon->xadjust, NULL, NULL, NULL);

	// draw the background
	y = frac * SCREEN_HEIGHT - 2;
	if (y < 1) {
		y = 0;
	}
	else {
		SCR_FillRect(0, 0, SCREEN_WIDTH, y, bgColor);
	}

	int vertOffset;
	int horizOffset;
	horizOffset = 0;
	vertOffset = y;
	int tabWidth;
	int tabHeight;
	float old;

	for (i = 0; i < numConsoles; i++) {
		if (currentCon == &consoles[i]) {
			tabWidth = SCR_FontWidth(consoleNames[i], 0.24f) + 30;
			tabHeight = 22;
			color[3] = 1.0f;
		}
		else {
			tabWidth = SCR_FontWidth(consoleNames[i], 0.18f) + 18;
			tabHeight = 18;
			color[3] = 0.2f;
		}

		// tab background
		if (i)
			SCR_FillRect(horizOffset + 1, vertOffset, tabWidth - 1, tabHeight, bgColor);
		else
			SCR_FillRect(horizOffset, vertOffset, tabWidth, tabHeight, bgColor);

		if (currentCon != &consoles[i]) {
			old = color[3];
			color[3] = 1;

			// top border
			SCR_FillRect(horizOffset, vertOffset, tabWidth, 1, color);
			color[3] = old;
		}

		// bottom border
		SCR_FillRect(horizOffset, vertOffset + tabHeight - 1, tabWidth, 1, color);

		// left border
		if (i && currentCon == &consoles[i])
			SCR_FillRect(horizOffset, vertOffset, 1, tabHeight, color);

		// right border
		SCR_FillRect(horizOffset + tabWidth, vertOffset, 1, tabHeight, color);

		if (currentCon == &consoles[i]) {
			SCR_DrawFontText(horizOffset + 15, vertOffset + 14, 0.24f, color, consoleNames[i], ITEM_TEXTSTYLE_SHADOWED);
		}
		else {
			SCR_DrawFontText(horizOffset + 9, vertOffset + 12, 0.18f, color2, consoleNames[i], ITEM_TEXTSTYLE_SHADOWED);
		}

		horizOffset += tabWidth;
	}


	// The little bottom border
	color[3] = 1;
	totalOffset = horizOffset;
	SCR_FillRect(totalOffset, y, SCREEN_WIDTH - totalOffset, 1, color);


	// draw the version number

	re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);

	i = strlen(SVN_VERSION);

	for (x = 0; x < i; x++) {

		SCR_DrawSmallChar(cls.glconfig.vidWidth - (i - x) * (smallchar_width - 1),

			(lines - (smallchar_height + smallchar_height / 2)), SVN_VERSION[x]);

	}


	// draw the text
	currentCon->vislines = lines;
	rows = lines / smallchar_width - 1;	    // rows of text to draw

	y = lines - (smallchar_height * 3);

	// draw from the bottom up
	if (currentCon->display != currentCon->current)
	{
		// draw arrows to show the buffer is backscrolled
		re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
		for (x = 0; x < currentCon->linewidth; x += 4)
			SCR_DrawSmallChar(currentCon->xadjust + (x + 1) * smallchar_width, y, '^');
		y -= smallchar_height;
		rows--;
	}

	row = currentCon->display;

	currentColor = 7;
	re.SetColor(g_color_table[currentColor]);

	for (i = 0; i < rows; i++, y -= smallchar_height, row--)
	{
		if (row < 0)
			break;
		if (currentCon->current - row >= currentCon->totallines) {
			// past scrollback wrap point
			continue;
		}

		text = currentCon->text + (row % currentCon->totallines) * currentCon->linewidth;

		for (x = 0; x < currentCon->linewidth; x++) {
			if ((text[x] & 0xff) == ' ') {
				continue;
			}

			if (((text[x] >> 8) & 7) != currentColor) {
				currentColor = (text[x] >> 8) % 10;
				re.SetColor(g_color_table[currentColor]);
			}
			SCR_DrawSmallChar(currentCon->xadjust + (x + 1) * (smallchar_width - 1), y, text[x] & 0xff);
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();

	re.SetColor(NULL);
}


/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	int i;

	if (com_developer && com_developer->integer)
		numConsoles = 6;
	else
		numConsoles = 5;

	// check for console width changes from a vid mode change
	Con_CheckResize(currentCon);

	// if disconnected, render console full screen
	/*if (cls.state == CA_DISCONNECTED) {
		if (!(cls.keyCatchers & (KEYCATCH_UI | KEYCATCH_CGAME))) {
			Con_DrawSolidConsole(1.0);
			return;
		}
	}*/

	if (currentCon->displayFrac) {
		for (i = 0; i < numConsoles; i++)
			consoles[i].displayFrac = currentCon->displayFrac;

		Con_DrawSolidConsole(currentCon->displayFrac);
	}
	else {
		// draw notify lines
		if (cls.state == CA_ACTIVE) {
			Con_DrawNotify();
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole( void ) 
{
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		currentCon->finalFrac = 0.5;	// half screen
	else
		currentCon->finalFrac = 0.0;	// none visible
	
	// scroll towards the destination height
	if (currentCon->finalFrac < currentCon->displayFrac )
	{
		currentCon->displayFrac -= con_conspeed->value * cls.realFrametime * 0.001;
		if (currentCon->finalFrac > currentCon->displayFrac )
			currentCon->displayFrac = currentCon->finalFrac;

	}
	else if (currentCon->finalFrac > currentCon->displayFrac )
	{
		currentCon->displayFrac += con_conspeed->value * cls.realFrametime * 0.001;
		if (currentCon->finalFrac < currentCon->displayFrac )
			currentCon->displayFrac = currentCon->finalFrac;
	}
}


void Con_PageUp( int lines )
{
	if ( lines == 0 )
		lines = currentCon->vispage - 2;

	currentCon->display -= lines;

	Con_Fixup(currentCon);
}


void Con_PageDown( int lines )
{
	if ( lines == 0 )
		lines = currentCon->vispage - 2;

	currentCon->display += lines;

	Con_Fixup(currentCon);
}


void Con_Top( void )
{
	// this is generally incorrect but will be adjusted in Con_Fixup()
	currentCon->display = currentCon->current - currentCon->totallines;

	Con_Fixup(currentCon);
}


void Con_Bottom( void )
{
	currentCon->display = currentCon->current;

	Con_Fixup(currentCon);
}


void Con_Close( void )
{
	if ( !com_cl_running->integer )
		return;

	Field_Clear( &g_consoleField );
	Con_ClearNotify();
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	currentCon->finalFrac = 0.0;			// none visible
	currentCon->displayFrac = 0.0;
}
