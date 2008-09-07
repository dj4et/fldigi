// ====================================================================
//  logger.cxx Remote Log Interface for fldigi
//
// Copyright W1HKJ, Dave Freese 2006
//
// This library is free software; you can RGBredistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "w1hkj@w1hkj.com".
//
// ====================================================================

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef __CYGWIN__
#  include <sys/ipc.h>
#  include <sys/msg.h>
#endif
#include <errno.h>
#include <string>

#include "logger.h"
#include "main.h"
#include "modem.h"
#include "debug.h"

#include <FL/fl_ask.H>

struct FIELD {
  const char *name;
  unsigned int  size;
};
  
FIELD fields[] = {
//  NAME,  SIZE
  {"ADDRESS",   40},  // 0 - contacted stations mailing address
  {"AGE",        3},  // 1 - contacted operators age in years
  {"ARRL_SECT", 12},  // 2 - contacted stations ARRL section
  {"BAND",       6},  // 3 - QSO band
  {"CALL",      10},  // 4 - contacted stations CALLSIGN
  {"CNTY",      20},  // 5 - secondary political subdivision, ie: STATE
  {"COMMENT",   80},  // 6 - comment field for QSO
  {"CONT",      10},  // 7 - contacted stations continent
  {"CONTEST_ID", 6},  // 8 - QSO contest identifier
  {"COUNTRY",   20},  // 9 - contacted stations DXCC entity name
  {"CQZ",        8},  // 10 - contacted stations CQ Zone
  {"DXCC",       8},  // 11 - contacted stations Country Code
  {"FREQ",      10},  // 12 - QSO frequency in Mhz
  {"GRIDSQUARE", 6},  // 13 - contacted stations Maidenhead Grid Square
  {"MODE",       8},  // 14 - QSO mode
  {"NAME",      18},  // 15 - contacted operators NAME
  {"NOTES",     80},  // 16 - QSO notes
  {"QSLRDATE",   8},  // 17 - QSL received date
  {"QSLSDATE",   8},  // 18 - QSL sent date
  {"QSL_RCVD",   1},  // 19 - QSL received status
  {"QSL_SENT",   1},  // 20 - QSL sent status
  {"QSO_DATE",   8},  // 21 - QSO date
  {"QTH",       30},  // 22 - contacted stations city
  {"RST_RCVD",   3},  // 23 - received signal report
  {"RST_SENT",   3},  // 24 - sent signal report
  {"STATE",      2},  // 25 - contacted stations STATE
  {"STX",        8},  // 26 - QSO transmitted serial number
  {"TIME_OFF",   4},  // 27 - HHMM or HHMMSS in UTC
  {"TIME_ON",    4},  // 28 - HHMM or HHMMSS in UTC
  {"TX_PWR",     4}   // 29 - power transmitted by this station
};

#define ADIF_VERS "2.1.9"
static string adif;

const char *ADIFHEADER = 
"<ADIF_VERS:%d>%s\n\
<PROGRAMID:%d>%s\n\
<PROGRAMVERSION:%d>%s\n\
<EOH>\n\n";


void writeadif () {
// open the adif file
	FILE *adiFile;

// Append to fldigi.adif on all platforms
	string sfname = HomeDir;
	sfname.append("fldigi.adif");
	adiFile = fopen (sfname.c_str(), "a");
	if (adiFile) {
// write the current record to the file  
		adif.append("<EOR>\n");
		fprintf(adiFile,"%s", adif.c_str());
		fclose (adiFile);
	}

// Append to FL_LOGBOOK adif file on Windows if and only if C:\FL_LOGBOOK exists
#ifdef __CYGWIN__
	sfname = "C:/FL_LOGBOOK/log.adif";
	adiFile = fopen (sfname.c_str(), "a");
	if (adiFile) {
// write the current record to the file  
		fprintf(adiFile,"%s", adif.c_str());
		fclose (adiFile);
	}
#endif
}

void putadif(int num, const char *s)
{
        char tempstr[100];
        size_t slen = strlen(s);
        if (slen > fields[num].size) slen = fields[num].size;
        int n = snprintf(tempstr, sizeof(tempstr), "<%s:%zu>", fields[num].name, slen);
        if (n == -1) {
		LOG_PERROR("snprintf");
                return;
        }
        memcpy(tempstr + n, s, slen);
        tempstr[n + slen] = '\0';
        adif.append(tempstr);
}


