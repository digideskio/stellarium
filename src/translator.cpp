/*
* Stellarium
* Copyright (C) 2005 Fabien Chereau
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <config.h>
#include <cassert>
#include <locale>

#include "translator.h"

// Use system locale language by default
Translator Translator::globalTranslator = Translator(PACKAGE, LOCALEDIR, "");
Translator* Translator::lastUsed = NULL;

std::string Translator::translateUTF8(const std::string& s)
{
	reload();
	return gettext(s.c_str());
}

void Translator::reload()
{
	if (Translator::lastUsed == this) return;
	// This needs to be static as it is used a each gettext call... It tooks me quite a while before I got that :(
	static char envstr[25];
	snprintf(envstr, 25, "LANGUAGE=%s", langName.c_str());
	//printf("Setting locale: %s\n", envstr);
	putenv(envstr);
	setlocale(LC_MESSAGES, "");
	
	std::string result = bind_textdomain_codeset(domain.c_str(), "UTF-8");
	assert(result=="UTF-8");
	bindtextdomain (domain.c_str(), moDirectory.c_str());	
	textdomain (domain.c_str());
	Translator::lastUsed = this;
}

/** Convert from ASCII to wchar_t */
// static wchar_t *ASCII_to_UNICODE(wchar_t *unicode, const char *text, int len)
// {
// 	int i;
// 
// 	for ( i=0; i < len; ++i ) {
// 		unicode[i] = ((const unsigned char *)text)[i];
// 	}
// 	unicode[i] = 0;
// 
// 	return unicode;
// }

/** Convert from char* UTF-8 to wchar_t UCS4 - stolen from SDL_ttf library */
static wchar_t *UTF8_to_UNICODE(wchar_t *unicode, const char *utf8, int len)
{
	int i, j;
	wchar_t ch;

	for ( i=0, j=0; i < len; ++i, ++j ) {
		ch = ((const unsigned char *)utf8)[i];
		if ( ch >= 0xF0 ) {
			ch  =  (wchar_t)(utf8[i]&0x07) << 18;
			ch |=  (wchar_t)(utf8[++i]&0x3F) << 12;
			ch |=  (wchar_t)(utf8[++i]&0x3F) << 6;
			ch |=  (wchar_t)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xE0 ) {
			ch  =  (wchar_t)(utf8[i]&0x3F) << 12;
			ch |=  (wchar_t)(utf8[++i]&0x3F) << 6;
			ch |=  (wchar_t)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xC0 ) {
			ch  =  (wchar_t)(utf8[i]&0x3F) << 6;
			ch |=  (wchar_t)(utf8[++i]&0x3F);
		}
		unicode[j] = ch;
	}
	unicode[j] = 0;

	return unicode;
}

/** 
 * Convert from UTF-8 to wchar_t 
 * Warning this is likely to be not very portable
 */
std::wstring Translator::UTF8stringToWstring(const string& s)
{
	wchar_t* outbuf = new wchar_t[s.length()+1];
	UTF8_to_UNICODE(outbuf, s.c_str(), s.length());
	wstring ws(outbuf);
	delete[] outbuf;
	return ws;
}


//! @brief Create a locale matching with a locale name
// std::locale Translator::tryLocale(const string& localeName)
// {
// 	std::locale loc;
// 	if (localeName=="system_default" || localeName=="system")
// 	{
// 		return std::locale("");
// 	}
// 	else
// 	{
// 		try
// 		{
// 			loc = std::locale(localeName.c_str());
// 		}
// 		catch (const std::exception& e)
// 		{
// 			cout << e.what() << " \"" << localeName << "\" : revert to default locale \"" << std::locale("").name() << "\"" << endl;
// 			// Fallback with current locale
// 			loc = std::locale("");
// 		}
// 	}
// 	return loc;
// }
