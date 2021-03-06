/*
Copyright (c) 2015 Miranda NG project (http://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SKYPE_REQUEST_LOGIN_H_
#define _SKYPE_REQUEST_LOGIN_H_

class LoginRequest : public HttpRequest
{
public:
	LoginRequest() :
		HttpRequest(REQUEST_POST, "login.skype.com/login")
	{
		Url
			<< INT_VALUE("client_id", 578134)
			<< CHAR_VALUE("redirect_uri", "https%3A%2F%2Fweb.skype.com");
	}

	LoginRequest(const char *skypename, const char *password, const char *pie, const char *etm) :
		HttpRequest(REQUEST_POST, "login.skype.com/login")
	{
		Url
			<< INT_VALUE("client_id", 578134)
			<< CHAR_VALUE("redirect_uri", "https%3A%2F%2Fweb.skype.com");

		Headers
			<< CHAR_VALUE("Referer", "https://login.skype.com/login?method=skype&client_id=578134&redirect_uri=https%3A%2F%2Fweb.skype.com");

		LPTIME_ZONE_INFORMATION tzi = tmi.getTziByContact(NULL);
		char sign = tzi->Bias > 0 ? '-' : '+';
		int hours = tzi->Bias / -60;
		int minutes = tzi->Bias % -60;

		Body
			<< CHAR_VALUE("username", skypename)
			<< CHAR_VALUE("password", password)
			<< CHAR_VALUE("pie", ptrA(mir_urlEncode(pie)))
			<< CHAR_VALUE("etm", ptrA(mir_urlEncode(etm)))
			<< FORMAT_VALUE("timezone_field", "%c%02d|%02d", sign, hours, minutes)
			<< FORMAT_VALUE("js_time", "%d.00", time(NULL))
			<< INT_VALUE("client_id", 578134)
			<< CHAR_VALUE("redirect_uri", "https%3A%2F%2Fweb.skype.com");
	}
};

#endif //_SKYPE_REQUEST_LOGIN_H_