//---------------------------------------------------------------------

#ifndef __CYGWIN__
static msgtype msgbuf;
#endif
static string log_msg;
static string errmsg;
static char strFreqMhz[20];
char   LOG_MSEPARATOR[2] = {1,0};

//---------------------------------------------------------------------

int submit_log(void)
{
	char logdate[32], logtime[32], adifdate[32];
#ifndef __CYGWIN__
	int msqid, len;
#endif
	struct tm *tm;
	time_t t;

	time(&t);
        tm = gmtime(&t);
		strftime(logdate, sizeof(logdate), "%d %b %Y", tm);
		snprintf(adifdate, sizeof(adifdate), "%04d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
		strftime(logtime, sizeof(logtime), "%H%M", tm);

	const char *mode = mode_info[active_modem->get_mode()].adif_name;
	
	snprintf(strFreqMhz, sizeof(strFreqMhz), "%-10f", wf->dFreq()/1.0e6);

	adif.erase();
	
	FL_LOCK();
	log_msg = "";
	log_msg = log_msg + "program:"	+ PACKAGE_NAME + " v " + PACKAGE_VERSION + LOG_MSEPARATOR;
	log_msg = log_msg + "version:"	+ LOG_MVERSION			+ LOG_MSEPARATOR;
	log_msg = log_msg + "date:"		+ logdate				+ LOG_MSEPARATOR;
	putadif(21, adifdate); 
	log_msg = log_msg + "time:"		+ inpTime->value()		+ LOG_MSEPARATOR;
	putadif(28, inpTime->value());
	log_msg = log_msg + "endtime:"	+ logtime				+ LOG_MSEPARATOR;
	putadif(27, logtime);
	log_msg = log_msg + "call:"		+ inpCall->value()		+ LOG_MSEPARATOR;
	putadif(4, inpCall->value());
	log_msg = log_msg + "mhz:"		+ strFreqMhz			+ LOG_MSEPARATOR;
	putadif(12, strFreqMhz);
	log_msg = log_msg + "mode:"		+ mode					+ LOG_MSEPARATOR;
	putadif(14, mode);
	log_msg = log_msg + "tx:"		+ inpRstOut->value()	+ LOG_MSEPARATOR;
	putadif(24, inpRstOut->value());
	log_msg = log_msg + "rx:"		+ inpRstIn->value() 	+ LOG_MSEPARATOR;
	putadif(23, inpRstIn->value());
	log_msg = log_msg + "name:"		+ inpName->value()		+ LOG_MSEPARATOR;
	putadif(15, inpName->value());
	log_msg = log_msg + "qth:"		+ inpQth->value()		+ LOG_MSEPARATOR;
	putadif(22, inpQth->value());
	log_msg = log_msg + "locator:"	+ inpLoc->value()		+ LOG_MSEPARATOR;
	putadif(13, inpLoc->value());
	log_msg = log_msg + "notes:"	+ inpNotes->value()		+ LOG_MSEPARATOR;
	putadif(16, inpNotes->value());
	FL_UNLOCK();

	writeadif();

#ifndef __CYGWIN__
	strncpy(msgbuf.mtext, log_msg.c_str(), sizeof(msgbuf.mtext));
	msgbuf.mtext[sizeof(msgbuf.mtext) - 1] = '\0';

	if ((msqid = msgget(LOG_MKEY, 0666 | IPC_CREAT)) == -1) {
		errmsg = "msgget: ";
		errmsg.append(strerror(errno));
		LOG_ERROR("%s", errmsg.c_str());
		fl_message(errmsg.c_str());
		return -1;
	}
	msgbuf.mtype = LOG_MTYPE;

// allow for the NUL
	len = strlen(msgbuf.mtext) + 1;
	if (msgsnd(msqid, &msgbuf, len, IPC_NOWAIT) < 0) {
		errmsg = "msgsnd: ";
		fl_message(errmsg.c_str());
		return -1;
	}
	return 0;
#else
	return -1;
#endif
}



//---------------------------------------------------------------------

